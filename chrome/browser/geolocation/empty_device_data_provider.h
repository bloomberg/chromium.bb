// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GEARS_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H__
#define GEARS_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H__

#include "gears/geolocation/device_data_provider.h"

// An implementation of DeviceDataProviderImplBase that does not provide any
// data. Used on platforms where a given data type is not available.

template<typename DataType>
class EmptyDeviceDataProvider : public DeviceDataProviderImplBase<DataType> {
 public:
  EmptyDeviceDataProvider() {}
  virtual ~EmptyDeviceDataProvider() {}

  // DeviceDataProviderImplBase implementation
  virtual bool GetData(DataType *data) {
    assert(data);
    // This is all the data we can get - nothing.
    return true;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(EmptyDeviceDataProvider);
};

#endif  // GEARS_GEOLOCATION_EMPTY_DEVICE_DATA_PROVIDER_H__
