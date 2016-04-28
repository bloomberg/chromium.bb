// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
#define COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/account_id/account_id.h"

namespace arc {

class ArcBridgeService;
class ArcService;

// Manages creation and destruction of services that communicate with the ARC
// instance via the ArcBridgeService.
class ArcServiceManager {
 public:
  ArcServiceManager();
  virtual ~ArcServiceManager();

  // |arc_bridge_service| can only be accessed on the thread that this
  // class was created on.
  ArcBridgeService* arc_bridge_service();

  // Adds a service to the managed services list.
  void AddService(std::unique_ptr<ArcService> service);

  // Gets the global instance of the ARC Service Manager. This can only be
  // called on the thread that this class was created on.
  static ArcServiceManager* Get();

  // Called when the main profile is initialized after user logs in.
  void OnPrimaryUserProfilePrepared(const AccountId& account_id);

  // Called once the windowing system (ash) has been started.
  void OnAshStarted();

  // Called to shut down all ARC services.
  void Shutdown();

  // Set ArcBridgeService instance for testing. Call before ArcServiceManager
  // creation. ArcServiceManager owns |arc_bridge_service|.
  static void SetArcBridgeServiceForTesting(
      std::unique_ptr<ArcBridgeService> arc_bridge_service);

 private:
  base::ThreadChecker thread_checker_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::vector<std::unique_ptr<ArcService>> services_;

  // True once the window manager service got added, barring adding any more
  // of those since OnAshStarted() might be called multiple times.
  bool on_ash_started_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
