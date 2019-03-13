// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_vm_client_adapter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"

namespace arc {

namespace {

// TODO(yusukes): Move ArcContainerStopReason to arc:: and stop including
// chromeos/dbus/session_manager_client.h.
constexpr login_manager::ArcContainerStopReason kDummyReason =
    login_manager::ArcContainerStopReason::SESSION_MANAGER_SHUTDOWN;

// The conversion of upstart job names to dbus object paths is undocumented. See
// arc_data_remover.cc for more information.
constexpr char kArcVmUpstartJob[] = "arcvm";

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter {
 public:
  ArcVmClientAdapter() : weak_factory_(this) {}
  ~ArcVmClientAdapter() override = default;

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    StartMiniArcCallback callback) override {
    // TODO(yusukes): Support mini ARC.
    VLOG(2) << "Mini ARC instance is not supported yet.";
    base::PostTask(
        FROM_HERE,
        base::BindOnce(&ArcVmClientAdapter::OnArcMiniInstanceStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  base::OnceClosure success_callback,
                  UpgradeErrorCallback error_callback) override {
    // TODO(yusukes): Consider doing the same as crostini rather than taking to
    // Upstart.
    VLOG(1) << "Starting arcvm";
    auto* upstart_client = chromeos::UpstartClient::Get();
    DCHECK(upstart_client);
    upstart_client->StartJob(
        kArcVmUpstartJob,
        // arc_session_impl.cc fills the |account_id| field, and it is always
        // guaranteed that the ID is not for Incognito mode and is a valid one.
        // TODO(yusukes): Pass other fields of the |request| to the job.
        {"CHROMEOS_USER=" + request.account_id()},
        base::BindOnce(&ArcVmClientAdapter::OnArcInstanceUpgraded,
                       weak_factory_.GetWeakPtr(), std::move(success_callback),
                       std::move(error_callback)));
  }

  void StopArcInstance() override {
    // TODO(yusukes): Consider doing the same as crostini rather than taking to
    // Upstart.
    VLOG(1) << "Stopping arcvm";
    auto* upstart_client = chromeos::UpstartClient::Get();
    DCHECK(upstart_client);
    upstart_client->StopJob(
        kArcVmUpstartJob,
        base::BindOnce(&ArcVmClientAdapter::OnArcInstanceStopped,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnArcMiniInstanceStarted(StartMiniArcCallback callback) {
    current_instance_id_ = base::GenerateGUID();
    std::move(callback).Run(current_instance_id_);
  }

  void OnArcInstanceUpgraded(base::OnceClosure success_callback,
                             UpgradeErrorCallback error_callback,
                             bool result) {
    VLOG(1) << "OnArcInstanceUpgraded result=" << result;
    if (result)
      std::move(success_callback).Run();
    else
      std::move(error_callback).Run(/*low_free_disk_space=*/false);
  }

  void OnArcInstanceStopped(bool result) {
    VLOG(1) << "OnArcInstanceStopped result=" << result;
    if (!result)
      LOG(WARNING) << "Failed to stop arcvm. Instance not running?";
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(kDummyReason, current_instance_id_);
    if (result)
      current_instance_id_.clear();
  }

  // A unique ID associated with the current Upstart job.
  std::string current_instance_id_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

}  // namespace arc
