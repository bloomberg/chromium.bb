// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {
bool PreferredRealmIsFromAndroid(const PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(fill_data.preferred_realm)
      .IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(const PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(login.second.realm)
            .IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

bool ShouldShowInitialPasswordAccountSuggestions() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kFillOnAccountSelect);
}

void Autofill(const PasswordManagerClient& client,
              PasswordManagerDriver* driver,
              const PasswordForm& form_for_autofill,
              const std::map<base::string16, const PasswordForm*>& best_matches,
              const std::vector<const PasswordForm*>& federated_matches,
              const PasswordForm& preferred_match,
              bool wait_for_username) {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(&client)) {
    logger.reset(new BrowserSavePasswordProgressLogger(client.GetLogManager()));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }

  autofill::PasswordFormFillData fill_data;
  InitPasswordFormFillData(form_for_autofill, best_matches, &preferred_match,
                           wait_for_username, &fill_data);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
      ContainsAndroidCredentials(fill_data));
  metrics_util::LogFilledCredentialIsFromAndroidApp(
      PreferredRealmIsFromAndroid(fill_data));
  driver->FillPasswordForm(fill_data);

  client.PasswordWasAutofilled(best_matches, form_for_autofill.origin,
                               &federated_matches);
}

void ShowInitialPasswordAccountSuggestions(
    const PasswordManagerClient& client,
    PasswordManagerDriver* driver,
    const PasswordForm& form_for_autofill,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(&client)) {
    logger.reset(new BrowserSavePasswordProgressLogger(client.GetLogManager()));
    logger->LogMessage(
        Logger::
            STRING_PASSWORDMANAGER_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS);
  }

  PasswordFormFillData fill_data;
  InitPasswordFormFillData(form_for_autofill, best_matches, &preferred_match,
                           wait_for_username, &fill_data);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  driver->ShowInitialPasswordAccountSuggestions(fill_data);
}

// Returns true if filling the |form| is likely to be useful for the user.
bool FormGoodForFilling(const PasswordForm& form) {
  return !form.password_element.empty() ||
         form.password_element_renderer_id !=
             autofill::FormFieldData::kNotSetFormControlRendererId;
}

}  // namespace

void SendFillInformationToRenderer(
    const PasswordManagerClient& client,
    PasswordManagerDriver* driver,
    bool is_blacklisted,
    const PasswordForm& observed_form,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<const PasswordForm*>& federated_matches,
    const PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder) {
  DCHECK(driver);
  DCHECK_EQ(PasswordForm::SCHEME_HTML, observed_form.scheme);

  if (!is_blacklisted)
    driver->AllowPasswordGenerationForForm(observed_form);

  if (best_matches.empty()) {
    driver->InformNoSavedCredentials();
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventNoCredential);
    return;
  }
  DCHECK(preferred_match);

  // Chrome tries to avoid filling into fields where the user is asked to enter
  // a fresh password. The old condition for filling on load was: "does the form
  // lack a new-password field?" There is currently a discussion whether this
  // should rather be: "does the form have a current-password field?" because
  // the current-password field is what should be filled. Currently, the old
  // condition is still used with the old parser, and the new condition with the
  // new one.
  // Old condition.
  const bool did_fill_on_load = !observed_form.IsPossibleChangePasswordForm();
  // New condition.
  const bool will_fill_on_load = FormGoodForFilling(observed_form);
  const bool form_good_for_filling =
      base::FeatureList::IsEnabled(features::kNewPasswordFormParsing)
          ? will_fill_on_load
          : did_fill_on_load;

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, or
  // (2) if it matched using public suffix domain matching, or
  // (3) the form is change password form.
  bool wait_for_username = client.IsIncognito() ||
                           preferred_match->is_public_suffix_match ||
                           !form_good_for_filling;

  // The following metric is only relevant when fill on load is not suppressed
  // by Incognito or PSL-matched credentials. It is also only relevant for the
  // new parser, because the tested change will only be launched for that.
  // TODO(crbug.com/895781): Remove once the password manager team decides
  // whether to go with the new condition or the old one.
  if (!client.IsIncognito() && !preferred_match->is_public_suffix_match &&
      base::FeatureList::IsEnabled(features::kNewPasswordFormParsing)) {
    PasswordFormMetricsRecorder::FillOnLoad fill_on_load_result =
        PasswordFormMetricsRecorder::FillOnLoad::kSame;
    // Note: The fill on load never happens if |will_fill_on_load| is false,
    // because PasswordAutofillAgent won't be able to locate the "current
    // password" field to fill. So even if |did_fill_on_load| is true and
    // |will_fill_on_load| is false, the behaviour of Chrome does not change if
    // either of them is picked for |form_good_for_filling|. So the only
    // interesting case to report is when |did...| is false and |will...| is
    // true.
    if (!did_fill_on_load && will_fill_on_load) {
      fill_on_load_result =
          PasswordFormMetricsRecorder::FillOnLoad::kStartsFillingOnLoad;
    }
    metrics_recorder->RecordFillOnLoad(fill_on_load_result);
  }

  if (wait_for_username) {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionNone);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventBlockedOnInteraction);
  } else {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionAutofilled);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventAutofilled);
    base::RecordAction(base::UserMetricsAction("PasswordManager_Autofilled"));
  }
  if (ShouldShowInitialPasswordAccountSuggestions()) {
    // This is for the fill-on-account-select experiment. Instead of autofilling
    // found usernames and passwords on load, this instructs the renderer to
    // return with any found password forms so a list of password account
    // suggestions can be drawn.
    ShowInitialPasswordAccountSuggestions(client, driver, observed_form,
                                          best_matches, *preferred_match,
                                          wait_for_username);
  } else {
    // If fill-on-account-select is not enabled, continue with autofilling any
    // password forms as traditionally has been done.
    Autofill(client, driver, observed_form, best_matches, federated_matches,
             *preferred_match, wait_for_username);
  }
}

}  // namespace password_manager
