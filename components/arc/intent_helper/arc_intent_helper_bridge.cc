// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>

#include "ash/common/new_window_controller.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
#include "components/arc/intent_helper/local_activity_resolver.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {

// static
const char ArcIntentHelperBridge::kArcServiceName[] =
    "arc::ArcIntentHelperBridge";

// static
const char ArcIntentHelperBridge::kArcIntentHelperPackageName[] =
    "org.chromium.arc.intent_helper";

ArcIntentHelperBridge::ArcIntentHelperBridge(
    ArcBridgeService* bridge_service,
    const scoped_refptr<LocalActivityResolver>& activity_resolver)
    : ArcService(bridge_service),
      binding_(this),
      activity_resolver_(activity_resolver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(this);
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->intent_helper(), Init);
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcIntentHelperBridge::OnInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(nullptr);
}

void ArcIntentHelperBridge::OnIconInvalidated(const std::string& package_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  icon_loader_.InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  ash::WmShell::Get()->new_window_controller()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::WmShell::Get()->delegate()->OpenUrlFromArc(GURL(url));
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::WmShell::Get()->wallpaper_controller()->OpenSetWallpaperPage();
}

void ArcIntentHelperBridge::SetWallpaperDeprecated(
    const std::vector<uint8_t>& jpeg_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << "IntentHelper.SetWallpaper is deprecated";
}

ArcIntentHelperBridge::GetResult ArcIntentHelperBridge::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    const OnIconsReadyCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return icon_loader_.GetActivityIcons(activities, callback);
}

void ArcIntentHelperBridge::AddObserver(ArcIntentHelperObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcIntentHelperBridge::RemoveObserver(ArcIntentHelperObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto impl = base::MakeUnique<LinkHandlerModelImpl>();
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
std::vector<mojom::IntentHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    std::vector<IntentFilter> filters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  activity_resolver_->UpdateIntentFilters(std::move(filters));

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated();
}

}  // namespace arc
