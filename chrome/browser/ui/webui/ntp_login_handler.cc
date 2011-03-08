// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_login_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/common/pref_names.h"
#include "content/browser/webui/web_ui_util.h"
#include "content/common/notification_details.h"

NTPLoginHandler::NTPLoginHandler() {
}

NTPLoginHandler::~NTPLoginHandler() {
}

WebUIMessageHandler* NTPLoginHandler::Attach(WebUI* web_ui) {
  PrefService* pref_service = web_ui->GetProfile()->GetPrefs();
  username_pref_.Init(prefs::kGoogleServicesUsername, pref_service, this);

  return WebUIMessageHandler::Attach(web_ui);
}

void NTPLoginHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("initializeLogin",
      NewCallback(this, &NTPLoginHandler::HandleInitializeLogin));
}

void NTPLoginHandler::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == NotificationType::PREF_CHANGED);
  std::string* name = Details<std::string>(details).ptr();
  if (prefs::kGoogleServicesUsername == *name)
    UpdateLogin();
}

void NTPLoginHandler::HandleInitializeLogin(const ListValue* args) {
  UpdateLogin();
}

void NTPLoginHandler::UpdateLogin() {
  std::string username = web_ui_->GetProfile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  StringValue string_value(username);
  web_ui_->CallJavascriptFunction(L"updateLogin", string_value);
}
