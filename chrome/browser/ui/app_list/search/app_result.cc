// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_result.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/tokenized_string.h"
#include "chrome/browser/ui/app_list/search/tokenized_string_match.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "content/public/browser/user_metrics.h"

namespace app_list {

AppResult::AppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller)
    : profile_(profile),
      app_id_(app_id),
      controller_(controller),
      install_tracker_(NULL) {
  set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id_).spec());

  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()
          ->GetInstalledExtension(app_id_);
  DCHECK(extension);

  is_platform_app_ = extension->is_platform_app();

  icon_.reset(new extensions::IconImage(
      profile_,
      extension,
      extensions::IconsInfo::GetIcons(extension),
      extension_misc::EXTENSION_ICON_SMALL,
      extensions::IconsInfo::GetDefaultAppIcon(),
      this));
  SetIcon(icon_->image_skia());
  StartObservingInstall();
}

AppResult::~AppResult() {
  StopObservingInstall();
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

void AppResult::Open(int event_flags) {
  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()
          ->GetInstalledExtension(app_id_);
  if (!extension)
    return;

  CoreAppLauncherHandler::RecordAppListSearchLaunch(extension);
  content::RecordAction(
      content::UserMetricsAction("AppList_ClickOnAppFromSearch"));

  controller_->ActivateApp(profile_, extension, event_flags);
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
        this, profile_, app_id_, controller_, is_platform_app_));
  }

  return context_menu_->GetMenuModel();
}

void AppResult::StartObservingInstall() {
  DCHECK(!install_tracker_);

  install_tracker_ = extensions::InstallTrackerFactory::GetForProfile(profile_);
  install_tracker_->AddObserver(this);
}

void AppResult::StopObservingInstall() {
  if (install_tracker_)
    install_tracker_->RemoveObserver(this);

  install_tracker_ = NULL;
}

void AppResult::OnExtensionIconImageChanged(extensions::IconImage* image) {
  DCHECK_EQ(icon_.get(), image);
  SetIcon(icon_->image_skia());
}

void AppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void AppResult::OnBeginExtensionInstall(const std::string& extension_id,
                                        const std::string& extension_name,
                                        const gfx::ImageSkia& installing_icon,
                                        bool is_app,
                                        bool is_platform_app) {}

void AppResult::OnDownloadProgress(const std::string& extension_id,
                                   int percent_downloaded) {}

void AppResult::OnInstallFailure(const std::string& extension_id) {}

void AppResult::OnExtensionInstalled(const extensions::Extension* extension) {}

void AppResult::OnExtensionLoaded(const extensions::Extension* extension) {}

void AppResult::OnExtensionUnloaded(const extensions::Extension* extension) {}

void AppResult::OnExtensionUninstalled(const extensions::Extension* extension) {
  if (extension->id() != app_id_)
    return;

  NotifyItemUninstalled();
}

void AppResult::OnAppsReordered() {}

void AppResult::OnAppInstalledToAppList(const std::string& extension_id) {}

void AppResult::OnShutdown() { StopObservingInstall(); }

}  // namespace app_list
