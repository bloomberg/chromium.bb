// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
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

  // Returns true if |form| has both the current and new password fields, and
  // the username field was not explicitly marked with "autocomplete=username"
  // and |form.username_value| and |form.password_value| fields do not match
  // the credentials already stored. In these cases it is not clear whether
  // the username field is the right guess (often such change password forms do
  // not contain the username at all), and the user should not be bothered with
  // saving a potentially malformed credential. Once we handle change password
  // forms correctly, this method should be replaced accordingly.
  bool IsIgnorableChangePasswordForm(const autofill::PasswordForm& form) const;

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

  // Checks if the form is a valid password form. Forms which lack password
  // field are not considered valid.
  bool HasValidPasswordForm() const;

  // These functions are used to determine if this form has had it's password
  // auto generated by the browser.
  bool HasGeneratedPassword() const;
  void SetHasGeneratedPassword();

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.). If this is called
  // before |this| has data from the PasswordStore, the execution will be
  // delayed until the data arrives.
  void ProcessFrame(const base::WeakPtr<PasswordManagerDriver>& driver);

  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

  // A user opted to 'never remember' passwords for this form.
  // Blacklist it so that from now on when it is seen we ignore it.
  // TODO: Make this private once we switch to the new UI.
  void PermanentlyBlacklist();

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

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void SubmitPassed();
  void SubmitFailed();

  // Returns the username associated with the credentials.
  const base::string16& associated_username() const {
    return pending_credentials_.username_value;
  }

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

  // Returns the realm URL for the form managed my this manager.
  const std::string& realm() const { return pending_credentials_.signon_realm; }

#if defined(UNIT_TEST)
  void SimulateFetchMatchingLoginsFromPasswordStore() {
    // Just need to update the internal states.
    state_ = MATCHING_PHASE;
  }
#endif

 protected:
  const autofill::PasswordForm& observed_form() const { return observed_form_; }

 private:
  friend class PasswordFormManagerTest;

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

  // The maximum number of combinations of the three preceding enums.
  // This is used when recording the actions taken by the form in UMA.
  static const int kMaxNumActionsTaken =
      kManagerActionMax * kUserActionMax * kSubmitResultMax;

  // Determines if we need to autofill given the results of the query.
  // Takes ownership of the elements in |result|.
  void OnRequestDone(ScopedVector<autofill::PasswordForm> result);

  // Helper for OnGetPasswordStoreResults to determine whether or not
  // the given result form is worth scoring.
  bool ShouldIgnoreResult(const autofill::PasswordForm& form) const;

  // Helper for Save in the case that best_matches.size() == 0, meaning
  // we have no prior record of this form/username/password and the user
  // has opted to 'Save Password'. If |reset_preferred_login| is set,
  // the previously preferred login from |best_matches_| will be reset.
  void SaveAsNewLogin(bool reset_preferred_login);

  // Helper for OnGetPasswordStoreResults to score an individual result
  // against the observed_form_.
  int ScoreResult(const autofill::PasswordForm& form) const;

  // Helper for Save in the case that best_matches.size() > 0, meaning
  // we have at least one match for this form/username/password. This
  // Updates the form managed by this object, as well as any matching forms
  // that now need to have preferred bit changed, since updated_credentials
  // is now implicitly 'preferred'.
  void UpdateLogin();

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server, so that we will trigger password generation in the future.
  // This function will update generation_upload_status of |pending| if an
  // upload is performed.
  void CheckForAccountCreationForm(const autofill::PasswordForm& observed,
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
  bool UploadPasswordForm(const autofill::FormData& form_data,
                          const autofill::ServerFieldType& password_type);

  // Set of PasswordForms from the DB that best match the form
  // being managed by this. Use a map instead of vector, because we most
  // frequently require lookups by username value in IsNewLogin.
  // TODO(vabr): Consider using ScopedPtrHashMap instead of the deleter below.
  autofill::PasswordFormMap best_matches_;

  // Cleans up when best_matches_ goes out of scope.
  STLValueDeleter<autofill::PasswordFormMap> best_matches_deleter_;

  // The PasswordForm from the page or dialog managed by |this|.
  const autofill::PasswordForm observed_form_;

  // The origin url path of observed_form_ tokenized, for convenience when
  // scoring.
  std::vector<std::string> form_path_tokens_;

  // Stores updated credentials when the form was submitted but success is
  // still unknown.
  autofill::PasswordForm pending_credentials_;

  // Whether pending_credentials_ stores a new login or is an update
  // to an existing one.
  bool is_new_login_;

  // Whether this form has an auto generated password.
  bool has_generated_password_;

  // Set if the user has selected one of the other possible usernames in
  // |pending_credentials_|.
  base::string16 selected_username_;

  // PasswordManager owning this.
  const PasswordManager* const password_manager_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_;

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

  // When |this| is created, it is because a form has been found in a frame,
  // represented by a driver. For that frame, and for all others with an
  // equivalent form, the associated drivers are needed to perform
  // frame-specific operations (filling etc.). While |this| waits for the
  // matching credentials from the PasswordStore, the drivers for frames needing
  // fill information are buffered in |drivers_|, and served once the results
  // arrive, if the drivers are still valid.
  std::vector<base::WeakPtr<PasswordManagerDriver>> drivers_;

  // These three fields record the "ActionsTaken" by the browser and
  // the user with this form, and the result. They are combined and
  // recorded in UMA when the manager is destroyed.
  ManagerAction manager_action_;
  UserAction user_action_;
  SubmitResult submit_result_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
