// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "url/gurl.h"

namespace arc {

ArcIntentHelperBridge::ArcIntentHelperBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnIntentHelperInstanceReady() {
  arc_bridge_service()->intent_helper_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  GURL gurl(url.get());
  ash::Shell::GetInstance()->delegate()->OpenUrl(gurl);
}

}  // namespace arc
