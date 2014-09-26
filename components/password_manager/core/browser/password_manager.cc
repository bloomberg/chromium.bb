// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "components/autofill/core/common/password_autofill_util.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"

using autofill::PasswordForm;
using autofill::PasswordFormMap;

namespace password_manager {

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

// This routine is called when PasswordManagers are constructed.
//
// Currently we report metrics only once at startup. We require
// that this is only ever called from a single thread in order to
// avoid needing to lock (a static boolean flag is then sufficient to
// guarantee running only once).
void ReportMetrics(bool password_manager_enabled) {
  static base::PlatformThreadId initial_thread_id =
      base::PlatformThread::CurrentId();
  DCHECK(initial_thread_id == base::PlatformThread::CurrentId());

  static bool ran_once = false;
  if (ran_once)
    return;
  ran_once = true;

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.Enabled", password_manager_enabled);
}

bool ShouldDropSyncCredential() {
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerDropSyncCredential");

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDropSyncCredential))
    return true;

  if (command_line->HasSwitch(switches::kDisableDropSyncCredential))
    return false;

  // Default to not saving.
  return group_name != "Disabled";
}

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

}  // namespace

const char PasswordManager::kOtherPossibleUsernamesExperiment[] =
    "PasswordManagerOtherPossibleUsernames";

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerSavingEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerAllowShowPasswords,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPasswordManagerGroupsForDomains,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

PasswordManager::PasswordManager(PasswordManagerClient* client)
    : client_(client), driver_(client->GetDriver()) {
  DCHECK(client_);
  DCHECK(driver_);
  saving_passwords_enabled_.Init(prefs::kPasswordManagerSavingEnabled,
                                 client_->GetPrefs());

  ReportMetrics(*saving_passwords_enabled_);
}

PasswordManager::~PasswordManager() {
  FOR_EACH_OBSERVER(LoginModelObserver, observers_, OnLoginModelDestroying());
}

void PasswordManager::SetFormHasGeneratedPassword(const PasswordForm& form) {
  DCHECK(IsSavingEnabledForCurrentPage());

  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end();
       ++iter) {
    if ((*iter)->DoesManage(form) ==
        PasswordFormManager::RESULT_COMPLETE_MATCH) {
      (*iter)->SetHasGeneratedPassword();
      return;
    }
  }
  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  bool ssl_valid = form.origin.SchemeIsSecure();
  PasswordFormManager* manager =
      new PasswordFormManager(this, client_, driver_, form, ssl_valid);
  pending_login_managers_.push_back(manager);
  manager->SetHasGeneratedPassword();
  // TODO(gcasto): Add UMA stats to track this.
}

bool PasswordManager::IsEnabledForCurrentPage() const {
  return !driver_->DidLastPageLoadEncounterSSLErrors() &&
      client_->IsPasswordManagerEnabledForCurrentPage();
}

