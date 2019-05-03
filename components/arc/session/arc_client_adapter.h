// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
#define COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace arc {

// TODO(yusukes): Move the enum and proto in system_api/ from login_manager's
// namespace to arc and remove all the type aliases.
using ArcContainerStopReason = login_manager::ArcContainerStopReason;
using StartArcMiniContainerRequest =
    login_manager::StartArcMiniContainerRequest;
using UpgradeArcContainerRequest = login_manager::UpgradeArcContainerRequest;

// An adapter to talk to a Chrome OS daemon to manage lifetime of ARC instance.
class ArcClientAdapter {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void ArcInstanceStopped(ArcContainerStopReason stop_reason) = 0;
  };

  // Creates a default instance of ArcClientAdapter.
  static std::unique_ptr<ArcClientAdapter> Create();
  virtual ~ArcClientAdapter();

  // StartMiniArc starts ARC with only a handful of ARC processes for Chrome OS
  // login screen.
  virtual void StartMiniArc(const StartArcMiniContainerRequest& request,
                            chromeos::VoidDBusMethodCallback callback) = 0;

  // UpgradeArc upgrades a mini ARC instance to a full ARC instance. In case of
  // success, success_callback is called. In case of error, |error_callback|
  // will be called with a |low_free_disk_space| signaling whether the failure
  // was due to low free disk space.
  using UpgradeErrorCallback =
      base::OnceCallback<void(bool low_free_disk_space)>;
  virtual void UpgradeArc(const UpgradeArcContainerRequest& request,
                          base::OnceClosure success_callback,
                          UpgradeErrorCallback error_callback) = 0;

  // Asynchronously stops the ARC instance.
  virtual void StopArcInstance() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcClientAdapter();

  base::ObserverList<Observer>::Unchecked observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcClientAdapter);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_CLIENT_ADAPTER_H_
