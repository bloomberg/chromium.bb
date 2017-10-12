// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/keychain_migration_status_mac.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "components/prefs/pref_registry_simple.h"
#endif

using autofill::PasswordForm;

namespace password_manager {

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

bool URLsEqualUpToScheme(const GURL& a, const GURL& b) {
  return (a.GetContent() == b.GetContent());
}

bool URLsEqualUpToHttpHttpsSubstitution(const GURL& a, const GURL& b) {
  if (a == b)
    return true;

  // The first-time and retry login forms action URLs sometimes differ in
  // switching from HTTP to HTTPS, see http://crbug.com/400769.
  if (a.SchemeIsHTTPOrHTTPS() && b.SchemeIsHTTPOrHTTPS())
    return URLsEqualUpToScheme(a, b);

  return false;
}

// Helper UMA reporting function for differences in URLs during form submission.
void RecordWhetherTargetDomainDiffers(const GURL& src, const GURL& target) {
  bool target_domain_differs =
      !net::registry_controlled_domains::SameDomainOrHost(
          src, target,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SubmitNavigatesToDifferentDomain",
                        target_domain_differs);
}

bool IsSignupForm(const PasswordForm& form) {
  return !form.new_password_element.empty() && form.password_element.empty();
}

// Tries to find if at least one of the values from |server_field_predictions|
// can be converted from AutofillQueryResponseContents::Field::FieldPrediction
// to a PasswordFormFieldPredictionType stored in |type|. Returns true if the
// conversion was made.
bool ServerPredictionsToPasswordFormPrediction(
    std::vector<autofill::AutofillQueryResponseContents::Field::FieldPrediction>
        server_field_predictions,
    autofill::PasswordFormFieldPredictionType* type) {
  for (auto const& server_field_prediction : server_field_predictions) {
    switch (server_field_prediction.type()) {
      case autofill::USERNAME:
      case autofill::USERNAME_AND_EMAIL_ADDRESS:
        *type = autofill::PREDICTION_USERNAME;
        return true;

      case autofill::PASSWORD:
        *type = autofill::PREDICTION_CURRENT_PASSWORD;
        return true;

      case autofill::ACCOUNT_CREATION_PASSWORD:
        *type = autofill::PREDICTION_NEW_PASSWORD;
        return true;

      default:
        break;
    }
  }
  return false;
}

// Returns true if the |field_type| is known to be possibly
// misinterpreted as a password by the Password Manager.
bool IsPredictedTypeNotPasswordPrediction(
    autofill::ServerFieldType field_type) {
  return field_type == autofill::CREDIT_CARD_NUMBER ||
         field_type == autofill::CREDIT_CARD_VERIFICATION_CODE;
}

bool PreferredRealmIsFromAndroid(
    const autofill::PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(
             fill_data.preferred_realm).IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(
    const autofill::PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(
            login.second.realm).IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

bool AreAllFieldsEmpty(const PasswordForm& form) {
  return form.username_value.empty() && form.password_value.empty() &&
         form.new_password_value.empty();
}

// Helper function that determines whether update or save prompt should be
// shown for credentials in |provisional_save_manager|.
bool IsPasswordUpdate(const PasswordFormManager& provisional_save_manager) {
  return (!provisional_save_manager.best_matches().empty() &&
          provisional_save_manager
              .is_possible_change_password_form_without_username()) ||
         provisional_save_manager.password_overridden() ||
         provisional_save_manager.retry_password_form_password_update();
}

// Finds the matched form manager for |form| in |pending_login_managers|.
PasswordFormManager* FindMatchedManager(
    const autofill::PasswordForm& form,
    const std::vector<std::unique_ptr<PasswordFormManager>>&
        pending_login_managers,
    const password_manager::PasswordManagerDriver* driver,
    BrowserSavePasswordProgressLogger* logger) {
  auto matched_manager_it = pending_login_managers.end();
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;
  // Below, "matching" is in DoesManage-sense and "not ready" in the sense of
  // FormFetcher being ready. We keep track of such PasswordFormManager
  // instances for UMA.
  for (auto iter = pending_login_managers.begin();
       iter != pending_login_managers.end(); ++iter) {
    PasswordFormManager::MatchResultMask result =
        (*iter)->DoesManage(form, driver);

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      if (logger)
        logger->LogMessage(Logger::STRING_EXACT_MATCH);
      matched_manager_it = iter;
      break;
    }

    if (result > current_match_result) {
      current_match_result = result;
      matched_manager_it = iter;

      if (logger) {
        if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                       ~PasswordFormManager::RESULT_ACTION_MATCH))
          logger->LogMessage(Logger::STRING_MATCH_WITHOUT_ACTION);
        if (IsSignupForm(form))
          logger->LogMessage(Logger::STRING_ORIGINS_MATCH);
      }
    }
  }

