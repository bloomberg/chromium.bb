// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace settings {

ManageProfileHandler::ManageProfileHandler(Profile* profile)
    : profile_(profile), weak_factory_(this) {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().AddObserver(this);
}

ManageProfileHandler::~ManageProfileHandler() {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().RemoveObserver(this);
}

void ManageProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("setProfileIconAndName",
      base::Bind(&ManageProfileHandler::SetProfileIconAndName,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestDefaultProfileIcons",
      base::Bind(&ManageProfileHandler::RequestDefaultProfileIcons,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestHasProfileShortcuts",
      base::Bind(&ManageProfileHandler::RequestHasProfileShortcuts,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("profileIconSelectionChanged",
      base::Bind(&ManageProfileHandler::ProfileIconSelectionChanged,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("addProfileShortcut",
      base::Bind(&ManageProfileHandler::AddProfileShortcut,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeProfileShortcut",
      base::Bind(&ManageProfileHandler::RemoveProfileShortcut,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("refreshGaiaPicture",
      base::Bind(&ManageProfileHandler::RefreshGaiaPicture,
                 base::Unretained(this)));
}

void ManageProfileHandler::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  SendAvailableIcons();
}

void ManageProfileHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  SendAvailableIcons();
}

void ManageProfileHandler::RequestDefaultProfileIcons(
    const base::ListValue* args) {
  SendAvailableIcons();
}

void ManageProfileHandler::SendAvailableIcons() {
  base::ListValue image_url_list;

  // First add the GAIA picture if it is available.
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    const gfx::Image* icon = entry->GetGAIAPicture();
    if (icon) {
      gfx::Image icon2 = profiles::GetAvatarIconForWebUI(*icon, true);
      gaia_picture_url_ = webui::GetBitmapDataUrl(icon2.AsBitmap());
      image_url_list.AppendString(gaia_picture_url_);
    }
  }

  // Next add the default avatar icons and names.
  for (size_t i = 0; i < profiles::GetDefaultAvatarIconCount(); i++) {
    std::string url = profiles::GetDefaultAvatarIconUrl(i);
    image_url_list.AppendString(url);
  }

  web_ui()->CallJavascriptFunction(
      "settings.SyncPrivateApi.receiveAvailableIcons",
      image_url_list);
}

void ManageProfileHandler::SetProfileIconAndName(const base::ListValue* args) {
  DCHECK(args);

  std::string icon_url;
  if (!args->GetString(0, &icon_url))
    return;

  PrefService* pref_service = profile_->GetPrefs();
  // Updating the profile preferences will cause the cache to be updated.

  // Metrics logging variable.
  bool previously_using_gaia_icon =
      pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar);

  size_t new_icon_index;
  if (icon_url == gaia_picture_url_) {
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, true);
    if (!previously_using_gaia_icon) {
      // Only log if they changed to the GAIA photo.
      // Selection of GAIA photo as avatar is logged as part of the function
      // below.
      ProfileMetrics::LogProfileSwitchGaia(ProfileMetrics::GAIA_OPT_IN);
    }
  } else if (profiles::IsDefaultAvatarIconUrl(icon_url, &new_icon_index)) {
    ProfileMetrics::LogProfileAvatarSelection(new_icon_index);
    pref_service->SetInteger(prefs::kProfileAvatarIndex, new_icon_index);
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);
  } else {
    // Only default avatars and Gaia account photos are supported.
    CHECK(false);
  }
  ProfileMetrics::LogProfileUpdate(profile_->GetPath());

  if (profile_->IsLegacySupervised())
    return;

  base::string16 new_profile_name;
  if (!args->GetString(1, &new_profile_name))
    return;

  base::TrimWhitespace(new_profile_name, base::TRIM_ALL, &new_profile_name);
  CHECK(!new_profile_name.empty());
  profiles::UpdateProfileName(profile_, new_profile_name);
}

void ManageProfileHandler::ProfileIconSelectionChanged(
    const base::ListValue* args) {
  DCHECK(args);

  std::string icon_url;
  if (!args->GetString(0, &icon_url))
    return;

  if (icon_url != gaia_picture_url_)
    return;

  // If the selection is the GAIA picture then also show the profile name in the
  // text field. This will display either the GAIA given name, if available,
  // or the first name.
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    return;
  }
  base::string16 gaia_name = entry->GetName();
  if (gaia_name.empty())
    return;

  web_ui()->CallJavascriptFunction(
      "settings.SyncPrivateApi.setProfileName",
      base::StringValue(gaia_name));
}

void ManageProfileHandler::RequestHasProfileShortcuts(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(profile_->GetPath(), &entry))
    return;

  // Don't show the add/remove desktop shortcut button in the single user case.
  if (storage.GetNumberOfProfiles() <= 1u)
    return;

  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  shortcut_manager->HasProfileShortcuts(
      profile_->GetPath(),
      base::Bind(&ManageProfileHandler::OnHasProfileShortcuts,
                 weak_factory_.GetWeakPtr()));
}

void ManageProfileHandler::OnHasProfileShortcuts(bool has_shortcuts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::FundamentalValue has_shortcuts_value(has_shortcuts);
  web_ui()->CallJavascriptFunction(
      "settings.SyncPrivateApi.receiveHasProfileShortcuts",
      has_shortcuts_value);
}

void ManageProfileHandler::AddProfileShortcut(const base::ListValue* args) {
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->CreateProfileShortcut(profile_->GetPath());

  // Update the UI buttons.
  OnHasProfileShortcuts(true);
}

void ManageProfileHandler::RemoveProfileShortcut(const base::ListValue* args) {
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
    g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->RemoveProfileShortcuts(profile_->GetPath());

  // Update the UI buttons.
  OnHasProfileShortcuts(false);
}

void ManageProfileHandler::RefreshGaiaPicture(const base::ListValue* args) {
  profiles::UpdateGaiaProfileInfoIfNeeded(Profile::FromWebUI(web_ui()));
}

}  // namespace settings
