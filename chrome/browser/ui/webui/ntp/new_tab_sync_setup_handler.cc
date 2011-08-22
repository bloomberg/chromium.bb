// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_sync_setup_handler.h"

#include "base/command_line.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

NewTabSyncSetupHandler::NewTabSyncSetupHandler() : SyncSetupHandler() {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 NotificationService::AllSources());
}

NewTabSyncSetupHandler::~NewTabSyncSetupHandler() {
}

bool NewTabSyncSetupHandler::ShouldShowSyncPromo() {
#if defined(OS_CHROMEOS)
  // There's no need to show the sync promo on cros since cros users are logged
  // into sync already.
  return false;
#endif

  // Temporarily hide this feature behind a command line flag.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kSyncShowPromo);
}

WebUIMessageHandler* NewTabSyncSetupHandler::Attach(WebUI* web_ui) {
  PrefService* pref_service = Profile::FromWebUI(web_ui)->GetPrefs();
  username_pref_.Init(prefs::kGoogleServicesUsername, pref_service, this);

  return SyncSetupHandler::Attach(web_ui);
}

void NewTabSyncSetupHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("InitializeSyncPromo",
      NewCallback(this, &NewTabSyncSetupHandler::HandleInitializeSyncPromo));
  web_ui_->RegisterMessageCallback("CollapseSyncPromo",
      NewCallback(this, &NewTabSyncSetupHandler::HandleCollapseSyncPromo));
  web_ui_->RegisterMessageCallback("ExpandSyncPromo",
      NewCallback(this, &NewTabSyncSetupHandler::HandleExpandSyncPromo));
  web_ui_->RegisterMessageCallback("ShowProfilesMenu",
      NewCallback(this, &NewTabSyncSetupHandler::HandleShowProfilesMenu));

  SyncSetupHandler::RegisterMessages();
}

void NewTabSyncSetupHandler::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED) {
    UpdateLogin();
    return;
  }

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* name = Details<std::string>(details).ptr();
    if (prefs::kGoogleServicesUsername == *name) {
      UpdateLogin();
      return;
    }
  }
  SyncSetupHandler::Observe(type, source, details);
}

void NewTabSyncSetupHandler::ShowSetupUI() {
  ProfileSyncService* service =
      Profile::FromWebUI(web_ui_)->GetProfileSyncService();
  service->get_wizard().Step(SyncSetupWizard::GetLoginState());
}

void NewTabSyncSetupHandler::HandleInitializeSyncPromo(const ListValue* args) {
  if (!ShouldShowSyncPromo())
    return;

  // Make sure the sync promo is visible.
  web_ui_->CallJavascriptFunction("new_tab.NewTabSyncPromo.showSyncPromo");

  UpdateLogin();

  Profile* profile = Profile::FromWebUI(web_ui_);
  ProfileSyncService* service = profile->GetProfileSyncService();
  DCHECK(service);

  // If the user has not signed into sync then expand the sync promo.
  // TODO(sail): Need to throttle this behind a server side flag.
  if (!service->HasSyncSetupCompleted() &&
      profile->GetPrefs()->GetBoolean(prefs::kSyncPromoExpanded)) {
    OpenSyncSetup();
    SaveExpandedPreference(true);
  }
}

void NewTabSyncSetupHandler::HandleCollapseSyncPromo(const ListValue* args) {
  CloseSyncSetup();
  SaveExpandedPreference(false);
}


void NewTabSyncSetupHandler::HandleExpandSyncPromo(const ListValue* args) {
  OpenSyncSetup();
  SaveExpandedPreference(true);
}

void NewTabSyncSetupHandler::HandleShowProfilesMenu(const ListValue* args) {
  // TODO(sail): Show the profiles menu.
}

void NewTabSyncSetupHandler::UpdateLogin() {
  std::string username = Profile::FromWebUI(web_ui_)->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);

  string16 status_msg;
  if (username.empty()) {
    status_msg = l10n_util::GetStringUTF16(IDS_SYNC_STATUS_NOT_CONNECTED);
  } else {
    string16 short_product_name =
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
    status_msg = l10n_util::GetStringFUTF16(IDS_SYNC_STATUS_CONNECTED,
                                            short_product_name,
                                            UTF8ToUTF16(username));
  }
  StringValue status_msg_value(status_msg);

  std::string icon_url;
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetMutableProfileInfo();
  if (cache.GetNumberOfProfiles() > 1) {
    size_t index = cache.GetIndexOfProfileWithPath(
        Profile::FromWebUI(web_ui_)->GetPath());
    if (index != std::string::npos) {
      size_t icon_index = cache.GetAvatarIconIndexOfProfileAtIndex(index);
      icon_url = ProfileInfoCache::GetDefaultAvatarIconUrl(icon_index);
    }
  }
  StringValue icon_url_value(icon_url);

  // If the user isn't signed in then make the login text clickable so that
  // users can click on it to expand the sync promo. Otherwise, if the user
  // has multiple profiles then clicking on it should show the profiles menu.
  base::FundamentalValue is_clickable_value(
      username.empty() || cache.GetNumberOfProfiles() > 1);

  base::FundamentalValue is_signed_in_value(!username.empty());
  web_ui_->CallJavascriptFunction("new_tab.NewTabSyncPromo.updateLogin",
                                  status_msg_value, icon_url_value,
                                  is_signed_in_value, is_clickable_value);
}

void NewTabSyncSetupHandler::SaveExpandedPreference(bool is_expanded) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  profile->GetPrefs()->SetBoolean(prefs::kSyncPromoExpanded, is_expanded);
  profile->GetPrefs()->ScheduleSavePersistentPrefs();
}
