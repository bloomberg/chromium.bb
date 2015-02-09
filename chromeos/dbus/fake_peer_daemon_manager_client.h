// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_PEER_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_PEER_DAEMON_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/dbus/peer_daemon_manager_client.h"

namespace chromeos {

// A fake implementation of PeerDaemonManagerClient. Invokes callbacks
// immediately.
class FakePeerDaemonManagerClient : public PeerDaemonManagerClient {
 public:
  FakePeerDaemonManagerClient();
  ~FakePeerDaemonManagerClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PeerDaemonManagerClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetPeers() override;
  std::vector<dbus::ObjectPath> GetServices() override;
  PeerProperties* GetPeerProperties(
      const dbus::ObjectPath& object_path) override;
  ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) override;
  void StartMonitoring(const std::vector<std::string>& requested_technologies,
                       const base::DictionaryValue& options,
                       const StringDBusMethodCallback& callback) override;
  void StopMonitoring(const std::string& monitoring_token,
                      const VoidDBusMethodCallback& callback) override;
  void ExposeService(const std::string& service_id,
                     const std::map<std::string, std::string>& service_info,
                     const base::DictionaryValue& options,
                     const StringDBusMethodCallback& callback) override;
  void RemoveExposedService(const std::string& service_token,
                            const VoidDBusMethodCallback& callback) override;
  void Ping(const StringDBusMethodCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePeerDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_PEER_DAEMON_MANAGER_CLIENT_H_
