// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_sync_setup_handler.h"

#include "base/command_line.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"

NewTabSyncSetupHandler::NewTabSyncSetupHandler() : SyncSetupHandler() {
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
  PrefService* pref_service = web_ui->GetProfile()->GetPrefs();
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

  SyncSetupHandler::RegisterMessages();
}

void NewTabSyncSetupHandler::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
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
  ProfileSyncService* service = web_ui_->GetProfile()->GetProfileSyncService();
  service->get_wizard().Step(SyncSetupWizard::GetLoginState());
}

void NewTabSyncSetupHandler::HandleInitializeSyncPromo(const ListValue* args) {
  if (!ShouldShowSyncPromo())
    return;

  // Make sure the sync promo is visible.
  web_ui_->CallJavascriptFunction("new_tab.NewTabSyncPromo.showSyncPromo");

  UpdateLogin();

  ProfileSyncService* service = web_ui_->GetProfile()->GetProfileSyncService();
  DCHECK(service);

  // If the user has not signed into sync then expand the sync promo.
  // TODO(sail): Need to throttle this behind a server side flag.
  if (!service->HasSyncSetupCompleted())
    OpenSyncSetup();
}

void NewTabSyncSetupHandler::HandleCollapseSyncPromo(const ListValue* args) {
  CloseSyncSetup();
}


void NewTabSyncSetupHandler::HandleExpandSyncPromo(const ListValue* args) {
  OpenSyncSetup();
}

void NewTabSyncSetupHandler::UpdateLogin() {
  std::string username = web_ui_->GetProfile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  StringValue string_value(username);
  web_ui_->CallJavascriptFunction("new_tab.NewTabSyncPromo.updateLogin",
                                  string_value);
}