bool PasswordManager::IsSavingEnabledForCurrentPage() const {
  return *saving_passwords_enabled_ && !driver_->IsOffTheRecord() &&
         IsEnabledForCurrentPage();
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  bool is_saving_enabled = IsSavingEnabledForCurrentPage();

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD);
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM,
                            form);
    logger->LogBoolean(Logger::STRING_IS_SAVING_ENABLED, is_saving_enabled);
    logger->LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT,
                       driver_->DidLastPageLoadEncounterSSLErrors());
  }

  if (!is_saving_enabled) {
    RecordFailure(SAVING_DISABLED, form.origin.host(), logger.get());
    return;
  }

  // No password to save? Then don't.
  if ((form.new_password_element.empty() && form.password_value.empty()) ||
      (!form.new_password_element.empty() && form.new_password_value.empty())) {
    RecordFailure(EMPTY_PASSWORD, form.origin.host(), logger.get());
    return;
  }

  scoped_ptr<PasswordFormManager> manager;
  ScopedVector<PasswordFormManager>::iterator matched_manager_it =
      pending_login_managers_.end();
  // Below, "matching" is in DoesManage-sense and "not ready" in
  // !HasCompletedMatching sense. We keep track of such PasswordFormManager
  // instances for UMA.
  bool has_found_matching_managers_which_were_not_ready = false;
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end();
       ++iter) {
    PasswordFormManager::MatchResultMask result = (*iter)->DoesManage(form);

    if (!(*iter)->HasCompletedMatching()) {
      if (result != PasswordFormManager::RESULT_NO_MATCH)
        has_found_matching_managers_which_were_not_ready = true;
      continue;
    }

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      if (logger)
        logger->LogMessage(Logger::STRING_EXACT_MATCH);
      matched_manager_it = iter;
      break;
    } else if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                          ~PasswordFormManager::RESULT_ACTION_MATCH)) {
      // If the current manager matches the submitted form excluding the action
      // URL, remember it as a candidate and continue searching for an exact
      // match. See http://crbug.com/27246 for an example where actions can
      // change.
      if (logger)
        logger->LogMessage(Logger::STRING_MATCH_WITHOUT_ACTION);
      matched_manager_it = iter;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (matched_manager_it != pending_login_managers_.end()) {
    // Transfer ownership of the manager from |pending_login_managers_| to
    // |manager|.
    manager.reset(*matched_manager_it);
    pending_login_managers_.weak_erase(matched_manager_it);
  } else if (has_found_matching_managers_which_were_not_ready) {
    // We found some managers, but none finished matching yet. The user has
    // tried to submit credentials before we had time to even find matching
    // results for the given form and autofill. If this is the case, we just
    // give up.
    RecordFailure(MATCHING_NOT_COMPLETE, form.origin.host(), logger.get());
    return;
  } else {
    RecordFailure(NO_MATCHING_FORM, form.origin.host(), logger.get());
    return;
  }

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted()) {
    RecordFailure(FORM_BLACKLISTED, form.origin.host(), logger.get());
    return;
  }

  // Bail if we're missing any of the necessary form components.
  if (!manager->HasValidPasswordForm()) {
    RecordFailure(INVALID_FORM, form.origin.host(), logger.get());
    return;
  }

  // Don't save credentials for the syncing account. See crbug.com/365832 for
  // background.
  if (ShouldDropSyncCredential() &&
      client_->IsSyncAccountCredential(
          base::UTF16ToUTF8(form.username_value), form.signon_realm)) {
    RecordFailure(SYNC_CREDENTIAL, form.origin.host(), logger.get());
    return;
  }

  // Always save generated passwords, as the user expresses explicit intent for
  // Chrome to manage such passwords. For other passwords, respect the
  // autocomplete attribute if autocomplete='off' is not ignored.
  if (!autofill::ShouldIgnoreAutocompleteOffForPasswordFields() &&
      !manager->HasGeneratedPassword() && !form.password_autocomplete_set) {
    RecordFailure(AUTOCOMPLETE_OFF, form.origin.host(), logger.get());
    return;
  }

  PasswordForm provisionally_saved_form(form);
  provisionally_saved_form.ssl_valid =
      form.origin.SchemeIsSecure() &&
      !driver_->DidLastPageLoadEncounterSSLErrors();
  provisionally_saved_form.preferred = true;
  if (logger) {
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVED_FORM,
                            provisionally_saved_form);
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
  manager->ProvisionallySave(provisionally_saved_form, action);
  provisional_save_manager_.swap(manager);
}

void PasswordManager::RecordFailure(ProvisionalSaveFailure failure,
                                    const std::string& form_origin,
                                    BrowserSavePasswordProgressLogger* logger) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ProvisionalSaveFailure", failure, MAX_FAILURE_VALUE);

  std::string group_name = metrics_util::GroupIdToString(
      metrics_util::MonitoredDomainGroupId(form_origin, client_->GetPrefs()));
  if (!group_name.empty()) {
    metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.ProvisionalSaveFailure_" + group_name,
        failure,
        MAX_FAILURE_VALUE);
  }

  if (logger) {
    switch (failure) {
      case SAVING_DISABLED:
        logger->LogMessage(Logger::STRING_SAVING_DISABLED);
        break;
      case EMPTY_PASSWORD:
        logger->LogMessage(Logger::STRING_EMPTY_PASSWORD);
        break;
      case MATCHING_NOT_COMPLETE:
        logger->LogMessage(Logger::STRING_MATCHING_NOT_COMPLETE);
        break;
      case NO_MATCHING_FORM:
        logger->LogMessage(Logger::STRING_NO_MATCHING_FORM);
        break;
      case FORM_BLACKLISTED:
        logger->LogMessage(Logger::STRING_FORM_BLACKLISTED);
        break;
      case INVALID_FORM:
        logger->LogMessage(Logger::STRING_INVALID_FORM);
        break;
      case AUTOCOMPLETE_OFF:
        logger->LogMessage(Logger::STRING_AUTOCOMPLETE_OFF);
        break;
      case SYNC_CREDENTIAL:
        logger->LogMessage(Logger::STRING_SYNC_CREDENTIAL);
        break;
      case MAX_FAILURE_VALUE:
        NOTREACHED();
        return;
    }
    logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
}

