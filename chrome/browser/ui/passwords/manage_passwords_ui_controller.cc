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
      state_(password_manager::ui::INACTIVE_STATE),
      should_pop_up_bubble_(false) {
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
    SetState(password_manager::ui::INACTIVE_STATE);
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
  form_manager_ = form_manager.Pass();
  password_form_map_ = ConstifyMap(form_manager_->best_matches());
  origin_ = PendingPassword().origin;
  SetState(password_manager::ui::PENDING_PASSWORD_STATE);
  timer_.reset(new base::ElapsedTimer);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::OnChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  DCHECK(!local_credentials.empty() || !federated_credentials.empty());
  SaveForms(local_credentials.Pass(), federated_credentials.Pass());
  origin_ = origin;
  SetState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
#if defined(OS_ANDROID)
  UpdateAndroidAccountChooserInfoBarVisibility();
#else
  UpdateBubbleAndIconVisibility();
#endif
  if (!should_pop_up_bubble_) {
    credentials_callback_ = callback;
    return true;
  }
  return false;
}

void ManagePasswordsUIController::OnAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
  DCHECK(!local_forms.empty());
  SaveForms(local_forms.Pass(), ScopedVector<autofill::PasswordForm>());
  origin_ = local_credentials_forms_[0]->origin;
  SetState(password_manager::ui::AUTO_SIGNIN_STATE);
  timer_.reset(new base::ElapsedTimer);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> form_manager) {
  form_manager_ = form_manager.Pass();
  password_form_map_ = ConstifyMap(form_manager_->best_matches());
  password_form_map_[form_manager_->associated_username()] =
      &form_manager_->pending_credentials();
  origin_ = form_manager_->pending_credentials().origin;
  SetState(password_manager::ui::CONFIRMATION_STATE);
  base::AutoReset<bool> resetter(&should_pop_up_bubble_, true);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  DeepCopyMap(password_form_map, &password_form_map_, &new_password_forms_);
  origin_ = password_form_map_.begin()->second->origin;
  // Don't show the UI for PSL matched passwords. They are not stored for this
  // page and cannot be deleted.
  SetState(password_form_map_.begin()->second->IsPublicSuffixMatch()
               ? password_manager::ui::INACTIVE_STATE
               : password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnBlacklistBlockedAutofill(
    const PasswordFormMap& password_form_map) {
  DeepCopyMap(password_form_map, &password_form_map_, &new_password_forms_);
  origin_ = password_form_map_.begin()->second->origin;
  SetState(password_manager::ui::BLACKLIST_STATE);
  UpdateBubbleAndIconVisibility();
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
        SetState(password_manager::ui::MANAGE_STATE);
    } else {
      new_password_forms_.push_back(new autofill::PasswordForm(changed_form));
      password_form_map_[changed_form.username_value] =
          new_password_forms_.back();
      if (changed_form.blacklisted_by_user)
        SetState(password_manager::ui::BLACKLIST_STATE);
    }
  }
  // TODO(vasilii): handle CREDENTIAL_REQUEST_STATE.
  if (current_state != state_)
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
  SetState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state_);
  DCHECK(!credentials_callback_.is_null());

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
  credentials_callback_.Run(info);
  credentials_callback_.Reset();
}

void ManagePasswordsUIController::SavePasswordInternal() {
  DCHECK(form_manager_.get());
  form_manager_->Save();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK(PasswordPendingUserDecision());
  NeverSavePasswordInternal();
  SetState(password_manager::ui::BLACKLIST_STATE);
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
  SetState(password_manager::ui::MANAGE_STATE);
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
  SetState(password_manager::ui::INACTIVE_STATE);
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

void ManagePasswordsUIController::SetState(password_manager::ui::State state) {
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  // |client| might be NULL in tests.
  if (client && client->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(client);
    logger.LogNumber(
        autofill::SavePasswordProgressLogger::STRING_NEW_UI_STATE,
        state);
  }
  state_ = state;
}

const autofill::PasswordForm& ManagePasswordsUIController::
    PendingPassword() const {
  DCHECK(form_manager_);
  return form_manager_->pending_credentials();
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIcon* icon) {
  if (should_pop_up_bubble_) {
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored.
    icon->SetState(state_);
    ShowBubbleWithoutUserInteraction();
  } else {
    icon->SetState(state_);
  }
}

void ManagePasswordsUIController::OnBubbleShown() {
  should_pop_up_bubble_ = false;
}

void ManagePasswordsUIController::OnBubbleHidden() {
  password_manager::ui::State next_state = state_;
  if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE)
    next_state = password_manager::ui::INACTIVE_STATE;
  else if (state_ == password_manager::ui::CONFIRMATION_STATE)
    next_state = password_manager::ui::MANAGE_STATE;
  else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE)
    next_state = password_manager::ui::INACTIVE_STATE;

  if (next_state != state_) {
    SetState(next_state);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(should_pop_up_bubble_);
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->toolbar_model()->input_in_progress())
    return;
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE &&
      !password_bubble_experiment::ShouldShowBubble(
          browser->profile()->GetPrefs()))
    return;
  CommandUpdater* updater = browser->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
#endif
}

bool ManagePasswordsUIController::PasswordPendingUserDecision() const {
  return state_ == password_manager::ui::PENDING_PASSWORD_STATE;
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveObserver(this);
}

void ManagePasswordsUIController::SaveForms(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms) {
  form_manager_.reset();
  origin_ = GURL();
  local_credentials_forms_.swap(local_forms);
  federated_credentials_forms_.swap(federated_forms);
  // The map is useless because usernames may overlap.
  password_form_map_.clear();
  new_password_forms_.clear();
}
