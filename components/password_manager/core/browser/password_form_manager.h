// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordManager;
class PasswordManagerClient;

// Per-password-form-{on-page, dialog} class responsible for interactions
// between a given form, the per-tab PasswordManager, and the PasswordStore.
class PasswordFormManager : public PasswordStoreConsumer {
 public:
  // |password_manager| owns this object
  // |form_on_page| is the form that may be submitted and could need login data.
  // |ssl_valid| represents the security of the page containing observed_form,
  //           used to filter login results from database.
  PasswordFormManager(PasswordManager* password_manager,
                      PasswordManagerClient* client,
                      const base::WeakPtr<PasswordManagerDriver>& driver,
                      const autofill::PasswordForm& observed_form,
                      bool ssl_valid);
  ~PasswordFormManager() override;

  // Flags describing the result of comparing two forms as performed by
  // DoesMatch. Individual flags are only relevant for HTML forms, but
  // RESULT_COMPLETE_MATCH will also be returned to indicate non-HTML forms
  // completely matching.
  // The ordering of these flags is important. Larger matches are more
  // preferred than lower matches. That is, since RESULT_HTML_ATTRIBUTES_MATCH
  // is greater than RESULT_ACTION_MATCH, a match of only attributes and not
  // actions will be preferred to one of actions and not attributes.
  enum MatchResultFlags {
    RESULT_NO_MATCH = 0,
    RESULT_ACTION_MATCH = 1 << 0,
    RESULT_HTML_ATTRIBUTES_MATCH = 1 << 1,
    RESULT_ORIGINS_MATCH = 1 << 2,
    RESULT_COMPLETE_MATCH = RESULT_ACTION_MATCH | RESULT_HTML_ATTRIBUTES_MATCH |
                            RESULT_ORIGINS_MATCH
  };
  // Use MatchResultMask to contain combinations of MatchResultFlags values.
  // It's a signed int rather than unsigned to avoid signed/unsigned mismatch
  // caused by the enum values implicitly converting to signed int.
  typedef int MatchResultMask;

  enum OtherPossibleUsernamesAction {
    ALLOW_OTHER_POSSIBLE_USERNAMES,
    IGNORE_OTHER_POSSIBLE_USERNAMES
  };

  // Chooses between the current and new password value which one to save. This
  // is whichever is non-empty, with the preference being given to the new one.
  static base::string16 PasswordToSave(const autofill::PasswordForm& form);

  // Compares basic data of |observed_form_| with |form| and returns how much
  // they match. The return value is a MatchResultMask bitmask.
  MatchResultMask DoesManage(const autofill::PasswordForm& form) const;

  // Retrieves potential matching logins from the database.
  // |prompt_policy| indicates whether it's permissible to prompt the user to
  // authorize access to locked passwords. This argument is only used on
  // platforms that support prompting the user for access (such as Mac OS).
  void FetchMatchingLoginsFromPasswordStore(
      PasswordStore::AuthorizationPromptPolicy prompt_policy);

  // Simple state-check to verify whether this object as received a callback
  // from the PasswordStore and completed its matching phase. Note that the
  // callback in question occurs on the same (and only) main thread from which
  // instances of this class are ever used, but it is required since it is
  // conceivable that a user (or ui test) could attempt to submit a login
  // prompt before the callback has occured, which would InvokeLater a call to
  // PasswordManager::ProvisionallySave, which would interact with this object
  // before the db has had time to answer with matching password entries.
  // This is intended to be a one-time check; if the return value is false the
  // expectation is caller will give up. This clearly won't work if you put it
  // in a loop and wait for matching to complete; you're (supposed to be) on
  // the same thread!
  bool HasCompletedMatching() const;

  // Update |this| with the |form| that was actually submitted. Used to
  // determine what type the submitted form is for
  // IsIgnorableChangePasswordForm() and UMA stats.
  void SetSubmittedForm(const autofill::PasswordForm& form);

  // Determines if the user opted to 'never remember' passwords for this form.
  bool IsBlacklisted() const;

  // Used by PasswordManager to determine whether or not to display
  // a SavePasswordBar when given the green light to save the PasswordForm
  // managed by this.
  bool IsNewLogin() const;

