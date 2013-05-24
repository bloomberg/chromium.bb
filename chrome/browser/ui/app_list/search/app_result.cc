// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_result.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/tokenized_string.h"
#include "chrome/browser/ui/app_list/search/tokenized_string_match.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
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
      controller_(controller) {
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
}
AppResult::~AppResult() {}

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

  AppLauncherHandler::RecordAppListSearchLaunch(extension);
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

ui::MenuModel* AppResult::GetContextMenuModel() {
  if (!context_menu_) {
    context_menu_.reset(new AppContextMenu(
        this, profile_, app_id_, controller_, is_platform_app_));
  }

  return context_menu_->GetMenuModel();
}

void AppResult::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK_EQ(icon_.get(), image);
  SetIcon(icon_->image_skia());
}

void AppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

}  // namespace app_list