void PasswordManager::AddSubmissionCallback(
    const PasswordSubmittedCallback& callback) {
  submission_callbacks_.push_back(callback);
}

void PasswordManager::AddObserver(LoginModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PasswordManager::RemoveObserver(LoginModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordManager::DidNavigateMainFrame(bool is_in_page) {
  // Clear data after main frame navigation if the navigation was to a
  // different page.
  if (!is_in_page) {
    pending_login_managers_.clear();
    driver_->GetPasswordAutofillManager()->Reset();
  }
}

void PasswordManager::OnPasswordFormSubmitted(
    const PasswordForm& password_form) {
  ProvisionallySavePassword(password_form);
  for (size_t i = 0; i < submission_callbacks_.size(); ++i) {
    submission_callbacks_[i].Run(password_form);
  }

  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormsParsed(
    const std::vector<PasswordForm>& forms) {
  CreatePendingLoginManagers(forms);
}

void PasswordManager::CreatePendingLoginManagers(
    const std::vector<PasswordForm>& forms) {
  if (!IsEnabledForCurrentPage())
    return;

  // Copy the weak pointers to the currently known login managers for comparison
  // against the newly added.
  std::vector<PasswordFormManager*> old_login_managers(
      pending_login_managers_.get());
  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end();
       ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (EndsWith(iter->signon_realm, kSpdyProxyRealm, true))
      continue;
    bool old_manager_found = false;
    for (std::vector<PasswordFormManager*>::const_iterator old_manager =
             old_login_managers.begin();
         !old_manager_found && old_manager != old_login_managers.end();
         ++old_manager) {
      old_manager_found = (*old_manager)->DoesManage(*iter) ==
                          PasswordFormManager::RESULT_COMPLETE_MATCH;
    }
    if (old_manager_found)
      continue;  // The current form is already managed.

    bool ssl_valid = iter->origin.SchemeIsSecure();
    PasswordFormManager* manager =
        new PasswordFormManager(this, client_, driver_, *iter, ssl_valid);
    pending_login_managers_.push_back(manager);

    PasswordStore::AuthorizationPromptPolicy prompt_policy =
        client_->GetAuthorizationPromptPolicy(*iter);

    manager->FetchMatchingLoginsFromPasswordStore(prompt_policy);
  }
}

bool PasswordManager::ShouldPromptUserToSavePassword() const {
  return !client_->IsAutomaticPasswordSavingEnabled() &&
         provisional_save_manager_->IsNewLogin() &&
         !provisional_save_manager_->HasGeneratedPassword() &&
         !provisional_save_manager_->IsPendingCredentialsPublicSuffixMatch();
}

void PasswordManager::OnPasswordFormsRendered(
    const std::vector<PasswordForm>& visible_forms,
    bool did_stop_loading) {
  CreatePendingLoginManagers(visible_forms);
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD);
  }

  if (!provisional_save_manager_.get()) {
    if (logger) {
      logger->LogMessage(Logger::STRING_NO_PROVISIONAL_SAVE_MANAGER);
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    }
    return;
  }

  DCHECK(IsSavingEnabledForCurrentPage());

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
    for (size_t i = 0; i < all_visible_forms_.size(); ++i) {
      // TODO(vabr): The similarity check is just action equality up to
      // HTTP<->HTTPS substitution for now. If it becomes more complex, it may
      // make sense to consider modifying and using
      // PasswordFormManager::DoesManage for it.
      if (all_visible_forms_[i].action.is_valid() &&
          URLsEqualUpToHttpHttpsSubstitution(
              provisional_save_manager_->pending_credentials().action,
              all_visible_forms_[i].action)) {
        if (logger) {
          logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                                  visible_forms[i]);
          logger->LogMessage(Logger::STRING_DECISION_DROP);
        }
        provisional_save_manager_->SubmitFailed();
        provisional_save_manager_.reset();
        // Clear all_visible_forms_ once we found the match.
        all_visible_forms_.clear();
        return;
      }
    }

    // Clear all_visible_forms_ after checking all the visible forms.
    all_visible_forms_.clear();

    // Looks like a successful login attempt. Either show an infobar or
    // automatically save the login data. We prompt when the user hasn't
    // already given consent, either through previously accepting the infobar
    // or by having the browser generate the password.
    provisional_save_manager_->SubmitPassed();

    if (ShouldPromptUserToSavePassword()) {
      if (logger)
        logger->LogMessage(Logger::STRING_DECISION_ASK);
      if (client_->PromptUserToSavePassword(provisional_save_manager_.Pass())) {
        if (logger)
          logger->LogMessage(Logger::STRING_SHOW_PASSWORD_PROMPT);
      }
    } else {
      if (logger)
        logger->LogMessage(Logger::STRING_DECISION_SAVE);
      provisional_save_manager_->Save();

      if (provisional_save_manager_->HasGeneratedPassword()) {
        client_->AutomaticPasswordSave(provisional_save_manager_.Pass());
      } else {
        provisional_save_manager_.reset();
      }
    }
  }
}

