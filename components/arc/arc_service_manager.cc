// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/auth/arc_auth_service.h"
#include "components/arc/clipboard/arc_clipboard_bridge.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/input/arc_input_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/power/arc_power_bridge.h"
#include "components/arc/settings/arc_settings_bridge.h"
#include "components/arc/video/arc_video_bridge.h"
#include "ui/arc/notification/arc_notification_manager.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager(
    scoped_ptr<ArcAuthService> auth_service,
    scoped_ptr<ArcIntentHelperBridge> intent_helper_bridge,
    scoped_ptr<ArcSettingsBridge> settings_bridge,
    scoped_ptr<ArcVideoBridge> video_bridge)
    : arc_bridge_service_(
          new ArcBridgeServiceImpl(ArcBridgeBootstrap::Create())),
      arc_auth_service_(std::move(auth_service)),
      arc_clipboard_bridge_(new ArcClipboardBridge(arc_bridge_service_.get())),
      arc_ime_bridge_(new ArcImeBridge(arc_bridge_service_.get())),
      arc_input_bridge_(ArcInputBridge::Create(arc_bridge_service_.get())),
      arc_intent_helper_bridge_(std::move(intent_helper_bridge)),
      arc_settings_bridge_(std::move(settings_bridge)),
      arc_power_bridge_(new ArcPowerBridge(arc_bridge_service_.get())),
      arc_video_bridge_(std::move(video_bridge)) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;

  arc_settings_bridge_->StartObservingBridgeServiceChanges();
  arc_auth_service_->StartObservingBridgeServiceChanges();
  arc_intent_helper_bridge_->StartObservingBridgeServiceChanges();
  arc_video_bridge_->StartObservingBridgeServiceChanges();
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(g_arc_service_manager == this);
  g_arc_service_manager = nullptr;
}

// static
ArcServiceManager* ArcServiceManager::Get() {
  DCHECK(g_arc_service_manager);
  DCHECK(g_arc_service_manager->thread_checker_.CalledOnValidThread());
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return arc_bridge_service_.get();
}

void ArcServiceManager::OnPrimaryUserProfilePrepared(
    const AccountId& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  arc_notification_manager_.reset(
      new ArcNotificationManager(arc_bridge_service(), account_id));
}

}  // namespace arc
