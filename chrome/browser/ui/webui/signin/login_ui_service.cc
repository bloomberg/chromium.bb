// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"

LoginUIService::LoginUIService(Profile* profile)
    : ui_(NULL),
      profile_(profile) {
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

void LoginUIService::ShowLoginUI() {
  if (ui_) {
    // We already have active login UI - make it visible.
    ui_->FocusUI();
    return;
  }

  // Need to navigate to the settings page and display the UI.
  if (profile_) {
    Browser* browser = browser::FindLastActiveWithProfile(profile_);
    if (!browser) {
      browser = Browser::Create(profile_);
      browser->ShowOptionsTab(chrome::kSyncSetupSubPage);
      browser->window()->Show();
    } else {
      browser->ShowOptionsTab(chrome::kSyncSetupSubPage);
    }
  }
}
