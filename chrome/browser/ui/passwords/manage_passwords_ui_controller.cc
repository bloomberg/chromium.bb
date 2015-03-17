// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include "base/auto_reset.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/password_bubble_experiment.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "content/public/browser/navigation_details.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"
#endif

using autofill::PasswordFormMap;
using password_manager::PasswordFormManager;

namespace {

password_manager::PasswordStore* GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             ServiceAccessType::EXPLICIT_ACCESS).get();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsUIController);

ManagePasswordsUIController::ManagePasswordsUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      should_pop_up_bubble_(false) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
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
    passwords_data_.OnInactive();
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

void ManagePasswordsUIController::
    UpdateAndroidAccountChooserInfoBarVisibility() {
#if defined(OS_ANDROID)
  AccountChooserInfoBarDelegateAndroid::Create(
      InfoBarService::FromWebContents(web_contents()), this);
  should_pop_up_bubble_ = false;
#endif
}

base::TimeDelta ManagePasswordsUIController::Elapsed() const {
  return timer_ ? timer_->Elapsed() : base::TimeDelta::Max();
}

void ManagePasswordsUIController::OnPasswordSubmitted(
    scoped_ptr<PasswordFormManager> form_manager) {
  passwords_data_.OnPendingPassword(form_manager.Pass());
  timer_.reset(new base::ElapsedTimer);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::OnChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  DCHECK_IMPLIES(local_credentials.empty(), !federated_credentials.empty());
  passwords_data_.OnRequestCredentials(local_credentials.Pass(),
                                       federated_credentials.Pass(),
                                       origin);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
#if defined(OS_ANDROID)
  UpdateAndroidAccountChooserInfoBarVisibility();
#else
  UpdateBubbleAndIconVisibility();
#endif
  if (!should_pop_up_bubble_) {
    passwords_data_.set_credentials_callback(callback);
    return true;
  }
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  return false;
}

void ManagePasswordsUIController::OnAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
  DCHECK(!local_forms.empty());
  passwords_data_.OnAutoSignin(local_forms.Pass());
  timer_.reset(new base::ElapsedTimer);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> form_manager) {
  passwords_data_.OnAutomaticPasswordSave(form_manager.Pass());
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  passwords_data_.OnPasswordAutofilled(password_form_map);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnBlacklistBlockedAutofill(
    const PasswordFormMap& password_form_map) {
  passwords_data_.OnBlacklistBlockedAutofill(password_form_map);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  password_manager::ui::State current_state = state();
  passwords_data_.ProcessLoginsChanged(changes);
  if (current_state != state())
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::
    NavigateToPasswordManagerSettingsPage() {
#if defined(OS_ANDROID)
  chrome::android::ChromiumApplication::ShowPasswordSettings();
#else
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPasswordManagerSubPage);
#endif
}

void ManagePasswordsUIController::SavePassword() {
  DCHECK(PasswordPendingUserDecision());
  SavePasswordInternal();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state());
  DCHECK(!passwords_data_.credentials_callback().is_null());

  // Here, |credential_type| refers to whether the credential was originally
  // passed into ::OnChooseCredentials as part of the |local_credentials| or
  // |federated_credentials| lists (e.g. whether it is an existing credential
  // saved for this origin, or whether we should synthesize a new
  // FederatedCredential).
  //
  // If |credential_type| is federated, the credential MUST be returned as
  // a FederatedCredential in order to prevent password information leaking
  // cross-origin.
  //
  // If |credential_type| is local, the credential MIGHT be a LocalCredential
  // or it MIGHT be a FederatedCredential. We inspect the |federation_url|
  // field to determine which we should return.
  //
  // TODO(mkwst): Clean this up. It is confusing.
  password_manager::CredentialType type_to_return;
  if (credential_type ==
          password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL &&
      form.federation_url.is_empty()) {
    type_to_return = password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL;
  } else if (credential_type ==
             password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY) {
    type_to_return = password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
  } else {
    type_to_return =
        password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
  }
  password_manager::CredentialInfo info =
      password_manager::CredentialInfo(form, type_to_return);
  passwords_data_.credentials_callback().Run(info);
  passwords_data_.set_credentials_callback(
      ManagePasswordsState::CredentialsCallback());
}

void ManagePasswordsUIController::SavePasswordInternal() {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->Save();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK(PasswordPendingUserDecision());
  NeverSavePasswordInternal();
  passwords_data_.TransitionToState(password_manager::ui::BLACKLIST_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NeverSavePasswordInternal() {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->PermanentlyBlacklist();
}

void ManagePasswordsUIController::UnblacklistSite() {
  // We're in one of two states: either the user _just_ blacklisted the site
  // by clicking "Never save" in the pending bubble, or the user is visiting
  // a blacklisted site.
  //
  // Either way, |passwords_data_| has been populated with the relevant form. We
  // can safely pull it out, send it over to the password store for removal, and
  // update our internal state.
  DCHECK(!passwords_data_.GetCurrentForms().empty());
  DCHECK_EQ(password_manager::ui::BLACKLIST_STATE, state());
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  DCHECK(GetCurrentForms().front()->blacklisted_by_user);
  if (password_store)
    password_store->RemoveLogin(*GetCurrentForms().front());
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
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
  if (Elapsed() < base::TimeDelta::FromSeconds(1))
    return;

  // Otherwise, reset the password manager and the timer.
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
  // This allows the bubble to survive several redirects in case the whole
  // process of navigating to the landing page is longer than 1 second.
  timer_.reset(new base::ElapsedTimer());
}

void ManagePasswordsUIController::WasHidden() {
#if !defined(OS_ANDROID)
  TabDialogs::FromWebContents(web_contents())->HideManagePasswordsBubble();
#endif
}

const autofill::PasswordForm& ManagePasswordsUIController::
    PendingPassword() const {
  DCHECK(state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         state() == password_manager::ui::CONFIRMATION_STATE) << state();
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  return form_manager->pending_credentials();
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIcon* icon) {
  if (should_pop_up_bubble_) {
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored.
    icon->SetState(state());
    ShowBubbleWithoutUserInteraction();
  } else {
    icon->SetState(state());
  }
}

void ManagePasswordsUIController::OnBubbleShown() {
  should_pop_up_bubble_ = false;
}

void ManagePasswordsUIController::OnBubbleHidden() {
  password_manager::ui::State next_state = state();
  if (state() == password_manager::ui::CREDENTIAL_REQUEST_STATE)
    next_state = password_manager::ui::MANAGE_STATE;
  else if (state() == password_manager::ui::CONFIRMATION_STATE)
    next_state = password_manager::ui::MANAGE_STATE;
  else if (state() == password_manager::ui::AUTO_SIGNIN_STATE)
    next_state = password_manager::ui::INACTIVE_STATE;

  if (next_state != state()) {
    if (next_state == password_manager::ui::INACTIVE_STATE)
      passwords_data_.OnInactive();
    else
      passwords_data_.TransitionToState(next_state);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(should_pop_up_bubble_);
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->toolbar_model()->input_in_progress())
    return;
  if (state() == password_manager::ui::PENDING_PASSWORD_STATE &&
      !password_bubble_experiment::ShouldShowBubble(
          browser->profile()->GetPrefs()))
    return;
  CommandUpdater* updater = browser->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
#endif
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveObserver(this);
}
