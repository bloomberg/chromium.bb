// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_BLE_DISCOVERY_H_
#define DEVICE_FIDO_FIDO_BLE_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/fido_discovery.h"

namespace device {

class BluetoothDevice;
class BluetoothDiscoverySession;
class BluetoothUUID;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleDiscovery
    : public FidoDiscovery,
      BluetoothAdapter::Observer {
 public:
  FidoBleDiscovery();
  ~FidoBleDiscovery() override;

  // FidoDiscovery:
  U2fTransportProtocol GetTransportProtocol() const override;
  void Start() override;
  void Stop() override;

 private:
  static const BluetoothUUID& FidoServiceUUID();

  void OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter);
  void OnSetPowered();
  void OnSetPoweredError();
  void OnStartDiscoverySessionWithFilter(
      std::unique_ptr<BluetoothDiscoverySession>);
  void OnStartDiscoverySessionWithFilterError();

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void OnStopped(bool success);

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;

  base::WeakPtrFactory<FidoBleDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoBleDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_BLE_DISCOVERY_H_
