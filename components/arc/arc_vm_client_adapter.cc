// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_vm_client_adapter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/session_manager_client.h"

namespace arc {

namespace {

// TODO(yusukes): Move ArcContainerStopReason to arc:: and remove the #include.
constexpr login_manager::ArcContainerStopReason kDummyReason =
    login_manager::ArcContainerStopReason::SESSION_MANAGER_SHUTDOWN;
constexpr char kDummyInstanceId[] = "dummyinstanceid";

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
    // TODO(yusukes): Start arcvm.
    base::PostTask(
        FROM_HERE,
        base::BindOnce(&ArcVmClientAdapter::OnArcInstanceUpgradeFailed,
                       weak_factory_.GetWeakPtr(), std::move(error_callback)));
  }

  void StopArcInstance() override {
    // TODO(yusukes): Stop arcvm.
    base::PostTask(FROM_HERE,
                   base::BindOnce(&ArcVmClientAdapter::OnArcInstanceStopped,
                                  weak_factory_.GetWeakPtr()));
  }

 private:
  void OnArcMiniInstanceStarted(StartMiniArcCallback callback) {
    std::move(callback).Run(kDummyInstanceId);
  }

  void OnArcInstanceUpgradeFailed(UpgradeErrorCallback callback) {
    std::move(callback).Run(/*low_free_disk_space=*/false);
  }

  void OnArcInstanceStopped() {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(kDummyReason, kDummyInstanceId);
  }

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

}  // namespace arc
