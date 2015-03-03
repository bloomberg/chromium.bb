// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <algorithm>
#include <set>

#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"

using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::PasswordFormMap;
using base::Time;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace password_manager {

namespace {

enum PasswordGenerationSubmissionEvent {
  // Generated password was submitted and saved.
  PASSWORD_SUBMITTED,

  // Generated password submission failed. These passwords aren't saved.
  PASSWORD_SUBMISSION_FAILED,

  // Generated password was not submitted before navigation. Currently these
  // passwords are not saved.
  PASSWORD_NOT_SUBMITTED,

  // Generated password was overridden by a non-generated one. This generally
  // signals that the user was unhappy with the generated password for some
  // reason.
  PASSWORD_OVERRIDDEN,

  // Number of enum entries, used for UMA histogram reporting macros.
  SUBMISSION_EVENT_ENUM_COUNT
};

void LogPasswordGenerationSubmissionEvent(
    PasswordGenerationSubmissionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.SubmissionEvent",
                            event, SUBMISSION_EVENT_ENUM_COUNT);
}

PasswordForm CopyAndModifySSLValidity(const PasswordForm& orig,
                                      bool ssl_valid) {
  PasswordForm result(orig);
  result.ssl_valid = ssl_valid;
  return result;
}

// Returns true if user-typed username and password field values match with one
// of the password form within |credentials| map; otherwise false.
bool DoesUsenameAndPasswordMatchCredentials(
    const base::string16& typed_username,
    const base::string16& typed_password,
    const autofill::PasswordFormMap& credentials) {
  for (auto match : credentials) {
    if (match.second->username_value == typed_username &&
        match.second->password_value == typed_password)
      return true;
  }
  return false;
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManager* password_manager,
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const PasswordForm& observed_form,
    bool ssl_valid)
    : best_matches_deleter_(&best_matches_),
      observed_form_(CopyAndModifySSLValidity(observed_form, ssl_valid)),
      is_new_login_(true),
      has_generated_password_(false),
      password_manager_(password_manager),
      preferred_match_(nullptr),
      state_(PRE_MATCHING_PHASE),
      client_(client),
      manager_action_(kManagerActionNone),
      user_action_(kUserActionNone),
      submit_result_(kSubmitResultNotSubmitted) {
  drivers_.push_back(driver);
  if (observed_form_.origin.is_valid())
    base::SplitString(observed_form_.origin.path(), '/', &form_path_tokens_);
}

PasswordFormManager::~PasswordFormManager() {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ActionsTakenV3", GetActionsTaken(), kMaxNumActionsTaken);
  if (has_generated_password_ && submit_result_ == kSubmitResultNotSubmitted)
    LogPasswordGenerationSubmissionEvent(PASSWORD_NOT_SUBMITTED);
}

int PasswordFormManager::GetActionsTaken() const {
  return user_action_ +
         kUserActionMax *
             (manager_action_ + kManagerActionMax * submit_result_);
}

// TODO(timsteele): use a hash of some sort in the future?
PasswordFormManager::MatchResultMask PasswordFormManager::DoesManage(
    const PasswordForm& form) const {
  // Non-HTML form case.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML ||
      form.scheme != PasswordForm::SCHEME_HTML) {
    const bool forms_match = observed_form_.signon_realm == form.signon_realm &&
                             observed_form_.scheme == form.scheme;
    return forms_match ? RESULT_COMPLETE_MATCH : RESULT_NO_MATCH;
  }

  // HTML form case.
  MatchResultMask result = RESULT_NO_MATCH;

  // Easiest case of matching origins.
  bool origins_match = form.origin == observed_form_.origin;
  // If this is a replay of the same form in the case a user entered an invalid
  // password, the origin of the new form may equal the action of the "first"
  // form instead.
  origins_match = origins_match || (form.origin == observed_form_.action);
  // Otherwise, if action hosts are the same, the old URL scheme is HTTP while
  // the new one is HTTPS, and the new path equals to or extends the old path,
  // we also consider the actions a match. This is to accommodate cases where
  // the original login form is on an HTTP page, but a failed login attempt
  // redirects to HTTPS (as in http://example.org -> https://example.org/auth).
  if (!origins_match && !observed_form_.origin.SchemeIsSecure() &&
      form.origin.SchemeIsSecure()) {
    const std::string& old_path = observed_form_.origin.path();
    const std::string& new_path = form.origin.path();
    origins_match =
        observed_form_.origin.host() == form.origin.host() &&
        observed_form_.origin.port() == form.origin.port() &&
        StartsWithASCII(new_path, old_path, /*case_sensitive=*/true);
  }

  if (!origins_match)
    return result;

  result |= RESULT_ORIGINS_MATCH;

  if (form.username_element == observed_form_.username_element &&
      form.password_element == observed_form_.password_element) {
    result |= RESULT_HTML_ATTRIBUTES_MATCH;
  }

  // Note: although saved password forms might actually have an empty action
  // URL if they were imported (see bug 1107719), the |form| we see here comes
  // never from the password store, and should have an exactly matching action.
  if (form.action == observed_form_.action)
    result |= RESULT_ACTION_MATCH;

  return result;
}

