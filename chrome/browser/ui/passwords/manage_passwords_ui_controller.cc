// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/password_dialog_controller_impl.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/navigation_handle.h"

using password_manager::PasswordFormManager;

int ManagePasswordsUIController::save_fallback_timeout_in_seconds_ = 90;

namespace {

password_manager::PasswordStore* GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             ServiceAccessType::EXPLICIT_ACCESS).get();
}

std::vector<std::unique_ptr<autofill::PasswordForm>> CopyFormVector(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> result(forms.size());
  for (size_t i = 0; i < forms.size(); ++i)
    result[i].reset(new autofill::PasswordForm(*forms[i]));
  return result;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsUIController);

ManagePasswordsUIController::ManagePasswordsUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      bubble_status_(NOT_SHOWN),
      weak_ptr_factory_(this) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->AddObserver(this);
}

ManagePasswordsUIController::~ManagePasswordsUIController() {}

void ManagePasswordsUIController::OnPasswordSubmitted(
    std::unique_ptr<PasswordFormManager> form_manager) {
  bool show_bubble = !form_manager->IsBlacklisted();
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnPendingPassword(std::move(form_manager));
  if (show_bubble) {
    const password_manager::InteractionsStats* stats =
        GetCurrentInteractionStats();
    const int show_threshold =
        password_bubble_experiment::GetSmartBubbleDismissalThreshold();
    if (stats && show_threshold > 0 && stats->dismissal_count >= show_threshold)
      show_bubble = false;
  }
  if (show_bubble)
    bubble_status_ = SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnUpdatePasswordSubmitted(
    std::unique_ptr<PasswordFormManager> form_manager) {
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnUpdatePassword(std::move(form_manager));
  bubble_status_ = SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManager> form_manager,
    bool has_generated_password,
    bool is_update) {
  DestroyAccountChooser();
  if (has_generated_password)
    passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  else if (is_update)
    passwords_data_.OnUpdatePassword(std::move(form_manager));
  else
    passwords_data_.OnPendingPassword(std::move(form_manager));
  UpdateBubbleAndIconVisibility();
  save_fallback_timer_.Start(
      FROM_HERE, GetTimeoutForSaveFallback(), this,
      &ManagePasswordsUIController::OnHideManualFallbackForSaving);
}

void ManagePasswordsUIController::OnHideManualFallbackForSaving() {
  if (passwords_data_.state() != password_manager::ui::PENDING_PASSWORD_STATE &&
      passwords_data_.state() !=
          password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      passwords_data_.state() != password_manager::ui::CONFIRMATION_STATE) {
    return;
  }
  // Don't hide the fallback if the bubble is open.
  if (bubble_status_ == SHOWN || bubble_status_ == SHOWN_PENDING_ICON_UPDATE)
    return;

  save_fallback_timer_.Stop();

  if (passwords_data_.GetCurrentForms().empty())
    passwords_data_.OnInactive();
  else
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);

  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::OnChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback) {
  DCHECK(!local_credentials.empty());
  if (!HasBrowserWindow())
    return false;
  // If |local_credentials| contains PSL matches they shouldn't be propagated to
  // the state because PSL matches aren't saved for current page. This logic is
  // implemented here because Android uses ManagePasswordsState as a data source
  // for account chooser.
  PasswordDialogController::FormsVector locals;
  if (!local_credentials[0]->is_public_suffix_match)
    locals = CopyFormVector(local_credentials);
  passwords_data_.OnRequestCredentials(std::move(locals), origin);
  passwords_data_.set_credentials_callback(callback);
  dialog_controller_.reset(new PasswordDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      this));
  dialog_controller_->ShowAccountChooser(
      CreateAccountChooser(dialog_controller_.get()),
      std::move(local_credentials));
  UpdateBubbleAndIconVisibility();
  return true;
}