void PasswordManager::PossiblyInitializeUsernamesExperiment(
    const PasswordFormMap& best_matches) const {
  if (base::FieldTrialList::Find(kOtherPossibleUsernamesExperiment))
    return;

  bool other_possible_usernames_exist = false;
  for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
       it != best_matches.end();
       ++it) {
    if (!it->second->other_possible_usernames.empty()) {
      other_possible_usernames_exist = true;
      break;
    }
  }

  if (!other_possible_usernames_exist)
    return;

  const base::FieldTrial::Probability kDivisor = 100;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kOtherPossibleUsernamesExperiment,
          kDivisor,
          "Disabled",
          2013, 12, 31,
          base::FieldTrial::ONE_TIME_RANDOMIZED,
          NULL));
  base::FieldTrial::Probability enabled_probability =
      client_->GetProbabilityForExperiment(kOtherPossibleUsernamesExperiment);
  trial->AppendGroup("Enabled", enabled_probability);
}

bool PasswordManager::OtherPossibleUsernamesEnabled() const {
  return base::FieldTrialList::FindFullName(
             kOtherPossibleUsernamesExperiment) == "Enabled";
}

void PasswordManager::Autofill(const PasswordForm& form_for_autofill,
                               const PasswordFormMap& best_matches,
                               const PasswordForm& preferred_match,
                               bool wait_for_username) const {
  PossiblyInitializeUsernamesExperiment(best_matches);

  // TODO(tedchoc): Switch to only requesting authentication if the user is
  //                acting on the autofilled forms (crbug.com/342594) instead
  //                of on page load.
  bool authentication_required = preferred_match.use_additional_authentication;
  for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
       !authentication_required && it != best_matches.end();
       ++it) {
    if (it->second->use_additional_authentication)
      authentication_required = true;
  }

  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observers_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      scoped_ptr<autofill::PasswordFormFillData> fill_data(
          new autofill::PasswordFormFillData());
      InitPasswordFormFillData(form_for_autofill,
                               best_matches,
                               &preferred_match,
                               wait_for_username,
                               OtherPossibleUsernamesEnabled(),
                               fill_data.get());
      if (authentication_required)
        client_->AuthenticateAutofillAndFillForm(fill_data.Pass());
      else
        driver_->FillPasswordForm(*fill_data.get());
      break;
    }
    default:
      FOR_EACH_OBSERVER(
          LoginModelObserver,
          observers_,
          OnAutofillDataAvailable(preferred_match.username_value,
                                  preferred_match.password_value));
      break;
  }

  client_->PasswordWasAutofilled(best_matches);
}

}  // namespace password_manager
