// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
namespace api {

class BluetoothLowEnergyGetServiceFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getService",
                             BLUETOOTHLOWENERGY_GETSERVICE);

 protected:
  virtual ~BluetoothLowEnergyGetServiceFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetServicesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getServices",
                             BLUETOOTHLOWENERGY_GETSERVICES);

 protected:
  virtual ~BluetoothLowEnergyGetServicesFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetCharacteristicFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristic",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTIC);

 protected:
  virtual ~BluetoothLowEnergyGetCharacteristicFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetCharacteristicsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristics",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTICS);

 protected:
  virtual ~BluetoothLowEnergyGetCharacteristicsFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetIncludedServicesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getIncludedServices",
                             BLUETOOTHLOWENERGY_GETINCLUDEDSERVICES);

 protected:
  virtual ~BluetoothLowEnergyGetIncludedServicesFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetDescriptorFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptor",
                             BLUETOOTHLOWENERGY_GETDESCRIPTOR);

 protected:
  virtual ~BluetoothLowEnergyGetDescriptorFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyGetDescriptorsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptors",
                             BLUETOOTHLOWENERGY_GETDESCRIPTORS);

 protected:
  virtual ~BluetoothLowEnergyGetDescriptorsFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyReadCharacteristicValueFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readCharacteristicValue",
                             BLUETOOTHLOWENERGY_READCHARACTERISTICVALUE);

 protected:
  virtual ~BluetoothLowEnergyReadCharacteristicValueFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyWriteCharacteristicValueFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeCharacteristicValue",
                             BLUETOOTHLOWENERGY_WRITECHARACTERISTICVALUE);

 protected:
  virtual ~BluetoothLowEnergyWriteCharacteristicValueFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyReadDescriptorValueFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readDescriptorValue",
                             BLUETOOTHLOWENERGY_READDESCRIPTORVALUE);

 protected:
  virtual ~BluetoothLowEnergyReadDescriptorValueFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothLowEnergyWriteDescriptorValueFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeDescriptorValue",
                             BLUETOOTHLOWENERGY_WRITEDESCRIPTORVALUE);

 protected:
  virtual ~BluetoothLowEnergyWriteDescriptorValueFunction() {}

  // UIThreadExtensionFunction override.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
