// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
#define COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/account_id/account_id.h"

namespace arc {

class ArcAuthService;
class ArcBridgeService;
class ArcClipboardBridge;
class ArcImeBridge;
class ArcInputBridge;
class ArcIntentHelperBridge;
class ArcNotificationManager;
class ArcPowerBridge;
class ArcSettingsBridge;
class ArcVideoBridge;

// Manages creation and destruction of services that communicate with the ARC
// instance via the ArcBridgeService.
class ArcServiceManager {
 public:
  ArcServiceManager(scoped_ptr<ArcAuthService> auth_service,
                    scoped_ptr<ArcIntentHelperBridge> intent_helper_bridge,
                    scoped_ptr<ArcSettingsBridge> settings_bridge,
                    scoped_ptr<ArcVideoBridge> video_bridge);
  virtual ~ArcServiceManager();

  // |arc_bridge_service| can only be accessed on the thread that this
  // class was created on.
  ArcBridgeService* arc_bridge_service();

  // Gets the global instance of the ARC Service Manager. This can only be
  // called on the thread that this class was created on.
  static ArcServiceManager* Get();

  // Called when the main profile is initialized after user logs in.
  void OnPrimaryUserProfilePrepared(const AccountId& account_id);

 private:
  base::ThreadChecker thread_checker_;
  scoped_ptr<ArcBridgeService> arc_bridge_service_;

  // Individual services
  scoped_ptr<ArcAuthService> arc_auth_service_;
  scoped_ptr<ArcClipboardBridge> arc_clipboard_bridge_;
  scoped_ptr<ArcImeBridge> arc_ime_bridge_;
  scoped_ptr<ArcInputBridge> arc_input_bridge_;
  scoped_ptr<ArcIntentHelperBridge> arc_intent_helper_bridge_;
  scoped_ptr<ArcNotificationManager> arc_notification_manager_;
  scoped_ptr<ArcSettingsBridge> arc_settings_bridge_;
  scoped_ptr<ArcPowerBridge> arc_power_bridge_;
  scoped_ptr<ArcVideoBridge> arc_video_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
