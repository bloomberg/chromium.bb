// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_DISCOVERY_H_
#define DEVICE_U2F_U2F_BLE_DISCOVERY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/u2f/u2f_discovery.h"

#include <memory>

namespace device {

class BluetoothDevice;
class BluetoothDiscoverySession;
class BluetoothUUID;

class U2fBleDiscovery : public U2fDiscovery, BluetoothAdapter::Observer {
 public:
  U2fBleDiscovery();
  ~U2fBleDiscovery() override;

  // U2fDiscovery:
  void Start() override;
  void Stop() override;

 private:
  static const BluetoothUUID& U2fServiceUUID();

  void GetAdapterCallback(scoped_refptr<BluetoothAdapter> adapter);
  void OnSetPowered();
  void DiscoverySessionStarted(std::unique_ptr<BluetoothDiscoverySession>);

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;

  void OnStopped(bool success);

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;

  base::WeakPtrFactory<U2fBleDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fBleDiscovery);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_DISCOVERY_H_
