// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/window_manager/arc_window_manager_bridge.h"

#include "base/logging.h"

namespace arc {

ArcWindowManagerBridge::ArcWindowManagerBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
}

void ArcWindowManagerBridge::OnInstanceReady() {
}

ArcWindowManagerBridge::~ArcWindowManagerBridge() {
}

}  // namespace arc