  return matched_manager_it == pending_login_managers.end()
             ? nullptr
             : matched_manager_it->get();
}

}  // namespace

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kCredentialsEnableService, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kCredentialsEnableAutosignin, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(prefs::kWasObsoleteHttpDataCleaned, false);
  registry->RegisterStringPref(prefs::kSyncPasswordHash, std::string(),
                               PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterStringPref(prefs::kSyncPasswordLengthAndHashSalt,
                               std::string(),
                               PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterBooleanPref(
      prefs::kWasAutoSignInFirstRunExperienceShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
#if defined(OS_MACOSX)
  registry->RegisterIntegerPref(
      prefs::kKeychainMigrationStatus,
      static_cast<int>(MigrationStatus::MIGRATED_DELETED));
#endif
}

#if defined(OS_WIN)
// static
void PasswordManager::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kOsPasswordLastChanged, 0);
  registry->RegisterBooleanPref(prefs::kOsPasswordBlank, false);
}
#endif

PasswordManager::PasswordManager(PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

PasswordManager::~PasswordManager() {
  for (LoginModelObserver& observer : observers_)
    observer.OnLoginModelDestroying();
}

void PasswordManager::GenerationAvailableForForm(const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->MarkGenerationAvailable();
    return;
  }
}

void PasswordManager::OnPresaveGeneratedPassword(
    const autofill::PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());
  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->PresaveGeneratedPassword(form);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                          false);
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager", true);
}

void PasswordManager::OnPasswordNoLongerGenerated(const PasswordForm& form) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->PasswordNoLongerGenerated();
    return;
  }
}

void PasswordManager::SetGenerationElementAndReasonForForm(
    password_manager::PasswordManagerDriver* driver,
    const autofill::PasswordForm& form,
    const base::string16& generation_element,
    bool is_manually_triggered) {
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->set_generation_element(generation_element);
    form_manager->set_is_manual_generation(is_manually_triggered);
    form_manager->set_generation_popup_was_shown(true);
    return;
  }

  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  auto manager = std::make_unique<PasswordFormManager>(
      this, client_, driver->AsWeakPtr(), form,
      std::make_unique<FormSaverImpl>(client_->GetPasswordStore()), nullptr);
  manager->Init(nullptr);
  pending_login_managers_.push_back(std::move(manager));
}

void PasswordManager::SaveGenerationFieldDetectedByClassifier(
    const autofill::PasswordForm& form,
    const base::string16& generation_field) {
  if (!client_->IsSavingAndFillingEnabledForCurrentPage())
    return;
  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager)
    form_manager->SaveGenerationFieldDetectedByClassifier(generation_field);
}

void PasswordManager::ProvisionallySavePassword(
    const PasswordForm& form,
    const password_manager::PasswordManagerDriver* driver) {
  bool is_saving_and_filling_enabled =
      client_->IsSavingAndFillingEnabledForCurrentPage();

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD);
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM,
                            form);
  }

  if (!is_saving_and_filling_enabled) {
    client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_DISABLED, main_frame_url_,
        form.origin, logger.get());
    return;
  }

  // No password to save? Then don't.
  if (PasswordFormManager::PasswordToSave(form).empty()) {
    client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::EMPTY_PASSWORD, main_frame_url_,
        form.origin, logger.get());
    return;
  }

  bool should_block = ShouldBlockPasswordForSameOriginButDifferentScheme(form);
  metrics_util::LogShouldBlockPasswordForSameOriginButDifferentScheme(
      should_block);
  if (should_block) {
    client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::SAVING_ON_HTTP_AFTER_HTTPS,
        main_frame_url_, form.origin, logger.get());
    return;
  }

  PasswordFormManager* matched_manager =
      FindMatchedManager(form, pending_login_managers_, driver, logger.get());

  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!matched_manager) {
    client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::NO_MATCHING_FORM, main_frame_url_,
        form.origin, logger.get());
    return;
  }
  matched_manager->SaveSubmittedFormTypeForMetrics(form);

  ProvisionallySaveManager(form, matched_manager, logger.get());

  // Cache the user-visible URL (i.e., the one seen in the omnibox). Once the
  // post-submit navigation concludes, we compare the landing URL against the
  // cached and report the difference through UMA.
  main_frame_url_ = client_->GetMainFrameURL();

  // Report SubmittedFormFrame metric.
  if (driver) {
    metrics_util::SubmittedFormFrame frame;
    if (driver->IsMainFrame()) {
      frame = metrics_util::SubmittedFormFrame::MAIN_FRAME;
    } else if (form.origin == main_frame_url_) {
      frame =
          metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME;
    } else {
      GURL::Replacements rep;
      rep.SetPathStr("");
      std::string main_frame_signon_realm =
          main_frame_url_.ReplaceComponents(rep).spec();
      frame =
          (main_frame_signon_realm == form.signon_realm)
              ? metrics_util::SubmittedFormFrame::
                    IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME
              : metrics_util::SubmittedFormFrame::
                    IFRAME_WITH_DIFFERENT_SIGNON_REALM;
    }
    metrics_util::LogSubmittedFormFrame(frame);
  }
}

