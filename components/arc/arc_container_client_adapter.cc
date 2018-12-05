// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_container_client_adapter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager_client.h"

namespace arc {

namespace {

chromeos::SessionManagerClient* GetSessionManagerClient() {
  // If the DBusThreadManager or the SessionManagerClient aren't available,
  // there isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetSessionManagerClient()) {
    return nullptr;
  }
  return chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
}

}  // namespace

class ArcContainerClientAdapter
    : public ArcClientAdapter,
      public chromeos::SessionManagerClient::Observer {
 public:
  ArcContainerClientAdapter() {
    chromeos::SessionManagerClient* client = GetSessionManagerClient();
    if (client)
      client->AddObserver(this);
  }

  ~ArcContainerClientAdapter() override {
    chromeos::SessionManagerClient* client = GetSessionManagerClient();
    if (client)
      client->RemoveObserver(this);
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    StartMiniArcCallback callback) override {
    GetSessionManagerClient()->StartArcMiniContainer(request,
                                                     std::move(callback));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  base::OnceClosure success_callback,
                  UpgradeErrorCallback error_callback) override {
    GetSessionManagerClient()->UpgradeArcContainer(
        request, std::move(success_callback), std::move(error_callback));
  }

  void StopArcInstance() override {
    // Since we have the ArcInstanceStopped() callback, we don't need to do
    // anything when StopArcInstance completes.
    GetSessionManagerClient()->StopArcInstance(
        chromeos::EmptyVoidDBusMethodCallback());
  }

  // chromeos::SessionManagerClient::Observer overrides:
  void ArcInstanceStopped(login_manager::ArcContainerStopReason stop_reason,
                          const std::string& container_instance_id) override {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(stop_reason, container_instance_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContainerClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter() {
  return std::make_unique<ArcContainerClientAdapter>();
}

}  // namespace arc