bool PasswordFormManager::IsBlacklisted() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  if (preferred_match_ && preferred_match_->blacklisted_by_user)
    return true;
  return false;
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);

  // Configure the form about to be saved for blacklist status.
  pending_credentials_.preferred = true;
  pending_credentials_.blacklisted_by_user = true;
  pending_credentials_.username_value.clear();
  pending_credentials_.password_value.clear();

  // Retroactively forget existing matches for this form, so we NEVER prompt or
  // autofill it again.
  int num_passwords_deleted = 0;
  if (!best_matches_.empty()) {
    PasswordFormMap::const_iterator iter;
    PasswordStore* password_store = client_->GetPasswordStore();
    if (!password_store) {
      NOTREACHED();
      return;
    }
    for (iter = best_matches_.begin(); iter != best_matches_.end(); ++iter) {
      // We want to remove existing matches for this form so that the exact
      // origin match with |blackisted_by_user == true| is the only result that
      // shows up in the future for this origin URL. However, we don't want to
      // delete logins that were actually saved on a different page (hence with
      // different origin URL) and just happened to match this form because of
      // the scoring algorithm. See bug 1204493.
      if (iter->second->origin == observed_form_.origin) {
        password_store->RemoveLogin(*iter->second);
        ++num_passwords_deleted;
      }
    }
  }

  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedWhenBlacklisting",
                       num_passwords_deleted);

  // Save the pending_credentials_ entry marked as blacklisted.
  SaveAsNewLogin(false);
}

bool PasswordFormManager::IsNewLogin() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return is_new_login_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.IsPublicSuffixMatch();
}

void PasswordFormManager::SetHasGeneratedPassword() {
  has_generated_password_ = true;
}

bool PasswordFormManager::HasGeneratedPassword() const {
  // This check is permissive, as the user may have generated a password and
  // then edited it in the form itself. However, even in this case the user
  // has already given consent, so we treat these cases the same.
  return has_generated_password_;
}

bool PasswordFormManager::HasValidPasswordForm() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  // Non-HTML password forms (primarily HTTP and FTP autentication)
  // do not contain username_element and password_element values.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML)
    return true;
  return !observed_form_.password_element.empty() ||
         !observed_form_.new_password_element.empty();
}

