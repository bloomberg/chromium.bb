// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordFormMap;
using password_manager::PasswordFormManager;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsBubbleUIController);

ManagePasswordsBubbleUIController::ManagePasswordsBubbleUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manage_passwords_icon_to_be_shown_(false),
      password_to_be_saved_(false),
      manage_passwords_bubble_needs_showing_(false),
      password_submitted_(false),
      autofill_blocked_(false) {}

ManagePasswordsBubbleUIController::~ManagePasswordsBubbleUIController() {}

void ManagePasswordsBubbleUIController::UpdateBubbleAndIconVisibility() {
  #if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;
    LocationBar* location_bar = browser->window()->GetLocationBar();
    DCHECK(location_bar);
    location_bar->UpdateManagePasswordsIconAndBubble();
  #endif
}

void ManagePasswordsBubbleUIController::OnPasswordSubmitted(
    PasswordFormManager* form_manager) {
  form_manager_.reset(form_manager);
  password_form_map_ = form_manager_->best_matches();
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = true;
  manage_passwords_bubble_needs_showing_ = true;
  password_submitted_ = true;
  autofill_blocked_ = false;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  password_form_map_ = password_form_map;
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  password_submitted_ = false;
  autofill_blocked_ = false;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnBlacklistBlockedAutofill() {
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  password_submitted_ = false;
  autofill_blocked_ = true;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::RemoveFromBestMatches(
    autofill::PasswordForm password_form) {
  password_form_map_.erase(password_form.username_value);
}

void ManagePasswordsBubbleUIController::OnBubbleShown() {
  unset_manage_passwords_bubble_needs_showing();
}

void ManagePasswordsBubbleUIController::SavePassword() {
  DCHECK(form_manager_.get());
  form_manager_->Save();
}

void ManagePasswordsBubbleUIController::NeverSavePassword() {
  DCHECK(form_manager_.get());
  form_manager_->PermanentlyBlacklist();
}

void ManagePasswordsBubbleUIController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  // Reset password states for next page.
  manage_passwords_icon_to_be_shown_ = false;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  password_submitted_ = false;
  UpdateBubbleAndIconVisibility();
}