  // Returns true if the current pending credentials were found using
  // origin matching of the public suffix, instead of the signon realm of the
  // form.
  bool IsPendingCredentialsPublicSuffixMatch() const;

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.). If this is called
  // before |this| has data from the PasswordStore, the execution will be
  // delayed until the data arrives.
  void ProcessFrame(const base::WeakPtr<PasswordManagerDriver>& driver);

  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

  // A user opted to 'never remember' passwords for this form.
  // Blacklist it so that from now on when it is seen we ignore it.
  // TODO(vasilii): remove the 'virtual' specifier.
  virtual void PermanentlyBlacklist();

  // If the user has submitted observed_form_, provisionally hold on to
  // the submitted credentials until we are told by PasswordManager whether
  // or not the login was successful. |action| describes how we deal with
  // possible usernames. If |action| is ALLOW_OTHER_POSSIBLE_USERNAMES we will
  // treat a possible usernames match as a sign that our original heuristics
  // were wrong and that the user selected the correct username from the
  // Autofill UI.
  void ProvisionallySave(const autofill::PasswordForm& credentials,
                         OtherPossibleUsernamesAction action);

  // Handles save-as-new or update of the form managed by this manager.
  // Note the basic data of updated_credentials must match that of
  // observed_form_ (e.g DoesManage(pending_credentials_) == true).
  // TODO: Make this private once we switch to the new UI.
  void Save();

  // Update the password store entry for |credentials_to_update|, using the
  // password from |pending_credentials_|. It modifies |pending_credentials_|.
  // |credentials_to_update| should be one of |best_matches_| or
  // |pending_credentials_|.
  void Update(const autofill::PasswordForm& credentials_to_update);

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void LogSubmitPassed();
  void LogSubmitFailed();

  // When attempting to provisionally save |form|, call this to check if it is
  // actually a change-password form which should be ignored, i.e., whether:
  // * the username was not explicitly marked with "autocomplete=username", and
  // * both the current and new password fields are non-empty, and
  // * the username and password do not match any credentials already stored.
  // In these cases the username field is detection is unreliable (there might
  // even be none), and the user should not be bothered with saving a
  // potentially malformed credential. Once we handle change password forms
  // correctly, this method should be replaced accordingly.
  //
  // Must be called after SetSubmittedForm().
  bool is_ignorable_change_password_form() const {
    DCHECK(form_type_ != kFormTypeUnspecified);
    return is_ignorable_change_password_form_;
  }

  // These functions are used to determine if this form has had it's password
  // auto generated by the browser.
  bool has_generated_password() const { return has_generated_password_; }
  void set_has_generated_password(bool generated_password) {
    has_generated_password_ = generated_password;
  }

  bool password_overridden() const { return password_overridden_; }

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable() { generation_available_ = true; }

  // Returns the pending credentials.
  const autofill::PasswordForm& pending_credentials() const {
    return pending_credentials_;
  }

  // Returns the best matches.
  const autofill::PasswordFormMap& best_matches() const {
    return best_matches_;
  }

  const autofill::PasswordForm* preferred_match() const {
    return preferred_match_;
  }

  const ScopedVector<autofill::PasswordForm>& blacklisted_matches() const {
    return blacklisted_matches_;
  }

#if defined(UNIT_TEST)
  void SimulateFetchMatchingLoginsFromPasswordStore() {
    // Just need to update the internal states.
    state_ = MATCHING_PHASE;
  }
