// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
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

autofill::ConstPasswordFormMap ConstifyMap(
    const autofill::PasswordFormMap& map) {
  autofill::ConstPasswordFormMap ret;
  ret.insert(map.begin(), map.end());
  return ret;
}

// Performs a deep copy of the PasswordForm pointers in |map|. The resulting map
// is returned via |ret|. |deleter| is populated with these new objects.
void DeepCopyMap(const autofill::PasswordFormMap& map,
                 autofill::ConstPasswordFormMap* ret,
                 ScopedVector<autofill::PasswordForm>* deleter) {
  ConstifyMap(map).swap(*ret);
  deleter->clear();
  for (autofill::ConstPasswordFormMap::iterator i = ret->begin();
       i != ret->end(); ++i) {
    deleter->push_back(new autofill::PasswordForm(*i->second));
    i->second = deleter->back();
  }
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsUIController);

ManagePasswordsUIController::ManagePasswordsUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      state_(password_manager::ui::INACTIVE_STATE) {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->AddObserver(this);
}

ManagePasswordsUIController::~ManagePasswordsUIController() {}

void ManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  // If we're not on a "webby" URL (e.g. "chrome://sign-in"), we shouldn't
  // display either the bubble or the icon.
  if (!BrowsingDataHelper::IsWebScheme(
          web_contents()->GetLastCommittedURL().scheme())) {
    state_ = password_manager::ui::INACTIVE_STATE;
  }

  #if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;
    LocationBar* location_bar = browser->window()->GetLocationBar();
    DCHECK(location_bar);
    location_bar->UpdateManagePasswordsIconAndBubble();
  #endif
}

void ManagePasswordsUIController::OnPasswordSubmitted(
    scoped_ptr<PasswordFormManager> form_manager) {
  form_manager_ = form_manager.Pass();
  password_form_map_ = ConstifyMap(form_manager_->best_matches());
  origin_ = PendingCredentials().origin;
  state_ = password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> form_manager) {
  form_manager_ = form_manager.Pass();
  password_form_map_ = ConstifyMap(form_manager_->best_matches());
  password_form_map_[form_manager_->associated_username()] =
      &form_manager_->pending_credentials();
  origin_ = form_manager_->pending_credentials().origin;
  state_ = password_manager::ui::CONFIRMATION_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  DeepCopyMap(password_form_map, &password_form_map_, &new_password_forms_);
  origin_ = password_form_map_.begin()->second->origin;
  state_ = password_manager::ui::MANAGE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnBlacklistBlockedAutofill(
    const PasswordFormMap& password_form_map) {
  DeepCopyMap(password_form_map, &password_form_map_, &new_password_forms_);
  origin_ = password_form_map_.begin()->second->origin;
  state_ = password_manager::ui::BLACKLIST_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveObserver(this);
}

void ManagePasswordsUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  password_manager::ui::State current_state = state_;
  for (password_manager::PasswordStoreChangeList::const_iterator it =
           changes.begin();
       it != changes.end();
       it++) {
    const autofill::PasswordForm& changed_form = it->form();
    if (changed_form.origin != origin_)
      continue;

    if (it->type() == password_manager::PasswordStoreChange::REMOVE) {
      password_form_map_.erase(changed_form.username_value);
      if (changed_form.blacklisted_by_user)
        state_ = password_manager::ui::MANAGE_STATE;
    } else {
      new_password_forms_.push_back(new autofill::PasswordForm(changed_form));
      password_form_map_[changed_form.username_value] =
          new_password_forms_.back();
      if (changed_form.blacklisted_by_user)
        state_ = password_manager::ui::BLACKLIST_STATE;
    }
  }
  if (current_state != state_)
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::
    NavigateToPasswordManagerSettingsPage() {
// TODO(mkwst): chrome_pages.h is compiled out of Android. Need to figure out
// how this navigation should work there.
#if !defined(OS_ANDROID)
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPasswordManagerSubPage);
#endif
}

void ManagePasswordsUIController::SavePassword() {
  DCHECK(PasswordPendingUserDecision());
  SavePasswordInternal();
  state_ = password_manager::ui::MANAGE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::SavePasswordInternal() {
  DCHECK(form_manager_.get());
  form_manager_->Save();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK(PasswordPendingUserDecision());
  NeverSavePasswordInternal();
  state_ = password_manager::ui::BLACKLIST_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NeverSavePasswordInternal() {
  DCHECK(form_manager_.get());
  form_manager_->PermanentlyBlacklist();
}

void ManagePasswordsUIController::UnblacklistSite() {
  // We're in one of two states: either the user _just_ blacklisted the site
  // by clicking "Never save" in the pending bubble, or the user is visiting
  // a blacklisted site.
  //
  // Either way, |password_form_map_| has been populated with the relevant
  // form. We can safely pull it out, send it over to the password store
  // for removal, and update our internal state.
  DCHECK(!password_form_map_.empty());
  DCHECK(password_form_map_.begin()->second);
  DCHECK(state_ == password_manager::ui::BLACKLIST_STATE);
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveLogin(*password_form_map_.begin()->second);
  state_ = password_manager::ui::MANAGE_STATE;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Don't react to in-page (fragment) navigations.
  if (details.is_in_page)
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the password bubble.
  if (timer_ && timer_->Elapsed() < base::TimeDelta::FromSeconds(1))
    return;

  // Otherwise, reset the password manager and the timer.
  state_ = password_manager::ui::INACTIVE_STATE;
  UpdateBubbleAndIconVisibility();
  timer_.reset(new base::ElapsedTimer());
}

void ManagePasswordsUIController::WasHidden() {
#if !defined(OS_ANDROID)
  chrome::CloseManagePasswordsBubble(web_contents());
#endif
}

const autofill::PasswordForm& ManagePasswordsUIController::
    PendingCredentials() const {
  DCHECK(form_manager_);
  return form_manager_->pending_credentials();
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIcon* icon) {
  if (password_manager::ui::IsAutomaticDisplayState(state_)) {
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored. However, we can't change the controller's state
    // until _after_ the bubble is shown, as our metrics depend on the seeing
    // the original state to determine if the bubble opened automagically or via
    // user action.
    password_manager::ui::State end_state =
        GetEndStateForAutomaticState(state_);
    icon->SetState(end_state);
    ShowBubbleWithoutUserInteraction();
    state_ = end_state;
  } else {
    icon->SetState(state_);
  }
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(password_manager::ui::IsAutomaticDisplayState(state_));
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->toolbar_model()->input_in_progress())
    return;
  CommandUpdater* updater = browser->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
#endif
}

bool ManagePasswordsUIController::PasswordPendingUserDecision() const {
  return state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
         state_ == password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE;
}
