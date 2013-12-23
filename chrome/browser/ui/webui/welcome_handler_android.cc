// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_handler_android.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/web_ui.h"

WelcomeHandler::WelcomeHandler()
    : sync_service_(NULL),
      observer_manager_(this),
      is_sync_footer_visible_(false) {}

WelcomeHandler::~WelcomeHandler() {}

void WelcomeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updateSyncFooterVisibility",
      base::Bind(&WelcomeHandler::HandleUpdateSyncFooterVisibility,
      base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "showSyncSettings",
      base::Bind(&WelcomeHandler::HandleShowSyncSettings,
      base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "showTermsOfService",
      base::Bind(&WelcomeHandler::HandleShowTermsOfService,
      base::Unretained(this)));

  // Register for callbacks
  sync_service_ = ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      Profile::FromWebUI(web_ui()));
  if (sync_service_)
    observer_manager_.Add(sync_service_);
}

void WelcomeHandler::HandleUpdateSyncFooterVisibility(
    const base::ListValue* args) {
  UpdateSyncFooterVisibility(true);
}

void WelcomeHandler::HandleShowSyncSettings(const base::ListValue* args) {
  chrome::android::ChromiumApplication::ShowSyncSettings();
}

void WelcomeHandler::HandleShowTermsOfService(const base::ListValue* args) {
  chrome::android::ChromiumApplication::ShowTermsOfServiceDialog();
}

void WelcomeHandler::OnStateChanged() {
  UpdateSyncFooterVisibility(false);
}

void WelcomeHandler::UpdateSyncFooterVisibility(bool forced) {
  bool is_sync_enabled =
      sync_service_ && sync_service_->IsSyncEnabledAndLoggedIn();

  if (forced || is_sync_footer_visible_ != is_sync_enabled) {
    is_sync_footer_visible_ = is_sync_enabled;
    web_ui()->CallJavascriptFunction("welcome.setSyncFooterVisible",
                                     base::FundamentalValue(is_sync_enabled));
  }
}
