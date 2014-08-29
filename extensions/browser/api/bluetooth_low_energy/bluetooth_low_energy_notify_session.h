// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_NOTIFY_SESSION_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_NOTIFY_SESSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace extensions {

// An ApiResource wrapper for a device::BluetoothGattNotifySession
class BluetoothLowEnergyNotifySession : public ApiResource {
 public:
  explicit BluetoothLowEnergyNotifySession(
      bool persistent,
      const std::string& owner_extension_id,
      scoped_ptr<device::BluetoothGattNotifySession> session);
  virtual ~BluetoothLowEnergyNotifySession();

  // Returns a pointer to the underlying session object.
  device::BluetoothGattNotifySession* GetSession() const;

  // ApiResource override.
  virtual bool IsPersistent() const OVERRIDE;

  // This resource should be managed on the UI thread.
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<BluetoothLowEnergyNotifySession>;
  static const char* service_name() {
    return "BluetoothLowEnergyNotifySessionManager";
  }

  // True, if this resource should be persistent across suspends.
  bool persistent_;

  // The session is owned by this instance and will automatically stop when
  // deleted.
  scoped_ptr<device::BluetoothGattNotifySession> session_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyNotifySession);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_NOTIFY_SESSION_H_
