// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
#define DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) BleAdapterManager
    : public BluetoothAdapter::Observer {
 public:
  // Handles notifying |request_handler| when BluetoothAdapter is powered on and
  // off. Exposes API for |request_handler| to power on BluetoothAdapter
  // programmatically, and if BluetoothAdapter was powered on programmatically,
  // powers off BluetoothAdapter when |this| goes out of scope.
  // |request_handler| must outlive |this|.
  BleAdapterManager(FidoRequestHandlerBase* request_handler);
  ~BleAdapterManager() override;

  void SetAdapterPower(bool set_power_on);

 private:
  friend class FidoBleAdapterManagerTest;

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

  void Start(scoped_refptr<BluetoothAdapter> adapter);

  FidoRequestHandlerBase* const request_handler_;
  scoped_refptr<BluetoothAdapter> adapter_;
  bool adapter_powered_on_programmatically_ = false;

  base::WeakPtrFactory<BleAdapterManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleAdapterManager);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