void PasswordManager::UpdateFormManagers() {
  for (const auto& form_manager : pending_login_managers_) {
    form_manager->form_fetcher()->Fetch();
  }
}

void PasswordManager::DropFormManagers() {
  pending_login_managers_.clear();
  provisional_save_manager_.reset();
  all_visible_forms_.clear();
}

bool PasswordManager::IsPasswordFieldDetectedOnPage() {
  return !pending_login_managers_.empty();
}

void PasswordManager::AddSubmissionCallback(
    const PasswordSubmittedCallback& callback) {
  submission_callbacks_.push_back(callback);
}

void PasswordManager::AddObserverAndDeliverCredentials(
    LoginModelObserver* observer,
    const PasswordForm& observed_form) {
  observers_.AddObserver(observer);

  observer->set_signon_realm(observed_form.signon_realm);
  // TODO(vabr): Even though the observers do the realm check, this mechanism
  // will still result in every observer being notified about every form. We
  // could perhaps do better by registering an observer call-back instead.

  std::vector<PasswordForm> observed_forms;
  observed_forms.push_back(observed_form);
  OnPasswordFormsParsed(nullptr, observed_forms);
}

void PasswordManager::RemoveObserver(LoginModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordManager::DidNavigateMainFrame() {
  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormSubmitted(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  ProvisionallySavePassword(password_form, driver);
  for (size_t i = 0; i < submission_callbacks_.size(); ++i) {
    submission_callbacks_[i].Run(password_form);
  }

  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormForceSaveRequested(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  // TODO(msramek): This is just a sketch. We will need to show a custom bubble,
  // mark the form as force saved, and recreate the pending login managers,
  // because the password store might have changed.
  ProvisionallySavePassword(password_form, driver);
  if (provisional_save_manager_)
    OnLoginSuccessful();
}

void PasswordManager::ShowManualFallbackForSaving(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  if (!client_->IsSavingAndFillingEnabledForCurrentPage() ||
      ShouldBlockPasswordForSameOriginButDifferentScheme(password_form))
    return;

  PasswordFormManager* matched_manager = FindMatchedManager(
      password_form, pending_login_managers_, driver, nullptr);
  if (!matched_manager)
    return;
  // TODO(crbug.com/741537): Process manual saving request even if there is
  // still no response from the store.
  if (matched_manager->form_fetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    return;
  }
  ProvisionallySaveManager(password_form, matched_manager, nullptr);

  // Show the fallback if a prompt or a confirmation bubble should be available.
  bool has_generated_password =
      provisional_save_manager_->has_generated_password();
  if (ShouldPromptUserToSavePassword() || has_generated_password) {
    DCHECK(provisional_save_manager_);
    bool is_update = IsPasswordUpdate(*provisional_save_manager_);
    client_->ShowManualFallbackForSaving(std::move(provisional_save_manager_),
                                         has_generated_password, is_update);
  } else {
    provisional_save_manager_.reset();
    HideManualFallbackForSaving();
  }
}

void PasswordManager::HideManualFallbackForSaving() {
  client_->HideManualFallbackForSaving();
}

void PasswordManager::OnPasswordFormsParsed(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  CreatePendingLoginManagers(driver, forms);
}

void PasswordManager::CreatePendingLoginManagers(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_CREATE_LOGIN_MANAGERS_METHOD);
  }

  // Record whether or not this top-level URL has at least one password field.
  client_->AnnotateNavigationEntry(!forms.empty());

  if (!client_->IsFillingEnabledForCurrentPage())
    return;

  if (logger) {
    logger->LogNumber(Logger::STRING_OLD_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }

  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (base::EndsWith(iter->signon_realm, kSpdyProxyRealm,
                       base::CompareCase::SENSITIVE))
      continue;
    bool old_manager_found = false;
    for (const auto& old_manager : pending_login_managers_) {
      if (old_manager->DoesManage(*iter, driver) !=
          PasswordFormManager::RESULT_COMPLETE_MATCH) {
        continue;
      }
      old_manager_found = true;
      if (driver)
        old_manager->ProcessFrame(driver->AsWeakPtr());
      break;
    }
    if (old_manager_found)
      continue;  // The current form is already managed.

    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.ParsedUsernameField",
                          iter->username_element.empty());

    // Out of the forms not containing a username field, determine how many
    // are password change forms.
    if (iter->username_element.empty()) {
      UMA_HISTOGRAM_BOOLEAN(
          "PasswordManager.EmptyUsernames."
          "FormWithoutUsernameFieldIsPasswordChangeForm",
          !iter->new_password_element.empty());
    }

    if (logger)
      logger->LogFormSignatures(Logger::STRING_ADDING_SIGNATURE, *iter);
    auto manager = std::make_unique<PasswordFormManager>(
        this, client_,
        (driver ? driver->AsWeakPtr() : base::WeakPtr<PasswordManagerDriver>()),
        *iter, std::make_unique<FormSaverImpl>(client_->GetPasswordStore()),
        nullptr);
    manager->Init(nullptr);
    pending_login_managers_.push_back(std::move(manager));
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NEW_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }
}