void ManagePasswordsUIController::OnAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  DestroyAccountChooser();
  passwords_data_.OnAutoSignin(std::move(local_forms), origin);
  bubble_status_ = SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPromptEnableAutoSignin() {
  // Both the account chooser and the previous prompt shouldn't be closed.
  if (dialog_controller_)
    return;
  dialog_controller_.reset(new PasswordDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      this));
  dialog_controller_->ShowAutosigninPrompt(
      CreateAutoSigninPrompt(dialog_controller_.get()));
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    std::unique_ptr<PasswordFormManager> form_manager) {
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  bubble_status_ = SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const std::map<base::string16, const autofill::PasswordForm*>&
        password_form_map,
    const GURL& origin,
    const std::vector<const autofill::PasswordForm*>* federated_matches) {
  // To change to managed state only when the managed state is more important
  // for the user that the current state.
  if (passwords_data_.state() == password_manager::ui::INACTIVE_STATE ||
      passwords_data_.state() == password_manager::ui::MANAGE_STATE) {
    passwords_data_.OnPasswordAutofilled(password_form_map, origin,
                                         federated_matches);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  password_manager::ui::State current_state = GetState();
  passwords_data_.ProcessLoginsChanged(changes);
  if (current_state != GetState())
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIconView* icon) {
  if (bubble_status_ == SHOULD_POP_UP) {
    DCHECK(!dialog_controller_);
    // This will detach any existing bubble so OnBubbleHidden() isn't called.
    weak_ptr_factory_.InvalidateWeakPtrs();
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored.
    icon->SetState(GetState());
    ShowBubbleWithoutUserInteraction();
    // If the bubble appeared then the status is updated in OnBubbleShown().
    if (bubble_status_ == SHOULD_POP_UP)
      bubble_status_ = NOT_SHOWN;
  } else {
    password_manager::ui::State state = GetState();
    // The dialog should hide the icon.
    if (dialog_controller_ &&
        state == password_manager::ui::CREDENTIAL_REQUEST_STATE)
      state = password_manager::ui::INACTIVE_STATE;
    icon->SetState(state);
  }
}

base::WeakPtr<PasswordsModelDelegate>
ManagePasswordsUIController::GetModelDelegateProxy() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* ManagePasswordsUIController::GetWebContents() const {
  return web_contents();
}

const GURL& ManagePasswordsUIController::GetOrigin() const {
  return passwords_data_.origin();
}

password_manager::PasswordFormMetricsRecorder*
ManagePasswordsUIController::GetPasswordFormMetricsRecorder() {
  // The form manager may be null for example for auto sign-in toasts of the
  // credential manager API.
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  return form_manager ? form_manager->metrics_recorder() : nullptr;
}

password_manager::ui::State ManagePasswordsUIController::GetState() const {
  return passwords_data_.state();
}

const autofill::PasswordForm& ManagePasswordsUIController::
    GetPendingPassword() const {
  if (GetState() == password_manager::ui::AUTO_SIGNIN_STATE)
    return *GetCurrentForms()[0];

  DCHECK(GetState() == password_manager::ui::PENDING_PASSWORD_STATE ||
         GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         GetState() == password_manager::ui::CONFIRMATION_STATE)
      << GetState();
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  return form_manager->pending_credentials();
}

password_manager::metrics_util::CredentialSourceType
ManagePasswordsUIController::GetCredentialSource() const {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  return form_manager
             ? form_manager->GetCredentialSource()
             : password_manager::metrics_util::CredentialSourceType::kUnknown;
}

bool ManagePasswordsUIController::IsPasswordOverridden() const {
  const password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  return form_manager ? form_manager->password_overridden() : false;
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
ManagePasswordsUIController::GetCurrentForms() const {
  return passwords_data_.GetCurrentForms();
}

const password_manager::InteractionsStats*
ManagePasswordsUIController::GetCurrentInteractionStats() const {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  return password_manager::FindStatsByUsername(
      form_manager->form_fetcher()->GetInteractionsStats(),
      form_manager->pending_credentials().username_value);
}

bool ManagePasswordsUIController::BubbleIsManualFallbackForSaving() const {
  return save_fallback_timer_.IsRunning();
}

void ManagePasswordsUIController::OnBubbleShown() {
  bubble_status_ = SHOWN;
}

void ManagePasswordsUIController::OnBubbleHidden() {
  bool update_icon = (bubble_status_ == SHOWN_PENDING_ICON_UPDATE);
  bubble_status_ = NOT_SHOWN;
  if (GetState() == password_manager::ui::CONFIRMATION_STATE ||
      GetState() == password_manager::ui::AUTO_SIGNIN_STATE) {
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    update_icon = true;
  }
  if (update_icon)
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnNoInteraction() {
  if (GetState() != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      GetState() != password_manager::ui::PENDING_PASSWORD_STATE) {
    // Do nothing if the state was changed. It can happen for example when the
    // bubble is active and a page navigation happens.
    return;
  }
  bool is_update =
      GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNoInteraction(is_update);
}

void ManagePasswordsUIController::OnNopeUpdateClicked() {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNopeUpdateClicked();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  NeverSavePasswordInternal();
  // The state stays the same.
}

void ManagePasswordsUIController::SavePassword(const base::string16& username,
                                               const base::string16& password) {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  const auto& pending_credentials =
      passwords_data_.form_manager()->pending_credentials();
  bool username_edited = pending_credentials.username_value != username;
  bool password_changed = pending_credentials.password_value != password;
  if (username_edited) {
    passwords_data_.form_manager()->UpdateUsername(username);
    if (GetPasswordFormMetricsRecorder()) {
      GetPasswordFormMetricsRecorder()->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kEditedUsernameInBubble);
    }
  }
  if (password_changed) {
    passwords_data_.form_manager()->UpdatePasswordValue(password);
    if (GetPasswordFormMetricsRecorder()) {
      GetPasswordFormMetricsRecorder()->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kSelectedDifferentPasswordInBubble);
    }
  }

  // Values of this histogram are a bit mask. Only the lower two bits are used:
  // 0001 to indicate that the user has edited the username in the password
  //      save bubble.
  // 0010 to indicate that the user has changed the password in the password
  //      save bubble.
  // The maximum possible value is defined by OR-ing these values.
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.EditsInSaveBubble",
                            username_edited + 2 * password_changed, 4);

  save_fallback_timer_.Stop();
  SavePasswordInternal();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  // The icon is to be updated after the bubble (either "Save password" or "Sign
  // in to Chrome") is closed.
  bubble_status_ = SHOWN_PENDING_ICON_UPDATE;
}