void PasswordFormManager::ProvisionallySave(
    const PasswordForm& credentials,
    OtherPossibleUsernamesAction action) {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK_NE(RESULT_NO_MATCH, DoesManage(credentials));

  // If this was a sign-up or change password form, we want to persist the new
  // password; if this was a login form, then the current password (which might
  // still be "new" in the sense that we see these credentials for the first
  // time, or that the user manually entered his actual password to overwrite an
  // obsolete password we had in the store).
  base::string16 password_to_save(credentials.new_password_element.empty() ?
      credentials.password_value : credentials.new_password_value);

  // Make sure the important fields stay the same as the initially observed or
  // autofilled ones, as they may have changed if the user experienced a login
  // failure.
  // Look for these credentials in the list containing auto-fill entries.
  PasswordFormMap::const_iterator it =
      best_matches_.find(credentials.username_value);
  if (it != best_matches_.end()) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *it->second;
    bool password_changed =
        pending_credentials_.password_value != password_to_save;
    if (IsPendingCredentialsPublicSuffixMatch()) {
      // If the autofilled credentials were only a PSL match, store a copy with
      // the current origin and signon realm. This ensures that on the next
      // visit, a precise match is found.
      is_new_login_ = true;
      user_action_ = password_changed ? kUserActionChoosePslMatch
                                      : kUserActionOverridePassword;

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      //
      // TODO(gcasto): It would be nice if other state were shared such that if
      // say a password was updated on one match it would update on all related
      // passwords. This is a much larger change.
      UpdateMetadataForUsage(pending_credentials_);

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |credentials| actually correspond
      // to two different accounts (see http://crbug.com/385619). In that case
      // the user should be asked again before saving the password. This is
      // ensured by clearing |original_signon_realm| on |pending_credentials_|,
      // which unmarks it as a PSL match.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |credentials| but the stored password was out of
      // date. In that case, the user just had to manually enter the new
      // password, which is now in |credentials|. The best thing would be to
      // save automatically, and also update the original credentials. However,
      // we have no way to tell if this is the case. This will likely happen
      // infrequently, and the inconvenience put on the user by asking them is
      // not significant, so we are fine with asking here again.
      if (password_changed) {
        pending_credentials_.original_signon_realm.clear();
        DCHECK(!IsPendingCredentialsPublicSuffixMatch());
      }
    } else {  // Not a PSL match.
      is_new_login_ = false;
      if (password_changed)
        user_action_ = kUserActionOverridePassword;
    }
  } else if (action == ALLOW_OTHER_POSSIBLE_USERNAMES &&
             UpdatePendingCredentialsIfOtherPossibleUsername(
                 credentials.username_value)) {
    // |pending_credentials_| is now set. Note we don't update
    // |pending_credentials_.username_value| to |credentials.username_value|
    // yet because we need to keep the original username to modify the stored
    // credential.
    selected_username_ = credentials.username_value;
    is_new_login_ = false;
  } else {
    // User typed in a new, unknown username.
    user_action_ = kUserActionOverrideUsernameAndPassword;
    pending_credentials_ = observed_form_;
    pending_credentials_.username_value = credentials.username_value;
    pending_credentials_.other_possible_usernames =
        credentials.other_possible_usernames;

    // The password value will be filled in later, remove any garbage for now.
    pending_credentials_.password_value.clear();
    pending_credentials_.new_password_value.clear();

    // If this was a sign-up or change password form, the names of the elements
    // are likely different than those on a login form, so do not bother saving
    // them. We will fill them with meaningful values in UpdateLogin() when the
    // user goes onto a real login form for the first time.
    if (!credentials.new_password_element.empty()) {
      pending_credentials_.password_element.clear();
      pending_credentials_.new_password_element.clear();
    }
  }

  pending_credentials_.action = credentials.action;
  // If the user selected credentials we autofilled from a PasswordForm
  // that contained no action URL (IE6/7 imported passwords, for example),
  // bless it with the action URL from the observed form. See bug 1107719.
  if (pending_credentials_.action.is_empty())
    pending_credentials_.action = observed_form_.action;

  pending_credentials_.password_value = password_to_save;
  pending_credentials_.preferred = credentials.preferred;

  if (user_action_ == kUserActionOverridePassword &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED &&
      !has_generated_password_) {
    LogPasswordGenerationSubmissionEvent(PASSWORD_OVERRIDDEN);
  }

  if (has_generated_password_)
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;
}

void PasswordFormManager::Save() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!client_->IsOffTheRecord());

  if (IsNewLogin())
    SaveAsNewLogin(true);
  else
    UpdateLogin();
}

void PasswordFormManager::FetchMatchingLoginsFromPasswordStore(
    PasswordStore::AuthorizationPromptPolicy prompt_policy) {
  DCHECK_EQ(state_, PRE_MATCHING_PHASE);
  state_ = MATCHING_PHASE;

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_FETCH_LOGINS_METHOD);
  }

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  password_store->GetLogins(observed_form_, prompt_policy, this);
}

bool PasswordFormManager::HasCompletedMatching() const {
  return state_ == POST_MATCHING_PHASE;
}

bool PasswordFormManager::IsIgnorableChangePasswordForm(
    const PasswordForm& form) const {
  bool is_change_password_form =
      !form.new_password_element.empty() && !form.password_element.empty();
  return is_change_password_form && !form.username_marked_by_site &&
         !DoesUsenameAndPasswordMatchCredentials(
             form.username_value, form.password_value, best_matches_);
}

