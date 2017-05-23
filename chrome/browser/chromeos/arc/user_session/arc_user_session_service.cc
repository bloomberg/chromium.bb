// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/user_session/arc_user_session_service.h"

#include "components/arc/arc_bridge_service.h"
#include "components/session_manager/core/session_manager.h"

namespace arc {

ArcUserSessionService::ArcUserSessionService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
  arc_bridge_service()->intent_helper()->AddObserver(this);
}

ArcUserSessionService::~ArcUserSessionService() {
  arc_bridge_service()->intent_helper()->RemoveObserver(this);
}

void ArcUserSessionService::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state != session_manager::SessionState::ACTIVE)
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->intent_helper(), SendBroadcast);
  if (!instance)
    return;

  instance->SendBroadcast(
      "org.chromium.arc.intent_helper.USER_SESSION_ACTIVE",
      "org.chromium.arc.intent_helper",
      "org.chromium.arc.intent_helper.ArcIntentHelperService", "{}");
}

void ArcUserSessionService::OnInstanceReady() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

void ArcUserSessionService::OnInstanceClosed() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

}  // namespace arc
