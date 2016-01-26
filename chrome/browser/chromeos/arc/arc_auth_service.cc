// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"

#include <utility>

namespace arc {

ArcAuthService::ArcAuthService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcAuthService::~ArcAuthService() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcAuthService::OnAuthInstanceReady() {
  arc_bridge_service()->auth_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcAuthService::GetAuthCode(const GetAuthCodeCallback& callback) {
  // TODO(victorhsieh): request auth code from LSO (crbug.com/571146).
  callback.Run(mojo::String("fake auth code from ArcAuthService in Chrome"));
}

}  // namespace arc