void PasswordFormManager::OnRequestDone(
    ScopedVector<PasswordForm> logins_result) {
  preferred_match_ = nullptr;
  STLDeleteValues(&best_matches_);
  const size_t logins_result_size = logins_result.size();

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_REQUEST_DONE_METHOD);
  }

  // First, compute scores for credentials in |login_result|.
  std::vector<int> credential_scores;
  credential_scores.reserve(logins_result.size());
  int best_score = 0;
  for (const PasswordForm* login : logins_result) {
    if (ShouldIgnoreResult(*login)) {
      credential_scores.push_back(-1);
      continue;
    }

    int current_score = ScoreResult(*login);
    if (current_score > best_score)
      best_score = current_score;
    credential_scores.push_back(current_score);
  }

  if (best_score <= 0) {
    if (logger)
      logger->LogNumber(Logger::STRING_BEST_SCORE, best_score);
    return;
  }

  // Start the |best_matches_| with the best-scoring normal credentials and save
  // the worse-scoring "protected" ones for later.
  ScopedVector<PasswordForm> protected_credentials;
  for (size_t i = 0; i < logins_result.size(); ++i) {
    if (credential_scores[i] < 0)
      continue;
    if (credential_scores[i] < best_score) {
      // Empty path matches are most commonly imports from Firefox, and
      // generally useful to autofill. Blacklisted entries are only meaningful
      // in the absence of non-blacklisted entries, in which case they need no
      // protection to become |best_matches_|. TODO(timsteele): Bug 1269400. We
      // probably should do something more elegant for any shorter-path match
      // instead of explicitly handling empty path matches.
      bool is_credential_protected =
          observed_form_.scheme == PasswordForm::SCHEME_HTML &&
          StartsWithASCII("/", logins_result[i]->origin.path(), true) &&
          credential_scores[i] > 0 && !logins_result[i]->blacklisted_by_user;
      // Passwords generated on a signup form must show on a login form even if
      // there are better-matching saved credentials. TODO(gcasto): We don't
      // want to cut credentials that were saved on signup forms even if they
      // weren't generated, but currently it's hard to distinguish between those
      // forms and two different login forms on the same domain. Filed
      // http://crbug.com/294468 to look into this.
      is_credential_protected |=
          logins_result[i]->type == PasswordForm::TYPE_GENERATED;

      if (is_credential_protected) {
        protected_credentials.push_back(logins_result[i]);
        logins_result[i] = nullptr;
      }
      continue;
    }

    // If there is another best-score match for the same username, replace it.
    // TODO(vabr): Spare the replacing and keep the first instead of the last
    // candidate.
    auto& best_match = best_matches_[logins_result[i]->username_value];
    if (best_match == preferred_match_)
      preferred_match_ = nullptr;
    delete best_match;

    best_match = logins_result[i];
    logins_result[i] = nullptr;
    preferred_match_ = best_match->preferred ? best_match : preferred_match_;
  }

  // Add the protected results if we don't already have a result with the same
  // username.
  for (auto& protege : protected_credentials) {
    auto& corresponding_best_match = best_matches_[protege->username_value];
    if (!corresponding_best_match) {
      corresponding_best_match = protege;
      protege = nullptr;
    }
  }

  client_->AutofillResultsComputed();

  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsNotShown",
                       logins_result_size - best_matches_.size());

  // It is possible we have at least one match but have no preferred_match_,
  // because a user may have chosen to 'Forget' the preferred match. So we
  // just pick the first one and whichever the user selects for submit will
  // be saved as preferred.
  DCHECK(!best_matches_.empty());
  if (!preferred_match_)
    preferred_match_ = best_matches_.begin()->second;

  // Check to see if the user told us to ignore this site in the past.
  if (preferred_match_->blacklisted_by_user) {
    client_->PasswordAutofillWasBlocked(best_matches_);
    manager_action_ = kManagerActionBlacklisted;
  }
}

