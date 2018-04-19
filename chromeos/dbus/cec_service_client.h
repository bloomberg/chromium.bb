// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// CecServiceClient is used to communicate with org.chromium.CecService.
//
// CecService offers a small subset of HDMI CEC capabilities focused on power
// management of connected displays.
//
// All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT CecServiceClient : public DBusClient {
 public:
  ~CecServiceClient() override;

  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<CecServiceClient> Create(
      DBusClientImplementationType type);

  // Puts all connected HDMI CEC capable displays into stand-by mode. The effect
  // of calling this method is on a best effort basis, no guarantees of displays
  // going into stand-by is made.
  virtual void SendStandBy() = 0;

  // Wakes up all connected HDMI CEC capable displays from stand-by mode. The
  // effect of calling this method is on a best effort basis, no guarantees of
  // displays going into stand-by is made.
  virtual void SendWakeUp() = 0;

 protected:
  CecServiceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CecServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CEC_SERVICE_CLIENT_H_
