// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_result.h"

#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search/tokenized_string.h"
#include "ui/app_list/search/tokenized_string_match.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

AppResult::AppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller)
    : profile_(profile),
      app_id_(app_id),
      controller_(controller),
      extension_registry_(NULL) {
  set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id_).spec());
  if (app_list::switches::IsExperimentalAppListEnabled())
    set_display_type(DISPLAY_TILE);

  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()
          ->GetInstalledExtension(app_id_);
  DCHECK(extension);

  is_platform_app_ = extension->is_platform_app();

  icon_.reset(
      new extensions::IconImage(profile_,
                                extension,
                                extensions::IconsInfo::GetIcons(extension),
                                GetPreferredIconDimension(),
                                extensions::util::GetDefaultAppIcon(),
                                this));
  UpdateIcon();

  StartObservingExtensionRegistry();
}

AppResult::~AppResult() {
  StopObservingExtensionRegistry();
}

void AppResult::UpdateFromMatch(const TokenizedString& title,
                                const TokenizedStringMatch& match) {
  const TokenizedStringMatch::Hits& hits = match.hits();

  Tags tags;
  tags.reserve(hits.size());
  for (size_t i = 0; i < hits.size(); ++i)
    tags.push_back(Tag(Tag::MATCH, hits[i].start(), hits[i].end()));

  set_title(title.text());
  set_title_tags(tags);
  set_relevance(match.relevance());
}

void AppResult::UpdateFromLastLaunched(const base::Time& current_time,
                                       const base::Time& last_launched) {
  base::TimeDelta delta = current_time - last_launched;
  DCHECK_LE(0, delta.InSeconds());
  const int kSecondsInWeek = 60 * 60 * 24 * 7;

  // Set the relevance to a value between 0 and 1. This function decays as the
  // time delta increases and reaches a value of 0.5 at 1 week.
  set_relevance(1 / (1 + delta.InSecondsF() / kSecondsInWeek));
}

void AppResult::Open(int event_flags) {
  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()
          ->GetInstalledExtension(app_id_);
  if (!extension)
    return;

  // Don't auto-enable apps that cannot be launched.
  if (!extensions::util::IsAppLaunchable(app_id_, profile_))
    return;

  // Check if enable flow is already running or should be started
  if (RunExtensionEnableFlow())
    return;

  CoreAppLauncherHandler::RecordAppListSearchLaunch(extension);
  content::RecordAction(
      base::UserMetricsAction("AppList_ClickOnAppFromSearch"));

  controller_->ActivateApp(
      profile_,
      extension,
      AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH,
      event_flags);
}

void AppResult::InvokeAction(int action_index, int event_flags) {}

scoped_ptr<ChromeSearchResult> AppResult::Duplicate() {
  scoped_ptr<ChromeSearchResult> copy(
      new AppResult(profile_, app_id_, controller_));
  copy->set_title(title());
  copy->set_title_tags(title_tags());

  return copy.Pass();
}

ChromeSearchResultType AppResult::GetType() {
  return APP_SEARCH_RESULT;
}

ui::MenuModel* AppResult::GetContextMenuModel() {
  if (!context_menu_) {
    context_menu_.reset(new AppContextMenu(
        this, profile_, app_id_, controller_));
    context_menu_->set_is_platform_app(is_platform_app_);
    context_menu_->set_is_search_result(true);
  }

  return context_menu_->GetMenuModel();
}

void AppResult::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);

  extension_registry_ = extensions::ExtensionRegistry::Get(profile_);
  extension_registry_->AddObserver(this);
}

void AppResult::StopObservingExtensionRegistry() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
  extension_registry_ = NULL;
}

bool AppResult::RunExtensionEnableFlow() {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id_, profile_))
    return false;

  if (!extension_enable_flow_) {
    controller_->OnShowChildDialog();

    extension_enable_flow_.reset(new ExtensionEnableFlow(
        profile_, app_id_, this));
    extension_enable_flow_->StartForNativeWindow(
        controller_->GetAppListWindow());
  }
  return true;
}

void AppResult::UpdateIcon() {
  gfx::ImageSkia icon = icon_->image_skia();

  if (!extensions::util::IsAppLaunchable(app_id_, profile_)) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

  SetIcon(icon);
}

void AppResult::OnExtensionIconImageChanged(extensions::IconImage* image) {
  DCHECK_EQ(icon_.get(), image);
  UpdateIcon();
}

void AppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void AppResult::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  controller_->OnCloseChildDialog();

  // Automatically open app after enabling.
  Open(ui::EF_NONE);
}

void AppResult::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  controller_->OnCloseChildDialog();
}

void AppResult::OnExtensionLoaded(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension) {
  UpdateIcon();
}

void AppResult::OnExtensionUninstalled(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension,
                                       extensions::UninstallReason reason) {
  if (extension->id() != app_id_)
    return;

  NotifyItemUninstalled();
}

void AppResult::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

}  // namespace app_list