void PasswordFormManager::ProcessFrame(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  if (state_ != POST_MATCHING_PHASE) {
    drivers_.push_back(driver);
    return;
  }

  if (!driver || manager_action_ == kManagerActionBlacklisted)
    return;

  // Allow generation for any non-blacklisted form.
  driver->AllowPasswordGenerationForForm(observed_form_);

  if (best_matches_.empty())
    return;

  // Do not autofill on sign-up or change password forms (until we have some
  // working change password functionality).
  if (!observed_form_.new_password_element.empty()) {
    if (client_->IsLoggingActive()) {
      BrowserSavePasswordProgressLogger logger(client_);
      logger.LogMessage(Logger::PROCESS_FRAME_METHOD);
      logger.LogMessage(Logger::STRING_FORM_NOT_AUTOFILLED);
    }
    return;
  }

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, (2) the ACTION paths don't match,
  // or (3) if it matched using public suffix domain matching.
  bool wait_for_username = client_->IsOffTheRecord() ||
                           observed_form_.action.GetWithEmptyPath() !=
                               preferred_match_->action.GetWithEmptyPath() ||
                           preferred_match_->IsPublicSuffixMatch();
  if (wait_for_username)
    manager_action_ = kManagerActionNone;
  else
    manager_action_ = kManagerActionAutofilled;
  password_manager_->Autofill(driver.get(), observed_form_, best_matches_,
                              *preferred_match_, wait_for_username);
}

void PasswordFormManager::OnGetPasswordStoreResults(
    ScopedVector<PasswordForm> results) {
  DCHECK_EQ(state_, MATCHING_PHASE);

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  if (!results.empty())
    OnRequestDone(results.Pass());
  state_ = POST_MATCHING_PHASE;

  if (manager_action_ != kManagerActionBlacklisted) {
    for (auto const& driver : drivers_)
      ProcessFrame(driver);
  }
  drivers_.clear();
}

bool PasswordFormManager::ShouldIgnoreResult(const PasswordForm& form) const {
  // Don't match an invalid SSL form with one saved under secure circumstances.
  if (form.ssl_valid && !observed_form_.ssl_valid)
    return true;

  if (client_->ShouldFilterAutofillResult(form))
    return true;

  return false;
}

void PasswordFormManager::SaveAsNewLogin(bool reset_preferred_login) {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(IsNewLogin());
  // The new_form is being used to sign in, so it is preferred.
  DCHECK(pending_credentials_.preferred);
  // new_form contains the same basic data as observed_form_ (because its the
  // same form), but with the newly added credentials.

  DCHECK(!client_->IsOffTheRecord());

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    NOTREACHED();
    return;
  }

  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Blacklisted credentials will never be used, so don't upload a vote for
  // them. Credentials that have been previously used (e.g. PSL matches) are
  // checked to see if they are valid account creation forms.
  if (!pending_credentials_.blacklisted_by_user) {
    if (pending_credentials_.times_used == 0) {
      UploadPasswordForm(pending_credentials_.form_data, autofill::PASSWORD);
    } else {
      CheckForAccountCreationForm(observed_form_, &pending_credentials_);
    }
  }

  pending_credentials_.date_created = Time::Now();
  SanitizePossibleUsernames(&pending_credentials_);
  password_store->AddLogin(pending_credentials_);

  if (reset_preferred_login) {
    UpdatePreferredLoginState(password_store);
  }
}

void PasswordFormManager::SanitizePossibleUsernames(PasswordForm* form) {
  // Remove any possible usernames that could be credit cards or SSN for privacy
  // reasons. Also remove duplicates, both in other_possible_usernames and
  // between other_possible_usernames and username_value.
  std::set<base::string16> set;
  for (std::vector<base::string16>::iterator it =
           form->other_possible_usernames.begin();
       it != form->other_possible_usernames.end(); ++it) {
    if (!autofill::IsValidCreditCardNumber(*it) && !autofill::IsSSN(*it))
      set.insert(*it);
  }
  set.erase(form->username_value);
  std::vector<base::string16> temp(set.begin(), set.end());
  form->other_possible_usernames.swap(temp);
}

void PasswordFormManager::UpdatePreferredLoginState(
    PasswordStore* password_store) {
  DCHECK(password_store);
  PasswordFormMap::iterator iter;
  for (iter = best_matches_.begin(); iter != best_matches_.end(); iter++) {
    if (iter->second->username_value != pending_credentials_.username_value &&
        iter->second->preferred) {
      // This wasn't the selected login but it used to be preferred.
      iter->second->preferred = false;
      if (user_action_ == kUserActionNone)
        user_action_ = kUserActionChoose;
      password_store->UpdateLogin(*iter->second);
    }
  }
}

