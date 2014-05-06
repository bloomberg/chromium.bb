// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordFormMap;
using password_manager::PasswordFormManager;

namespace {

password_manager::PasswordStore* GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             Profile::EXPLICIT_ACCESS).get();
}

} // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsBubbleUIController);

ManagePasswordsBubbleUIController::ManagePasswordsBubbleUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      state_(password_manager::ui::INACTIVE_STATE) {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
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

void ManagePasswordsBubbleUIController::OnPasswordSubmitted(
    PasswordFormManager* form_manager) {
  form_manager_.reset(form_manager);
  password_form_map_ = form_manager_->best_matches();
  origin_ = PendingCredentials().origin;
  state_ = password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  password_form_map_ = password_form_map;
  origin_ = password_form_map_.begin()->second->origin;
  state_ = password_manager::ui::MANAGE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnBlacklistBlockedAutofill(
    const PasswordFormMap& password_form_map) {
  password_form_map_ = password_form_map;
  origin_ = password_form_map_.begin()->second->origin;
  state_ = password_manager::ui::BLACKLIST_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::WebContentsDestroyed(
    content::WebContents* web_contents) {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->RemoveObserver(this);
}

void ManagePasswordsBubbleUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (password_manager::PasswordStoreChangeList::const_iterator it =
           changes.begin();
       it != changes.end();
       it++) {
    const autofill::PasswordForm& changed_form = it->form();
    if (changed_form.origin != origin_)
      continue;

    if (it->type() == password_manager::PasswordStoreChange::REMOVE) {
      password_form_map_.erase(changed_form.username_value);
    } else {
      autofill::PasswordForm* new_form =
          new autofill::PasswordForm(changed_form);
      password_form_map_[changed_form.username_value] = new_form;
    }
  }
}

void ManagePasswordsBubbleUIController::
    NavigateToPasswordManagerSettingsPage() {
// TODO(mkwst): chrome_pages.h is compiled out of Android. Need to figure out
// how this navigation should work there.
#if !defined(OS_ANDROID)
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPasswordManagerSubPage);
#endif
}

void ManagePasswordsBubbleUIController::SavePassword() {
  DCHECK(PasswordPendingUserDecision());
  DCHECK(form_manager_.get());
  form_manager_->Save();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleUIController::NeverSavePassword() {
  DCHECK(PasswordPendingUserDecision());
  DCHECK(form_manager_.get());
  form_manager_->PermanentlyBlacklist();
  state_ = password_manager::ui::BLACKLIST_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::UnblacklistSite() {
  // We're in one of two states: either the user _just_ blacklisted the site
  // by clicking "Never save" in the pending bubble, or the user is visiting
  // a blacklisted site.
  //
  // Either way, |password_form_map_| has been populated with the relevant
  // form. We can safely pull it out, send it over to the password store
  // for removal, and update our internal state.
  DCHECK(!password_form_map_.empty());
  DCHECK(state_ == password_manager::ui::BLACKLIST_STATE);
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveLogin(*password_form_map_.begin()->second);
  state_ = password_manager::ui::MANAGE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  state_ = password_manager::ui::INACTIVE_STATE;
  UpdateBubbleAndIconVisibility();
}

const autofill::PasswordForm& ManagePasswordsBubbleUIController::
    PendingCredentials() const {
  DCHECK(form_manager_);
  return form_manager_->pending_credentials();
}

void ManagePasswordsBubbleUIController::UpdateIconAndBubbleState(
    ManagePasswordsIcon* icon) {
  if (state_ == password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE) {
    ShowBubbleWithoutUserInteraction();
    state_ = password_manager::ui::PENDING_PASSWORD_STATE;
  }
  icon->SetState(state_);
}

void ManagePasswordsBubbleUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK_EQ(state_, password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->toolbar_model()->input_in_progress())
    return;
  CommandUpdater* updater = browser->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
#endif
}

bool ManagePasswordsBubbleUIController::PasswordPendingUserDecision() const {
  return state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE;
}
