// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SENESCHAL_CLIENT_H_
#define CHROMEOS_DBUS_SENESCHAL_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"

namespace chromeos {

// SeneschalClient is used to communicate with Seneschal, which manages
// 9p file servers.
class COMPONENT_EXPORT(CHROMEOS_DBUS) SeneschalClient : public DBusClient {
 public:
  ~SeneschalClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<SeneschalClient> Create();

  // Shares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void SharePath(
      const vm_tools::seneschal::SharePathRequest& request,
      DBusMethodCallback<vm_tools::seneschal::SharePathResponse> callback) = 0;

  // Unshares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void UnsharePath(
      const vm_tools::seneschal::UnsharePathRequest& request,
      DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
          callback) = 0;

 protected:
  // Create() should be used instead.
  SeneschalClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SeneschalClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SENESCHAL_CLIENT_H_