bool PasswordManager::OtherPossibleUsernamesEnabled() const {
  return false;
}

void PasswordManager::ProvisionallySaveManager(
    const PasswordForm& form,
    PasswordFormManager* matched_manager,
    BrowserSavePasswordProgressLogger* logger) {
  DCHECK(matched_manager);
  std::unique_ptr<PasswordFormManager> manager = matched_manager->Clone();

  PasswordForm submitted_form(form);
  submitted_form.preferred = true;
  if (logger) {
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVED_FORM,
                            submitted_form);
  }
  PasswordFormManager::OtherPossibleUsernamesAction action =
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES;
  if (OtherPossibleUsernamesEnabled())
    action = PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES;
  if (logger) {
    logger->LogBoolean(
        Logger::STRING_IGNORE_POSSIBLE_USERNAMES,
        action == PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  }
  manager->ProvisionallySave(submitted_form, action);
  provisional_save_manager_.swap(manager);
}

bool PasswordManager::CanProvisionalManagerSave() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD);
  }

  if (!provisional_save_manager_) {
    if (logger) {
      logger->LogMessage(Logger::STRING_NO_PROVISIONAL_SAVE_MANAGER);
    }
    return false;
  }

  if (provisional_save_manager_->form_fetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    // We have a provisional save manager, but it didn't finish matching yet.
    // We just give up.
    client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
        PasswordManagerMetricsRecorder::MATCHING_NOT_COMPLETE, main_frame_url_,
        provisional_save_manager_->observed_form().origin, logger.get());
    provisional_save_manager_.reset();
    return false;
  }
  return true;
}

bool PasswordManager::ShouldBlockPasswordForSameOriginButDifferentScheme(
    const PasswordForm& form) const {
  const GURL& old_origin = main_frame_url_.GetOrigin();
  const GURL& new_origin = form.origin.GetOrigin();
  return old_origin.host_piece() == new_origin.host_piece() &&
         old_origin.SchemeIsCryptographic() &&
         !new_origin.SchemeIsCryptographic();
}

bool PasswordManager::ShouldPromptUserToSavePassword() const {
  return (provisional_save_manager_->IsNewLogin() ||
          provisional_save_manager_
              ->is_possible_change_password_form_without_username() ||
          provisional_save_manager_->retry_password_form_password_update() ||
          provisional_save_manager_->password_overridden()) &&
         !(provisional_save_manager_->has_generated_password() &&
           provisional_save_manager_->IsNewLogin()) &&
         !provisional_save_manager_->IsPendingCredentialsPublicSuffixMatch();
}