void PasswordFormManager::UpdateLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(preferred_match_);
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(!IsNewLogin() && pending_credentials_.preferred);
  DCHECK(!client_->IsOffTheRecord());

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    NOTREACHED();
    return;
  }

  UpdateMetadataForUsage(pending_credentials_);

  if (client_->IsSyncAccountCredential(
          base::UTF16ToUTF8(pending_credentials_.username_value),
          pending_credentials_.signon_realm)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialUsed"));
  }

  // Check to see if this form is a candidate for password generation.
  CheckForAccountCreationForm(observed_form_, &pending_credentials_);

  UpdatePreferredLoginState(password_store);

  // Update the new preferred login.
  if (!selected_username_.empty()) {
    // An other possible username is selected. We set this selected username
    // as the real username. The PasswordStore API isn't designed to update
    // username, so we delete the old credentials and add a new one instead.
    password_store->RemoveLogin(pending_credentials_);
    pending_credentials_.username_value = selected_username_;
    password_store->AddLogin(pending_credentials_);
  } else if ((observed_form_.scheme == PasswordForm::SCHEME_HTML) &&
             (observed_form_.origin.spec().length() >
              observed_form_.signon_realm.length()) &&
             (observed_form_.signon_realm ==
              pending_credentials_.origin.spec())) {
    // Note origin.spec().length > signon_realm.length implies the origin has a
    // path, since signon_realm is a prefix of origin for HTML password forms.
    //
    // The user logged in successfully with one of our autofilled logins on a
    // page with non-empty path, but the autofilled entry was initially saved/
    // imported with an empty path. Rather than just mark this entry preferred,
    // we create a more specific copy for this exact page and leave the "master"
    // unchanged. This is to prevent the case where that master login is used
    // on several sites (e.g site.com/a and site.com/b) but the user actually
    // has a different preference on each site. For example, on /a, he wants the
    // general empty-path login so it is flagged as preferred, but on /b he logs
    // in with a different saved entry - we don't want to remove the preferred
    // status of the former because upon return to /a it won't be the default-
    // fill match.
    // TODO(timsteele): Bug 1188626 - expire the master copies.
    PasswordForm copy(pending_credentials_);
    copy.origin = observed_form_.origin;
    copy.action = observed_form_.action;
    password_store->AddLogin(copy);
  } else if (observed_form_.new_password_element.empty() &&
             (pending_credentials_.password_element.empty() ||
              pending_credentials_.username_element.empty() ||
              pending_credentials_.submit_element.empty())) {
    // If |observed_form_| was a sign-up or change password form, there is no
    // point in trying to update element names: they are likely going to be
    // different than those on a login form.
    // Otherwise, given that |password_element| and |username_element| can't be
    // updated because they are part of Sync and PasswordStore primary key, we
    // must delete the old credentials altogether and then add the new ones.
    password_store->RemoveLogin(pending_credentials_);
    pending_credentials_.password_element = observed_form_.password_element;
    pending_credentials_.username_element = observed_form_.username_element;
    pending_credentials_.submit_element = observed_form_.submit_element;
    password_store->AddLogin(pending_credentials_);
  } else {
    password_store->UpdateLogin(pending_credentials_);
  }
}

void PasswordFormManager::UpdateMetadataForUsage(
    const PasswordForm& credential) {
  ++pending_credentials_.times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  pending_credentials_.other_possible_usernames.clear();
}

bool PasswordFormManager::UpdatePendingCredentialsIfOtherPossibleUsername(
    const base::string16& username) {
  for (PasswordFormMap::const_iterator it = best_matches_.begin();
       it != best_matches_.end(); ++it) {
    for (size_t i = 0; i < it->second->other_possible_usernames.size(); ++i) {
      if (it->second->other_possible_usernames[i] == username) {
        pending_credentials_ = *it->second;
        return true;
      }
    }
  }
  return false;
}

