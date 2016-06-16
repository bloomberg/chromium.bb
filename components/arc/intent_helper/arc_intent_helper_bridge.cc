// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <vector>

#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kArcIntentHelperPackageName[] = "org.chromium.arc.intent_helper";

}  // namespace

ArcIntentHelperBridge::ArcIntentHelperBridge(
    ArcBridgeService* bridge_service,
    const scoped_refptr<ActivityIconLoader>& icon_loader)
    : ArcService(bridge_service), binding_(this), icon_loader_(icon_loader) {
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

void ArcIntentHelperBridge::OnOpenDownloads() {
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  ash::Shell::GetInstance()->new_window_delegate()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  GURL gurl(url.get());
  ash::Shell::GetInstance()->delegate()->OpenUrl(gurl);
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  ash::Shell::GetInstance()->user_wallpaper_delegate()->OpenSetWallpaperPage();
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  std::unique_ptr<LinkHandlerModelImpl> impl(
      new LinkHandlerModelImpl(icon_loader_));
  if (!impl->Init(url))
    return nullptr;
  return std::move(impl);
}

// static
bool ArcIntentHelperBridge::IsIntentHelperPackage(
    const std::string& package_name) {
  return package_name == kArcIntentHelperPackageName;
}

// static
mojo::Array<mojom::UrlHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    mojo::Array<mojom::UrlHandlerInfoPtr> handlers) {
  mojo::Array<mojom::UrlHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name.get()))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

}  // namespace arc