void PasswordManager::OnPasswordFormsRendered(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& visible_forms,
    bool did_stop_loading) {
  CreatePendingLoginManagers(driver, visible_forms);
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD);
  }

  if (!CanProvisionalManagerSave())
    return;

  // If the server throws an internal error, access denied page, page not
  // found etc. after a login attempt, we do not save the credentials.
  if (client_->WasLastNavigationHTTPError()) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    provisional_save_manager_->LogSubmitFailed();
    provisional_save_manager_.reset();
    return;
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NUMBER_OF_VISIBLE_FORMS,
                      visible_forms.size());
  }

  // Record all visible forms from the frame.
  all_visible_forms_.insert(all_visible_forms_.end(),
                            visible_forms.begin(),
                            visible_forms.end());

  // If we see the login form again, then the login failed.
  if (did_stop_loading) {
    if (provisional_save_manager_->pending_credentials().scheme ==
        PasswordForm::SCHEME_HTML) {
      for (size_t i = 0; i < all_visible_forms_.size(); ++i) {
        // TODO(vabr): The similarity check is just action equality up to
        // HTTP<->HTTPS substitution for now. If it becomes more complex, it may
        // make sense to consider modifying and using
        // PasswordFormManager::DoesManage for it.
        if (all_visible_forms_[i].action.is_valid() &&
            URLsEqualUpToHttpHttpsSubstitution(
                provisional_save_manager_->pending_credentials().action,
                all_visible_forms_[i].action)) {
          if (provisional_save_manager_
                  ->is_possible_change_password_form_without_username() &&
              AreAllFieldsEmpty(all_visible_forms_[i]))
            continue;
          provisional_save_manager_->LogSubmitFailed();
          if (logger) {
            logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                                    all_visible_forms_[i]);
            logger->LogMessage(Logger::STRING_DECISION_DROP);
          }
          provisional_save_manager_.reset();
          // Clear all_visible_forms_ once we found the match.
          all_visible_forms_.clear();
          return;
        }
      }
    } else {
      if (logger)
        logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML);
    }

    // Clear all_visible_forms_ after checking all the visible forms.
    all_visible_forms_.clear();

    // Looks like a successful login attempt. Either show an infobar or
    // automatically save the login data. We prompt when the user hasn't
    // already given consent, either through previously accepting the infobar
    // or by having the browser generate the password.
    OnLoginSuccessful();
  }
}

void PasswordManager::OnInPageNavigation(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_IN_PAGE_NAVIGATION);
  }

  ProvisionallySavePassword(password_form, driver);

  if (!CanProvisionalManagerSave())
    return;

  OnLoginSuccessful();
}

void PasswordManager::OnLoginSuccessful() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_ASK_USER_OR_SAVE_PASSWORD);
  }

  client_->GetStoreResultFilter()->ReportFormLoginSuccess(
      *provisional_save_manager_);
  if (provisional_save_manager_->submitted_form()) {
    metrics_util::LogPasswordSuccessfulSubmissionIndicatorEvent(
        provisional_save_manager_->submitted_form()->submission_event);
    if (logger) {
      logger->LogSuccessfulSubmissionIndicatorEvent(
          provisional_save_manager_->submitted_form()->submission_event);
    }
  }

  if (base::FeatureList::IsEnabled(features::kDropSyncCredential)) {
    DCHECK(provisional_save_manager_->submitted_form());
    if (!client_->GetStoreResultFilter()->ShouldSave(
            *provisional_save_manager_->submitted_form())) {
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS)) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
      // When |username_value| is empty, it's not clear whether the submitted
      // credentials are really sync credentials. Don't save sync password hash
      // in that case.
      if (!provisional_save_manager_->submitted_form()
               ->username_value.empty()) {
        password_manager::PasswordStore* store = client_->GetPasswordStore();
        // May be null in tests.
        if (store) {
          metrics_util::LogSyncPasswordHashChange(
              metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA);
          store->SaveSyncPasswordHash(
              provisional_save_manager_->submitted_form()->password_value);
        }
      }
#endif
      provisional_save_manager_->WipeStoreCopyIfOutdated();
      client_->GetMetricsRecorder().RecordProvisionalSaveFailure(
          PasswordManagerMetricsRecorder::SYNC_CREDENTIAL, main_frame_url_,
          provisional_save_manager_->observed_form().origin, logger.get());
      provisional_save_manager_.reset();
      return;
    }
  }

  provisional_save_manager_->LogSubmitPassed();

  RecordWhetherTargetDomainDiffers(main_frame_url_, client_->GetMainFrameURL());

  if (ShouldPromptUserToSavePassword()) {
    bool empty_password =
        provisional_save_manager_->pending_credentials().username_value.empty();
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.OfferedToSave",
                          empty_password);
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_ASK);
    bool update_password = IsPasswordUpdate(*provisional_save_manager_);
    if (client_->PromptUserToSaveOrUpdatePassword(
            std::move(provisional_save_manager_), update_password)) {
      if (logger)
        logger->LogMessage(Logger::STRING_SHOW_PASSWORD_PROMPT);
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_SAVE);
    provisional_save_manager_->Save();

    if (!provisional_save_manager_->IsNewLogin()) {
      client_->NotifySuccessfulLoginWithExistingPassword(
          provisional_save_manager_->pending_credentials());
    }

    if (provisional_save_manager_->has_generated_password()) {
      client_->AutomaticPasswordSave(std::move(provisional_save_manager_));
    } else {
      provisional_save_manager_.reset();
    }
  }
}