void PasswordFormManager::CheckForAccountCreationForm(
    const PasswordForm& observed,
    PasswordForm* pending) {
  if (pending->form_data.fields.empty())
    return;

  FormStructure pending_structure(pending->form_data);
  FormStructure observed_structure(observed.form_data);

  // Ignore |pending_structure| if its FormData has no fields. This is to
  // weed out those credentials that were saved before FormData was added
  // to PasswordForm. Even without this check, these FormStructure's won't
  // be uploaded, but it makes it hard to see if we are encountering
  // unexpected errors.
  if (pending_structure.FormSignature() != observed_structure.FormSignature()) {
    // Only upload if this is the first time the password has been used.
    // Otherwise the credentials have been used on the same field before so
    // they aren't from an account creation form.
    if (pending->times_used == 1) {
      if (UploadPasswordForm(pending->form_data,
                             autofill::ACCOUNT_CREATION_PASSWORD)) {
        pending->generation_upload_status =
            autofill::PasswordForm::POSITIVE_SIGNAL_SENT;
      }
    }
  } else if (pending->generation_upload_status ==
             autofill::PasswordForm::POSITIVE_SIGNAL_SENT) {
    // A signal was sent that this was an account creation form, but the
    // credential is now being used on the same form again. This cancels out
    // the previous vote.
    if (UploadPasswordForm(pending->form_data,
                           autofill::NOT_ACCOUNT_CREATION_PASSWORD)) {
      pending->generation_upload_status =
          autofill::PasswordForm::NEGATIVE_SIGNAL_SENT;
    }
  }
}

bool PasswordFormManager::UploadPasswordForm(
    const autofill::FormData& form_data,
    const autofill::ServerFieldType& password_type) {
  autofill::AutofillManager* autofill_manager =
      client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager)
    return false;

  // Note that this doesn't guarantee that the upload succeeded, only that
  // |form_data| is considered uploadable.
  bool success = autofill_manager->UploadPasswordForm(form_data, password_type);
  UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", success);
  return success;
}

int PasswordFormManager::ScoreResult(const PasswordForm& candidate) const {
  DCHECK_EQ(state_, MATCHING_PHASE);
  // For scoring of candidate login data:
  // The most important element that should match is the signon_realm followed
  // by the origin, the action, the password name, the submit button name, and
  // finally the username input field name.
  // If public suffix origin match was not used, it gives an addition of
  // 128 (1 << 7).
  // Exact origin match gives an addition of 64 (1 << 6) + # of matching url
  // dirs.
  // Partial match gives an addition of 32 (1 << 5) + # matching url dirs
  // That way, a partial match cannot trump an exact match even if
  // the partial one matches all other attributes (action, elements) (and
  // regardless of the matching depth in the URL path).
  int score = 0;
  if (!candidate.IsPublicSuffixMatch()) {
    score += 1 << 7;
  }
  if (candidate.origin == observed_form_.origin) {
    // This check is here for the most common case which
    // is we have a single match in the db for the given host,
    // so we don't generally need to walk the entire URL path (the else
    // clause).
    score += (1 << 6) + static_cast<int>(form_path_tokens_.size());
  } else {
    // Walk the origin URL paths one directory at a time to see how
    // deep the two match.
    std::vector<std::string> candidate_path_tokens;
    base::SplitString(candidate.origin.path(), '/', &candidate_path_tokens);
    size_t depth = 0;
    size_t max_dirs =
        std::min(form_path_tokens_.size(), candidate_path_tokens.size());
    while ((depth < max_dirs) &&
           (form_path_tokens_[depth] == candidate_path_tokens[depth])) {
      depth++;
      score++;
    }
    // do we have a partial match?
    score += (depth > 0) ? 1 << 5 : 0;
  }
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    if (candidate.action == observed_form_.action)
      score += 1 << 3;
    if (candidate.password_element == observed_form_.password_element)
      score += 1 << 2;
    if (candidate.submit_element == observed_form_.submit_element)
      score += 1 << 1;
    if (candidate.username_element == observed_form_.username_element)
      score += 1 << 0;
  }

  return score;
}

void PasswordFormManager::SubmitPassed() {
  submit_result_ = kSubmitResultPassed;
  if (has_generated_password_)
    LogPasswordGenerationSubmissionEvent(PASSWORD_SUBMITTED);
}

void PasswordFormManager::SubmitFailed() {
  submit_result_ = kSubmitResultFailed;
  if (has_generated_password_)
    LogPasswordGenerationSubmissionEvent(PASSWORD_SUBMISSION_FAILED);
}

}  // namespace password_manager
