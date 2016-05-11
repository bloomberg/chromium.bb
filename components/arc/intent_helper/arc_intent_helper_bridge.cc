// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <vector>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {

namespace {

ui::ScaleFactor GetSupportedScaleFactor() {
  std::vector<ui::ScaleFactor> scale_factors = ui::GetSupportedScaleFactors();
  DCHECK(!scale_factors.empty());
  return scale_factors.back();
}

}  // namespace

ArcIntentHelperBridge::ArcIntentHelperBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      binding_(this),
      icon_loader_(new ActivityIconLoader(GetSupportedScaleFactor())) {
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

void ArcIntentHelperBridge::OnIconInvalidated(
    const mojo::String& package_name) {
  icon_loader_->InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  GURL gurl(url.get());
  ash::Shell::GetInstance()->delegate()->OpenUrl(gurl);
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  std::unique_ptr<LinkHandlerModelImpl> impl(
      new LinkHandlerModelImpl(icon_loader_));
  if (!impl->Init(url))
    return nullptr;
  return std::move(impl);
}

}  // namespace arc
