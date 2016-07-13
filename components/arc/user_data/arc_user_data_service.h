// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE
#define COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/account_id/account_id.h"

namespace arc {

class ArcBridgeService;

// This class controls the lifecycle of ARC user data, removing it when
// necessary.
class ArcUserDataService : public ArcService,
                           public ArcBridgeService::Observer {
 public:
  explicit ArcUserDataService(
      ArcBridgeService* arc_bridge_service,
      std::unique_ptr<BooleanPrefMember> arc_enabled_pref,
      const AccountId& account_id);
  ~ArcUserDataService() override;

  // ArcBridgeService::Observer:
  // Called whenever the arc bridge is stopped to potentially remove data if
  // the user has not opted in.
  void OnBridgeStopped(ArcBridgeService::StopReason reason) override;

 private:
  base::ThreadChecker thread_checker_;

  // Checks if ARC is both stopped and disabled (not opt-in) and triggers
  // removal of user data if both conditions are true.
  void ClearIfDisabled();

  const std::unique_ptr<BooleanPrefMember> arc_enabled_pref_;

  // Account ID for the account for which we currently have opt-in information.
  AccountId primary_user_account_id_;

  DISALLOW_COPY_AND_ASSIGN(ArcUserDataService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_USER_DATA_ARC_USER_DATA_SERVICE
