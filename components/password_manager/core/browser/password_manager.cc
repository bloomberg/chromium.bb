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
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

using autofill::PasswordForm;
using autofill::PasswordFormMap;

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

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

}  // namespace

const char PasswordManager::kOtherPossibleUsernamesExperiment[] =
    "PasswordManagerOtherPossibleUsernames";

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerEnabled,
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
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
                                 client_->GetPrefs());

  ReportMetrics(*password_manager_enabled_);
}

PasswordManager::~PasswordManager() {
  FOR_EACH_OBSERVER(LoginModelObserver, observers_, OnLoginModelDestroying());
}

void PasswordManager::SetFormHasGeneratedPassword(const PasswordForm& form) {
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      (*iter)->SetHasGeneratedPassword();
      return;
    }
  }
  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  bool ssl_valid = (form.origin.SchemeIsSecure() &&
                    !driver_->DidLastPageLoadEncounterSSLErrors());
  PasswordFormManager* manager = new PasswordFormManager(
      this, client_, driver_, form, ssl_valid);
  pending_login_managers_.push_back(manager);
  manager->SetHasGeneratedPassword();
  // TODO(gcasto): Add UMA stats to track this.
}

bool PasswordManager::IsSavingEnabled() const {
  return *password_manager_enabled_ && !driver_->IsOffTheRecord();
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  if (!IsSavingEnabled()) {
    RecordFailure(SAVING_DISABLED, form.origin.host());
    return;
  }

  // No password to save? Then don't.
  if (form.password_value.empty()) {
    RecordFailure(EMPTY_PASSWORD, form.origin.host());
    return;
  }

  scoped_ptr<PasswordFormManager> manager;
  ScopedVector<PasswordFormManager>::iterator matched_manager_it =
      pending_login_managers_.end();
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    // If we find a manager that exactly matches the submitted form including
    // the action URL, exit the loop.
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      matched_manager_it = iter;
      break;
    // If the current manager matches the submitted form excluding the action
    // URL, remember it as a candidate and continue searching for an exact
    // match.
    } else if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_NOT_REQUIRED)) {
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
  } else {
    RecordFailure(NO_MATCHING_FORM, form.origin.host());
    return;
  }

  // If we found a manager but it didn't finish matching yet, the user has
  // tried to submit credentials before we had time to even find matching
  // results for the given form and autofill. If this is the case, we just
  // give up.
  if (!manager->HasCompletedMatching()) {
    RecordFailure(MATCHING_NOT_COMPLETE, form.origin.host());
    return;
  }

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted()) {
    RecordFailure(FORM_BLACKLISTED, form.origin.host());
    return;
  }

  // Bail if we're missing any of the necessary form components.
  if (!manager->HasValidPasswordForm()) {
    RecordFailure(INVALID_FORM, form.origin.host());
    return;
  }

  // Always save generated passwords, as the user expresses explicit intent for
  // Chrome to manage such passwords. For other passwords, respect the
  // autocomplete attribute if autocomplete='off' is not ignored.
  if (!autofill::ShouldIgnoreAutocompleteOffForPasswordFields() &&
      !manager->HasGeneratedPassword() &&
      !form.password_autocomplete_set) {
    RecordFailure(AUTOCOMPLETE_OFF, form.origin.host());
    return;
  }

  PasswordForm provisionally_saved_form(form);
  provisionally_saved_form.ssl_valid =
      form.origin.SchemeIsSecure() &&
      !driver_->DidLastPageLoadEncounterSSLErrors();
  provisionally_saved_form.preferred = true;
  PasswordFormManager::OtherPossibleUsernamesAction action =
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES;
  if (OtherPossibleUsernamesEnabled())
    action = PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES;
  manager->ProvisionallySave(provisionally_saved_form, action);
  provisional_save_manager_.swap(manager);
}

void PasswordManager::RecordFailure(ProvisionalSaveFailure failure,
                                    const std::string& form_origin) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ProvisionalSaveFailure",
                            failure, MAX_FAILURE_VALUE);

  std::string group_name = password_manager_metrics_util::GroupIdToString(
      password_manager_metrics_util::MonitoredDomainGroupId(
          form_origin, client_->GetPrefs()));
  if (!group_name.empty()) {
    password_manager_metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.ProvisionalSaveFailure_" + group_name, failure,
        MAX_FAILURE_VALUE);
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
  if (!is_in_page)
    pending_login_managers_.clear();
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
  // Ask the SSLManager for current security.
  bool had_ssl_error = driver_->DidLastPageLoadEncounterSSLErrors();

  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (EndsWith(iter->signon_realm, kSpdyProxyRealm, true))
      continue;

    bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
    PasswordFormManager* manager = new PasswordFormManager(
        this, client_, driver_, *iter, ssl_valid);
    pending_login_managers_.push_back(manager);

    // Avoid prompting the user for access to a password if they don't have
    // password saving enabled.
    PasswordStore::AuthorizationPromptPolicy prompt_policy =
        *password_manager_enabled_ ? PasswordStore::ALLOW_PROMPT
                                   : PasswordStore::DISALLOW_PROMPT;

    manager->FetchMatchingLoginsFromPasswordStore(prompt_policy);
  }
}

bool PasswordManager::ShouldPromptUserToSavePassword() const {
  return provisional_save_manager_->IsNewLogin() &&
         !provisional_save_manager_->HasGeneratedPassword() &&
         !provisional_save_manager_->IsPendingCredentialsPublicSuffixMatch();
}

void PasswordManager::OnPasswordFormsRendered(
    const std::vector<PasswordForm>& visible_forms) {
  if (!provisional_save_manager_.get())
    return;

  DCHECK(IsSavingEnabled());

  // If we see the login form again, then the login failed.
  for (size_t i = 0; i < visible_forms.size(); ++i) {
    // TODO(vabr): The similarity check is just action equality for now. If it
    // becomes more complex, it may make sense to consider modifying and using
    // PasswordFormManager::DoesManage for it.
    if (visible_forms[i].action.is_valid() &&
        provisional_save_manager_->pending_credentials().action ==
            visible_forms[i].action) {
      provisional_save_manager_->SubmitFailed();
      provisional_save_manager_.reset();
      return;
    }
  }

  // Looks like a successful login attempt. Either show an infobar or
  // automatically save the login data. We prompt when the user hasn't already
  // given consent, either through previously accepting the infobar or by having
  // the browser generate the password.
  provisional_save_manager_->SubmitPassed();

  if (ShouldPromptUserToSavePassword()) {
    client_->PromptUserToSavePassword(provisional_save_manager_.release());
  } else {
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::PossiblyInitializeUsernamesExperiment(
    const PasswordFormMap& best_matches) const {
  if (base::FieldTrialList::Find(kOtherPossibleUsernamesExperiment))
    return;

  bool other_possible_usernames_exist = false;
  for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
       it != best_matches.end(); ++it) {
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

void PasswordManager::Autofill(
    const PasswordForm& form_for_autofill,
    const PasswordFormMap& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) const {
  PossiblyInitializeUsernamesExperiment(best_matches);

  // TODO(tedchoc): Switch to only requesting authentication if the user is
  //                acting on the autofilled forms (crbug.com/342594) instead
  //                of on page load.
  bool authentication_required = preferred_match.use_additional_authentication;
  for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
       !authentication_required && it != best_matches.end(); ++it) {
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
