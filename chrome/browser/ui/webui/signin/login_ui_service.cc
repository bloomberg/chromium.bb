// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"

LoginUIService::LoginUIService() : ui_(NULL) {
}

LoginUIService::~LoginUIService() {}

void LoginUIService::SetLoginUI(LoginUI* ui) {
  DCHECK(!current_login_ui() || current_login_ui() == ui);
  ui_ = ui;
}

void LoginUIService::LoginUIClosed(LoginUI* ui) {
  if (current_login_ui() == ui)
    ui_ = NULL;
}

void LoginUIService::ShowLoginUI(Browser* browser) {
  if (ui_) {
    // We already have active login UI - make it visible.
    ui_->FocusUI();
    return;
  }

  // Need to navigate to the settings page and display the UI.
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}
