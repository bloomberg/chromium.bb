// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/dbus/leadership_daemon_manager_client.h"

namespace chromeos {

// A fake implementation of LeadershipDaemonManagerClient. Invokes callbacks
// immediately.
class FakeLeadershipDaemonManagerClient : public LeadershipDaemonManagerClient {
 public:
  FakeLeadershipDaemonManagerClient();
  ~FakeLeadershipDaemonManagerClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // LeadershipDaemonManagerClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void JoinGroup(const std::string& group,
                 const base::DictionaryValue& options,
                 const ObjectPathDBusMethodCallback& callback) override;
  void LeaveGroup(const std::string& object_path,
                  const VoidDBusMethodCallback& callback) override;
  void SetScore(const std::string& object_path,
                int score,
                const VoidDBusMethodCallback& callback) override;
  void PokeLeader(const std::string& object_path,
                  const VoidDBusMethodCallback& callback) override;
  void Ping(const StringDBusMethodCallback& callback) override;
  const GroupProperties* GetGroupProperties(
      const dbus::ObjectPath& object_path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLeadershipDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_
