// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
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
  ash::Shell::GetInstance()->set_link_handler_model_factory(this);
  arc_bridge_service()->intent_helper_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcIntentHelperBridge::OnIntentHelperInstanceClosed() {
  ash::Shell::GetInstance()->set_link_handler_model_factory(nullptr);
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  GURL gurl(url.get());
  ash::Shell::GetInstance()->delegate()->OpenUrl(gurl);
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  std::unique_ptr<LinkHandlerModelImpl> impl(new LinkHandlerModelImpl);
  if (!impl->Init(url))
    return nullptr;
  return std::move(impl);
}

}  // namespace arc
