// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/extension_app_result.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#endif

namespace app_list {

ExtensionAppResult::ExtensionAppResult(Profile* profile,
                                       const std::string& app_id,
                                       AppListControllerDelegate* controller,
                                       bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id).spec());

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  DCHECK(extension);

  is_platform_app_ = extension->is_platform_app();

  icon_.reset(
      new extensions::IconImage(profile,
                                extension,
                                extensions::IconsInfo::GetIcons(extension),
                                GetPreferredIconDimension(),
                                extensions::util::GetDefaultAppIcon(),
                                this));
  UpdateIcon();

  StartObservingExtensionRegistry();
}

ExtensionAppResult::~ExtensionAppResult() {
  StopObservingExtensionRegistry();
}

void ExtensionAppResult::Open(int event_flags) {
  RecordHistogram(APP_SEARCH_RESULT);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id());
  if (!extension)
    return;

  // Don't auto-enable apps that cannot be launched.
  if (!extensions::util::IsAppLaunchable(app_id(), profile()))
    return;

  // Check if enable flow is already running or should be started
  if (RunExtensionEnableFlow())
    return;

  if (display_type() != DISPLAY_RECOMMENDATION) {
    extensions::RecordAppListSearchLaunch(extension);
    content::RecordAction(
        base::UserMetricsAction("AppList_ClickOnAppFromSearch"));
  }

  controller()->ActivateApp(
      profile(),
      extension,
      AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH,
      event_flags);
}

std::unique_ptr<SearchResult> ExtensionAppResult::Duplicate() const {
  std::unique_ptr<SearchResult> copy(
      new ExtensionAppResult(profile(), app_id(), controller(),
                             display_type() == DISPLAY_RECOMMENDATION));
  copy->set_title(title());
  copy->set_title_tags(title_tags());
  copy->set_relevance(relevance());

  return copy;
}

ui::MenuModel* ExtensionAppResult::GetContextMenuModel() {
  if (!context_menu_) {
    context_menu_.reset(new ExtensionAppContextMenu(
        this, profile(), app_id(), controller()));
    context_menu_->set_is_platform_app(is_platform_app_);
  }

  return context_menu_->GetMenuModel();
}

void ExtensionAppResult::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);

  extension_registry_ = extensions::ExtensionRegistry::Get(profile());
  extension_registry_->AddObserver(this);
}

void ExtensionAppResult::StopObservingExtensionRegistry() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
  extension_registry_ = NULL;
}

bool ExtensionAppResult::RunExtensionEnableFlow() {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id(), profile()))
    return false;

  if (!extension_enable_flow_) {
    controller()->OnShowChildDialog();

    extension_enable_flow_.reset(new ExtensionEnableFlow(
        profile(), app_id(), this));
    extension_enable_flow_->StartForNativeWindow(
        controller()->GetAppListWindow());
  }
  return true;
}

void ExtensionAppResult::UpdateIcon() {
  gfx::ImageSkia icon = icon_->image_skia();

#if defined(OS_CHROMEOS)
  extensions::util::MaybeApplyChromeBadge(profile(), app_id(), &icon);
#endif

  if (!extensions::util::IsAppLaunchable(app_id(), profile())) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

  SetIcon(icon);
}

void ExtensionAppResult::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK_EQ(icon_.get(), image);
  UpdateIcon();
}

void ExtensionAppResult::OnExtensionIconImageDestroyed(
    extensions::IconImage* image) {
  SetIcon(gfx::ImageSkia());
}

void ExtensionAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void ExtensionAppResult::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  controller()->OnCloseChildDialog();

  // Automatically open app after enabling.
  Open(ui::EF_NONE);
}

void ExtensionAppResult::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  controller()->OnCloseChildDialog();
}

void ExtensionAppResult::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  UpdateIcon();
}

void ExtensionAppResult::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

}  // namespace app_list
