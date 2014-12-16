// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// PeerDaemonManagerClient is used to communicate with the PeerDaemon Manager
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT PeerDaemonManagerClient : public DBusClient {
 public:
  ~PeerDaemonManagerClient() override;

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static PeerDaemonManagerClient* Create();

  // Calls StartMonitoring method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StartMonitoring(
      const std::vector<std::string>& requested_technologies,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) = 0;

  // Calls StopMonitoring method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void StopMonitoring(const std::string& monitoring_token,
                              const VoidDBusMethodCallback& callback) = 0;

  // Calls ExposeService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void ExposeService(
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) = 0;

  // Calls RemoveExposedService method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void RemoveExposedService(const std::string& service_token,
                                    const VoidDBusMethodCallback& callback) = 0;

  // Calls Ping method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void Ping(const StringDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  PeerDaemonManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PeerDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PEER_DAEMON_MANAGER_CLIENT_H_
