#include "AC_CASS_HYT271.h"
#include <utility>
#include <stdio.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_Math/AP_Math.h>

extern const AP_HAL::HAL &hal;

AC_CASS_HYT271::AC_CASS_HYT271() :
    _dev(nullptr),
    _temperature(0),
    _humidity(0),
    _healthy(false)
{
}

bool AC_CASS_HYT271::init(uint8_t busId, uint8_t i2cAddr)
{

    sem = hal.util->new_semaphore();

    // Bus 0 is for Pixhawk 2.1 I2C and Bus 1 is for Pixhawk 1 and PixRacer I2C
    // Check if device exists
    _dev = std::move(hal.i2c_mgr->get_device(busId, i2cAddr));
    if (!_dev) {
        return false;
    }

    if (!_dev->get_semaphore()->take(0)) {}

    _dev->set_retries(10);

    // Start the first measurement
    if (!_measure()) {
        _dev->get_semaphore()->give();
        return false;
    }

    // lower retries for run
    _dev->set_retries(2);

    _dev->get_semaphore()->give();

    // Register sensor to the I2C manager
    _dev->register_periodic_callback(100000,
                                     FUNCTOR_BIND_MEMBER(&AC_CASS_HYT271::_timer, void));
    return true;
}

void AC_CASS_HYT271::set_i2c_addr(uint8_t addr){
    if (_dev) {
        _dev->set_address(addr);
    }
}

bool AC_CASS_HYT271::_measure()
{
    // Send start measuring command
    uint8_t cmd = 0x00;
    if (!_dev->transfer(&cmd, 1, nullptr, 0)) {
        return false;
    }
    return true;
}

bool AC_CASS_HYT271::_collect(float &hum, float &temp)
{
    uint8_t data[4];
    int16_t raw;
    
    // Fetch data
    if (!_dev->transfer(nullptr, 0, data, sizeof(data))) {
        return false;
    }

    // Verify data with the checksum
    if ((data[0] & 0x40) == 0x40){
        return false;
    }

    // Bit shift and convert to floating point number
    raw = (data[0] << 8) | data[1];
    raw = raw & 0x3FFF;

    if(sem->take(HAL_SEMAPHORE_BLOCK_FOREVER)){

        // Calculate relative humidity percentage
        hum = (100.0 / (powf(2,14) - 1)) * (float)raw;

        // Calculate temperature (in Kelvin)
        data[3] = (data[3] >> 2);
        raw = (data[2] << 6) | data[3];
        temp = (165.0 / (powf(2,14) - 1)) * (float)raw + 233.15f;

        sem->give(); 
    }

    return true; 
}

void AC_CASS_HYT271::_timer(void)
{
    // Loop sequence
    _healthy = _collect(_humidity, _temperature);   // Retreive data from the sensor
    _measure();                                     // Request a new measurement to the sensor
}