#endif

  const autofill::PasswordForm& observed_form() const { return observed_form_; }

  bool is_possible_change_password_form_without_username() const {
    return is_possible_change_password_form_without_username_;
  }

  // Use this to wipe copies of |pending_credentials_| from the password store
  // (and |best_matches_| as well. It will only wipe if:
  // 1. The stored password differs from the one in |pending_credentials_|.
  // 2. And the store already returned results for the observed form.
  // This is designed for use with sync credentials, so it will use GAIA utils
  // to catch equivalent usernames (e.g., if |pending_credentials_| have
  // username 'test', and the store also contains outdated entries for
  // 'test@gmail.com' and 'test@googlemail.com', those will be wiped).
  void WipeStoreCopyIfOutdated();

 private:
  // ManagerAction - What does the manager do with this form? Either it
  // fills it, or it doesn't. If it doesn't fill it, that's either
  // because it has no match, or it is blacklisted, or it is disabled
  // via the AUTOCOMPLETE=off attribute. Note that if we don't have
  // an exact match, we still provide candidates that the user may
  // end up choosing.
  enum ManagerAction {
    kManagerActionNone = 0,
    kManagerActionAutofilled,
    kManagerActionBlacklisted,
    kManagerActionMax
  };

  // UserAction - What does the user do with this form? If he or she
  // does nothing (either by accepting what the password manager did, or
  // by simply (not typing anything at all), you get None. If there were
  // multiple choices and the user selects one other than the default,
  // you get Choose, if user selects an entry from matching against the Public
  // Suffix List you get ChoosePslMatch, if the user types in a new value
  // for just the password you get OverridePassword, and if the user types in a
  // new value for the username and password you get
  // OverrideUsernameAndPassword.
  enum UserAction {
    kUserActionNone = 0,
    kUserActionChoose,
    kUserActionChoosePslMatch,
    kUserActionOverridePassword,
    kUserActionOverrideUsernameAndPassword,
    kUserActionMax
  };

  // Result - What happens to the form?
  enum SubmitResult {
    kSubmitResultNotSubmitted = 0,
    kSubmitResultFailed,
    kSubmitResultPassed,
    kSubmitResultMax
  };

  // What the form is used for. kFormTypeUnspecified is only set before
  // the SetSubmittedForm() is called, and should never be actually uploaded.
  enum FormType {
    kFormTypeLogin,
    kFormTypeLoginNoUsername,
    kFormTypeChangePasswordEnabled,
    kFormTypeChangePasswordDisabled,
    kFormTypeChangePasswordNoUsername,
    kFormTypeSignup,
    kFormTypeSignupNoUsername,
    kFormTypeLoginAndSignup,
    kFormTypeUnspecified,
    kFormTypeMax
  };

  // The maximum number of combinations of the three preceding enums.
  // This is used when recording the actions taken by the form in UMA.
  static const int kMaxNumActionsTaken =
      kManagerActionMax * kUserActionMax * kSubmitResultMax;

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.).
  void ProcessFrameInternal(const base::WeakPtr<PasswordManagerDriver>& driver);

  // Trigger filling of HTTP auth dialog and update |manager_action_|.
  void ProcessLoginPrompt();

  // Determines if we need to autofill given the results of the query.
  // Takes ownership of the elements in |result|.
  void OnRequestDone(ScopedVector<autofill::PasswordForm> result);

  // Helper for Save in the case that best_matches.size() == 0, meaning
  // we have no prior record of this form/username/password and the user
  // has opted to 'Save Password'. The previously preferred login from
  // |best_matches_| will be reset.
  void SaveAsNewLogin();

  // Helper for OnGetPasswordStoreResults to score an individual result
  // against the observed_form_.
  uint32_t ScoreResult(const autofill::PasswordForm& candidate) const;

  // For the blacklisted |form| returns true iff it blacklists |observed_form_|.
  bool IsBlacklistMatch(const autofill::PasswordForm& form) const;

  // Helper for Save in the case that best_matches.size() > 0, meaning
  // we have at least one match for this form/username/password. This
  // Updates the form managed by this object, as well as any matching forms
  // that now need to have preferred bit changed, since updated_credentials
  // is now implicitly 'preferred'.
  void UpdateLogin();

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server to vote for the correct username field, and also so that
  // we will trigger password generation in the future. This function will
  // update generation_upload_status of |pending| if an upload is performed.
  void SendAutofillVotes(const autofill::PasswordForm& observed,
                         autofill::PasswordForm* pending);

  // Update all login matches to reflect new preferred state - preferred flag
  // will be reset on all matched logins that different than the current
  // |pending_credentials_|.
  void UpdatePreferredLoginState(PasswordStore* password_store);

  // Returns true if |username| is one of the other possible usernames for a
  // password form in |best_matches_| and sets |pending_credentials_| to the
  // match which had this username.
  bool UpdatePendingCredentialsIfOtherPossibleUsername(
      const base::string16& username);

  // Returns true if |form| is a username update of a credential already in
  // |best_matches_|. Sets |pending_credentials_| to the appropriate
  // PasswordForm if it returns true.
  bool UpdatePendingCredentialsIfUsernameChanged(
      const autofill::PasswordForm& form);

  // Update state to reflect that |credential| was used. This is broken out from
  // UpdateLogin() so that PSL matches can also be properly updated.
  void UpdateMetadataForUsage(const autofill::PasswordForm& credential);

  // Converts the "ActionsTaken" fields into an int so they can be logged to
  // UMA.
  int GetActionsTaken() const;

  // Remove possible_usernames that may contains sensitive information and
  // duplicates.
  void SanitizePossibleUsernames(autofill::PasswordForm* form);

  // Helper function to delegate uploading to the AutofillManager. Returns true
  // on success.
  // |login_form_signature| may be empty.  It is non-empty when the user fills
  // and submits a login form using a generated password. In this case,
  // |login_form_signature| should be set to the submitted form's signature.
  // Note that in this case, |form.FormSignature()| gives the signature for the
  // registration form on which the password was generated, rather than the
  // submitted form's signature.
  bool UploadPasswordForm(const autofill::FormData& form_data,
                          const base::string16& username_field,
                          const autofill::ServerFieldType& password_type,
                          const std::string& login_form_signature);

  // Create pending credentials from provisionally saved form and forms received
  // from password store.
  void CreatePendingCredentials();

  // If |pending_credentials_.username_value| is not empty, iterates over all
  // forms from |best_matches_| and deletes from the password store all which
  // are not PSL-matched, have an empty username, and a password equal to
  // |pending_credentials_.password_value|.
  void DeleteEmptyUsernameCredentials();

  // If |best_matches| contains only one entry then return this entry. Otherwise
  // for empty |password| return nullptr and for non-empty |password| returns
  // the unique entry in |best_matches_| with the same password, if it exists,
  // and nullptr otherwise.
  autofill::PasswordForm* FindBestMatchForUpdatePassword(
      const base::string16& password) const;

  // Set of nonblacklisted PasswordForms from the DB that best match the form
  // being managed by this. Use a map instead of vector, because we most
  // frequently require lookups by username value in IsNewLogin.
  autofill::PasswordFormMap best_matches_;

  // Set of forms from PasswordStore that correspond to the current site and
  // that are not in |best_matches_|.
  ScopedVector<autofill::PasswordForm> not_best_matches_;

  // Set of blacklisted forms from the PasswordStore that best match the current
  // form.
  ScopedVector<autofill::PasswordForm> blacklisted_matches_;

  // The PasswordForm from the page or dialog managed by |this|.
  const autofill::PasswordForm observed_form_;

  // Stores provisionally saved form until |pending_credentials_| is created.
  scoped_ptr<const autofill::PasswordForm> provisionally_saved_form_;
  // Stores if for creating |pending_credentials_| other possible usernames
  // option should apply.
  OtherPossibleUsernamesAction other_possible_username_action_;

  // The origin url path of observed_form_ tokenized, for convenience when
  // scoring.
  const std::vector<std::string> form_path_segments_;

  // Stores updated credentials when the form was submitted but success is
  // still unknown.
  autofill::PasswordForm pending_credentials_;

  // Whether pending_credentials_ stores a new login or is an update
  // to an existing one.
  bool is_new_login_;

  // Whether this form has an auto generated password.
  bool has_generated_password_;

  // Whether the saved password was overridden.
  bool password_overridden_;

  // Whether the user can choose to generate a password for this form.
  bool generation_available_;

  // Set if the user has selected one of the other possible usernames in
  // |pending_credentials_|.
  base::string16 selected_username_;

  // PasswordManager owning this.
  PasswordManager* const password_manager_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_;

  // If the submitted form is for a change password form and the username
  // doesn't match saved credentials.
  bool is_ignorable_change_password_form_;

  // True if we consider this form to be a change password form without username
  // field. We use only client heuristics, so it could include signup forms.
  // The value of this variable is calculated based not only on information from
  // |observed_form_| but also on the credentials that the user submitted.
  bool is_possible_change_password_form_without_username_;

  typedef enum {
    PRE_MATCHING_PHASE,  // Have not yet invoked a GetLogins query to find
                         // matching login information from password store.
    MATCHING_PHASE,      // We've made a GetLogins request, but
                         // haven't received or finished processing result.
    POST_MATCHING_PHASE  // We've queried the DB and processed matching
                         // login results.
  } PasswordFormManagerState;

  // State of matching process, used to verify that we don't call methods
  // assuming we've already processed the request for matching logins,
  // when we actually haven't.
  PasswordFormManagerState state_;

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  // |this| is created for a form in some frame, which is represented by a
  // driver. Similar form can appear in more frames, represented with more
  // drivers. The drivers are needed to perform frame-specific operations
  // (filling etc.). These drivers are kept in |drivers_| to allow updating of
  // the filling information when needed.
  std::vector<base::WeakPtr<PasswordManagerDriver>> drivers_;

  // These three fields record the "ActionsTaken" by the browser and
  // the user with this form, and the result. They are combined and
  // recorded in UMA when the manager is destroyed.
  ManagerAction manager_action_;
  UserAction user_action_;
  SubmitResult submit_result_;

  // Form type of the form that |this| is managing. Set after SetSubmittedForm()
  // as our classification of the form can change depending on what data the
  // user has entered.
  FormType form_type_;

  // Null unless FetchMatchingLoginsFromPasswordStore has been called again
  // without the password store returning results in the meantime. In that case
  // this stores the prompt policy for the refresh call.
  scoped_ptr<PasswordStore::AuthorizationPromptPolicy> next_prompt_policy_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
