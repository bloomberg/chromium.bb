// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_PERMISSION_BROKER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// PermissionBrokerClient is used to communicate with the permission broker, a
// process that allows requesting permission to access specific device nodes.
// For example, one place that this client is used is within the USB extension
// API code, where it is used to request explicit access to USB peripherals
// which the user the browser runs under normally wouldn't have access to. For
// more details on the permission broker see:
// http://git.chromium.org/gitweb/?p=chromiumos/platform/permission_broker.git
class CHROMEOS_EXPORT PermissionBrokerClient : public DBusClient {
 public:
  // The ResultCallback is used for both the RequestPathAccess and
  // RequestUsbAccess methods. Its boolean parameter represents the result of
  // the operation that it was submitted alongside.
  typedef base::Callback<void(bool)> ResultCallback;

  virtual ~PermissionBrokerClient();

  static PermissionBrokerClient* Create();

  // RequestPathAccess requests access to a single device node identified by
  // |path|. If |interface_id| value is passed (different than
  // UsbDevicePermissionData::ANY_INTERFACE), the request will check if a
  // specific interface is claimed while requesting access.
  // This allows devices with multiple interfaces to be accessed even if
  // some of them are already claimed by kernel.
  virtual void RequestPathAccess(const std::string& path,
                                 int interface_id,
                                 const ResultCallback& callback) = 0;

  // RequestUsbAccess attempts to request access to _all_ USB devices attached
  // to the system that match |vendor_id| and |product_id|. If |interface_id| is
  // passed (not -1), the request will check if a specific interface is claimed
  // while requesting access. This allows devices with multiple interfaces to be
  // accessed even if some of them are already claimed by kernel.
  // This call makes no attempt to guarantee atomicity, and partial failure is
  // indistinguishable from complete failure.
  virtual void RequestUsbAccess(uint16_t vendor_id,
                                uint16_t product_id,
                                int interface_id,
                                const ResultCallback& callback) = 0;

 protected:
  PermissionBrokerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PERMISSION_BROKER_CLIENT_H_