void PasswordManager::Autofill(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& form_for_autofill,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<const PasswordForm*>& federated_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) const {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }

  autofill::PasswordFormFillData fill_data;
  InitPasswordFormFillData(form_for_autofill, best_matches, &preferred_match,
                           wait_for_username, OtherPossibleUsernamesEnabled(),
                           &fill_data);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
      ContainsAndroidCredentials(fill_data));
  metrics_util::LogFilledCredentialIsFromAndroidApp(
      PreferredRealmIsFromAndroid(fill_data));
  driver->FillPasswordForm(fill_data);

  client_->PasswordWasAutofilled(best_matches, form_for_autofill.origin,
                                 &federated_matches);
}

void PasswordManager::ShowInitialPasswordAccountSuggestions(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& form_for_autofill,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) const {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(
        Logger::
            STRING_PASSWORDMANAGER_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS);
  }
  autofill::PasswordFormFillData fill_data;
  InitPasswordFormFillData(form_for_autofill, best_matches, &preferred_match,
                           wait_for_username, OtherPossibleUsernamesEnabled(),
                           &fill_data);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  driver->ShowInitialPasswordAccountSuggestions(fill_data);
}

void PasswordManager::AutofillHttpAuth(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& preferred_match) const {
  DCHECK_NE(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILLHTTPAUTH);
    logger->LogBoolean(Logger::STRING_LOGINMODELOBSERVER_PRESENT,
                       observers_.might_have_observers());
  }

  for (LoginModelObserver& observer : observers_)
    observer.OnAutofillDataAvailable(preferred_match);
  DCHECK(!best_matches.empty());
  client_->PasswordWasAutofilled(best_matches,
                                 best_matches.begin()->second->origin, nullptr);
}

void PasswordManager::ProcessAutofillPredictions(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_))
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));

  // Leave only forms that contain fields that are useful for password manager.
  std::map<autofill::FormData, autofill::PasswordFormFieldPredictionMap>
      predictions;
  for (const autofill::FormStructure* form : forms) {
    if (logger)
      logger->LogFormStructure(Logger::STRING_SERVER_PREDICTIONS, *form);
    for (const auto& field : *form) {
      autofill::PasswordFormFieldPredictionType prediction_type;
      if (ServerPredictionsToPasswordFormPrediction(field->server_predictions(),
                                                    &prediction_type)) {
        predictions[form->ToFormData()][*field] = prediction_type;
      }
      // Certain fields are annotated by the browsers as "not passwords" i.e.
      // they should not be treated as passwords by the Password Manager.
      if (field->form_control_type == "password" &&
          IsPredictedTypeNotPasswordPrediction(
              field->Type().GetStorableType())) {
        predictions[form->ToFormData()][*field] =
            autofill::PREDICTION_NOT_PASSWORD;
      }
    }
  }
  if (predictions.empty())
    return;
  driver->AutofillDataReceived(predictions);
}

PasswordFormManager* PasswordManager::GetMatchingPendingManager(
    const PasswordForm& form) {
  PasswordFormManager* matched_manager = nullptr;
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;

  for (auto& login_manager : pending_login_managers_) {
    PasswordFormManager::MatchResultMask result =
        login_manager->DoesManage(form, nullptr);

    if (result == PasswordFormManager::RESULT_NO_MATCH)
      continue;

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      matched_manager = login_manager.get();
      break;
    } else if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                          ~PasswordFormManager::RESULT_ACTION_MATCH) &&
               result > current_match_result) {
      // If the current manager matches the submitted form excluding the action
      // URL, remember it as a candidate and continue searching for an exact
      // match. See http://crbug.com/27246 for an example where actions can
      // change.
      matched_manager = login_manager.get();
      current_match_result = result;
    } else if (result > current_match_result) {
      matched_manager = login_manager.get();
      current_match_result = result;
    }
  }
  return matched_manager;
}

}  // namespace password_manager
