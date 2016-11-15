// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UPSTART_CLIENT_H_
#define CHROMEOS_DBUS_UPSTART_CLIENT_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// UpstartClient is used to communicate with the com.ubuntu.Upstart
// sevice. All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT UpstartClient : public DBusClient {
 public:
  ~UpstartClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static UpstartClient* Create();

  // Starts authpolicyd.
  virtual void StartAuthPolicyService() = 0;

 protected:
  // Create() should be used instead.
  UpstartClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UpstartClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UPSTART_CLIENT_H_