void ManagePasswordsUIController::UpdatePassword(
    const autofill::PasswordForm& password_form) {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE, GetState());
  save_fallback_timer_.Stop();
  UpdatePasswordInternal(password_form);
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  DCHECK(dialog_controller_);
  DCHECK_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
            credential_type);
  // Copy the argument before destroying the controller. |form| is a member of
  // |dialog_controller_|.
  autofill::PasswordForm copy_form = form;
  dialog_controller_.reset();
  passwords_data_.ChooseCredential(&copy_form);
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NavigateToSmartLockHelpPage() {
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      GURL(chrome::kSmartLockHelpPage), ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void ManagePasswordsUIController::NavigateToPasswordManagerSettingsPage() {
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPasswordManagerSubPage);
}

void ManagePasswordsUIController::NavigateToPasswordManagerAccountDashboard() {
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void ManagePasswordsUIController::NavigateToChromeSignIn() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN, signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, false);
}

void ManagePasswordsUIController::OnDialogHidden() {
  dialog_controller_.reset();
  if (GetState() == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::SavePasswordInternal() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  for (auto* form : form_manager->blacklisted_matches()) {
    password_store->RemoveLogin(*form);
  }

  form_manager->Save();
}

void ManagePasswordsUIController::UpdatePasswordInternal(
    const autofill::PasswordForm& password_form) {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  form_manager->Update(password_form);
}

void ManagePasswordsUIController::NeverSavePasswordInternal() {
  password_manager::PasswordFormManager* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNeverClicked();
}

void ManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  // If we're not on a "webby" URL (e.g. "chrome://sign-in"), we shouldn't
  // display either the bubble or the icon.
  if (!ChromePasswordManagerClient::CanShowBubbleOnURL(
          web_contents()->GetLastCommittedURL())) {
    passwords_data_.OnInactive();
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  DCHECK(location_bar);
  location_bar->UpdateManagePasswordsIconAndBubble();
}

AccountChooserPrompt* ManagePasswordsUIController::CreateAccountChooser(
    PasswordDialogController* controller) {
  return CreateAccountChooserPromptView(controller, web_contents());
}

AutoSigninFirstRunPrompt* ManagePasswordsUIController::CreateAutoSigninPrompt(
    PasswordDialogController* controller) {
  return CreateAutoSigninPromptView(controller, web_contents());
}

bool ManagePasswordsUIController::HasBrowserWindow() const {
  return chrome::FindBrowserWithWebContents(web_contents()) != nullptr;
}

void ManagePasswordsUIController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      // Don't react to same-document (fragment) navigations.
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Keep the state if the bubble is currently open or the fallback for saving
  // should be still available.
  if (bubble_status_ == SHOWN || bubble_status_ == SHOWN_PENDING_ICON_UPDATE ||
      save_fallback_timer_.IsRunning()) {
    return;
  }

  // Otherwise, reset the password manager.
  DestroyAccountChooser();
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::WasHidden() {
  TabDialogs::FromWebContents(web_contents())->HideManagePasswordsBubble();
}

// static
base::TimeDelta ManagePasswordsUIController::GetTimeoutForSaveFallback() {
  return base::TimeDelta::FromSeconds(
      ManagePasswordsUIController::save_fallback_timeout_in_seconds_);
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(IsAutomaticallyOpeningBubble());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->toolbar_model()->input_in_progress())
    return;

  CommandUpdater* updater = browser->command_controller()->command_updater();
  updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
}

void ManagePasswordsUIController::DestroyAccountChooser() {
  if (dialog_controller_ && dialog_controller_->IsShowingAccountChooser()) {
    dialog_controller_.reset();
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  }
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveObserver(this);
}
