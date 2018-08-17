// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/user_manager/user_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Handles removal of pre-installed demo mode resources.
// Observes system state to detect when pre-installed demo mode resources are
// not needed anymore, and schedules their deletion from the disk.
//
// Only single instance is expected to be created per process.
//
// Demo mode resources are deleted if the device is not in demo mode, and any
// of the following conditions are satisfied:
//   * device is running low on disk space
//   * device is enrolled in a non-demo-mode domain
//   * TODO(crbug.com/827368): enough non-demo-mode user activity has been
//     detected on the device
class DemoModeResourcesRemover
    : public CryptohomeClient::Observer,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // The reason a removal was requested.
  // DO NOT REORDER - used to report metrics.
  enum class RemovalReason {
    kLowDiskSpace = 0,
    kEnterpriseEnrolled = 1,
    kRegularUsage = 2,
    kMaxValue = kRegularUsage
  };

  // The result of a removal attempt.
  // DO NOT REORDER - used to report metrics.
  enum class RemovalResult {
    // Pre-installed resources were removed.
    kSuccess = 0,

    // Pre-installed resources were not found.
    kNotFound = 1,

    // Pre-installed resources cannot be removed in this session.
    // Not expected to be reported in UMA.
    kNotAllowed = 2,

    // The resources have been previously removed.
    // Not expected to be reported in UMA.
    kAlreadyRemoved = 3,

    // Attempt to remove pre-installed resources failed.
    kFailed = 4,

    kMaxValue = kFailed
  };

  // Callback for a request to remove demo mode resources from the stateful
  // partition.
  using RemovalCallback = base::OnceCallback<void(RemovalResult result)>;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Gets the demo mode resources remover instance for this process - at most
  // one instance can be created per process.
  static DemoModeResourcesRemover* Get();

  // Creates a demo mode resources remover instance if it's required.
  // It will return nullptr if a DemoModeResourcesRemover is not needed at the
  // time - for example if the resources have previously been removed, or if the
  // device is in demo mode.
  //
  // It should be called at most once per process - creating more than one
  // DemoModeResourcesRemover instances will hit a CHECK.
  static std::unique_ptr<DemoModeResourcesRemover> CreateIfNeeded(
      PrefService* local_state);

  // Method used to determine whether a domain is associated with legacy demo
  // retail mode, where demo mode sessions are implemented as public sessions.
  // Exposed so the matching can be tested.
  // TODO(crbug.com/874778): Remove after legacy retail mode domains have been
  // disabled.
  static bool IsLegacyDemoRetailModeDomain(const std::string& domain);

  ~DemoModeResourcesRemover() override;

  // CryptohomeClient::Observer:
  void LowDiskSpace(uint64_t free_disk_space) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(const user_manager::User* user) override;

  // Requests demo mode resources removal from the disk. If a removal operation
  // is already in progress, this method will schedule the callback to be run
  // with the result of the operation in progress.
  void AttemptRemoval(RemovalReason reason, RemovalCallback callback);

 private:
  // Use CreateIfNeeded() to create an instance.
  explicit DemoModeResourcesRemover(PrefService* local_state);

  // Passes as the callback to directory removal file operations.
  void OnRemovalDone(RemovalResult result);

  PrefService* const local_state_;

  // Whether a resources removal operation is currently in progress.
  bool removal_in_progress_ = false;

  // Callbacks for the resources removal operation, if one is in progress.
  std::vector<RemovalCallback> removal_callbacks_;

  ScopedObserver<CryptohomeClient, DemoModeResourcesRemover>
      cryptohome_observer_;

  base::WeakPtrFactory<DemoModeResourcesRemover> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemover);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_MODE_RESOURCES_REMOVER_H_
