// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordFormMap;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsBubbleUIController);

ManagePasswordsBubbleUIController::ManagePasswordsBubbleUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      PasswordManager::PasswordObserver(
          PasswordManager::FromWebContents(web_contents)),
      manage_passwords_icon_to_be_shown_(false),
      password_to_be_saved_(false),
      manage_passwords_bubble_needs_showing_(false) {
  PasswordStore* password_store = GetPasswordStore(web_contents);
  // This cannot be a DCHECK because the unit_tests do not have a PasswordStore.
  if (password_store)
    password_store->AddObserver(this);
}

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

void ManagePasswordsBubbleUIController::OnBubbleShown() {
  unset_manage_passwords_bubble_needs_showing();
}

void ManagePasswordsBubbleUIController::OnCredentialAction(
    autofill::PasswordForm password_form,
    bool remove) {
  if (!web_contents())
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS).get();
  DCHECK(password_store);
  if (remove)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ManagePasswordsBubbleUIController::SavePassword() {
  GetPasswordStore(web_contents())->AddLogin(pending_credentials_);
  password_to_be_saved_ = false;
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
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::WebContentsDestroyed(
    content::WebContents* web_contents) {
  PasswordStore* password_store = GetPasswordStore(web_contents);
  if (password_store)
    password_store->RemoveObserver(this);
}

void ManagePasswordsBubbleUIController::OnLoginsChanged() {
  UpdateBestMatches();
}

void ManagePasswordsBubbleUIController::OnPasswordAction(
    PasswordObserver::BubbleNotification notification,
    const autofill::PasswordFormMap& best_matches,
    const autofill::PasswordForm& pending_credentials) {
  DCHECK(PasswordObserver::SAVE_NEW_PASSWORD ||
         PasswordObserver::AUTOFILL_PASSWORDS);
  password_form_map_ = best_matches;

  observed_form_ = pending_credentials;
  pending_credentials_ = pending_credentials;
  manage_passwords_icon_to_be_shown_ = true;
  if (notification == PasswordObserver::SAVE_NEW_PASSWORD) {
    password_to_be_saved_ = true;
    manage_passwords_bubble_needs_showing_ = true;
  } else {
    password_to_be_saved_ = false;
    manage_passwords_bubble_needs_showing_ = false;
  }
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnPasswordsUpdated(
    const autofill::PasswordFormMap& best_matches) {
  password_form_map_ = best_matches;
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  UpdateBubbleAndIconVisibility();
}

PasswordStore* ManagePasswordsBubbleUIController::GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      Profile::EXPLICIT_ACCESS).get();
}

void ManagePasswordsBubbleUIController::UpdateBestMatches() {
  if (!observed_form_.origin.is_valid())
    return;
  PasswordManager* password_manager =
      PasswordManager::FromWebContents(web_contents());
  if (!password_manager)
    return;
  password_manager->UpdateBestMatches(observed_form_);
}
