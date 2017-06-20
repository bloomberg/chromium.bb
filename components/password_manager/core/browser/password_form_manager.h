// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_form_user_action.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"

using autofill::FormData;
using autofill::FormStructure;

namespace password_manager {

class FormSaver;
class PasswordManager;
class PasswordManagerClient;

// A map from field names to field types.
using FieldTypeMap = std::map<base::string16, autofill::ServerFieldType>;

// This class helps with filling the observed form (both HTML and from HTTP
// auth) and with saving/updating the stored information about it.
class PasswordFormManager : public FormFetcher::Consumer {
 public:
  // |password_manager| owns |this|, |client| and |driver| serve to
  // communicate with embedder, |observed_form| is the associated form |this|
  // is managing, |form_saver| is used to save/update the form and
  // |form_fetcher| to get saved data about the form. |form_fetcher| must not be
  // destroyed before |this|.
  //
  // TODO(crbug.com/621355): So far, |form_fetcher| can be null. In that case
  // |this| creates an instance of it itself (meant for production code). Once
  // the fetcher is shared between PasswordFormManager instances, it will be
  // required that |form_fetcher| is not null.
  PasswordFormManager(PasswordManager* password_manager,
                      PasswordManagerClient* client,
                      const base::WeakPtr<PasswordManagerDriver>& driver,
                      const autofill::PasswordForm& observed_form,
                      std::unique_ptr<FormSaver> form_saver,
                      FormFetcher* form_fetcher);
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
    RESULT_ORIGINS_OR_FRAMES_MATCH = 1 << 2,
    RESULT_COMPLETE_MATCH = RESULT_ACTION_MATCH | RESULT_HTML_ATTRIBUTES_MATCH |
                            RESULT_ORIGINS_OR_FRAMES_MATCH
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
  // |driver| is optional and if it's given it should be a driver that
  // corresponds to a frame from which |form| comes from.
  MatchResultMask DoesManage(
      const autofill::PasswordForm& form,
      const password_manager::PasswordManagerDriver* driver) const;

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

  // These functions are used to determine if this form has had it's password
  // auto generated by the browser.
  bool has_generated_password() const { return has_generated_password_; }
  void SetHasGeneratedPassword(bool generated_password);

  bool is_manual_generation() { return is_manual_generation_; }
  void set_is_manual_generation(bool is_manual_generation) {
    is_manual_generation_ = is_manual_generation;
  }

  const base::string16& generation_element() { return generation_element_; }
  void set_generation_element(const base::string16& generation_element) {
    generation_element_ = generation_element;
  }

  bool get_generation_popup_was_shown() const {
    return generation_popup_was_shown_;
  }
  void set_generation_popup_was_shown(bool generation_popup_was_shown) {
    generation_popup_was_shown_ = generation_popup_was_shown;
  }

  bool password_overridden() const { return password_overridden_; }

  bool retry_password_form_password_update() const {
    return retry_password_form_password_update_;
  }

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable();

  // Returns the provisionally saved form, if it exists, otherwise nullptr.
  const autofill::PasswordForm* submitted_form() const {
    return submitted_form_.get();
  }

  // Returns the pending credentials.
  const autofill::PasswordForm& pending_credentials() const {
    return pending_credentials_;
  }

  // Returns the best matches.
  const std::map<base::string16, const autofill::PasswordForm*>& best_matches()
      const {
    return best_matches_;
  }

  const autofill::PasswordForm* preferred_match() const {
    return preferred_match_;
  }

  const std::vector<const autofill::PasswordForm*>& blacklisted_matches()
      const {
    return blacklisted_matches_;
  }

  const autofill::PasswordForm& observed_form() const { return observed_form_; }

  bool is_possible_change_password_form_without_username() const {
    return is_possible_change_password_form_without_username_;
  }

  FormFetcher* form_fetcher() { return form_fetcher_; }

  // Use this to wipe copies of |pending_credentials_| from the password store
  // (and |best_matches_| as well. It will only wipe if:
  // 1. The stored password differs from the one in |pending_credentials_|.
  // 2. And the store already returned results for the observed form.
  // This is designed for use with sync credentials, so it will use GAIA utils
  // to catch equivalent usernames (e.g., if |pending_credentials_| have
  // username 'test', and the store also contains outdated entries for
  // 'test@gmail.com' and 'test@googlemail.com', those will be wiped).
  void WipeStoreCopyIfOutdated();

  // Called when the user chose not to update password.
  void OnNopeUpdateClicked();

  // Called when the user clicked "Never" button in the "save password" prompt.
  void OnNeverClicked();

  // Called when the user didn't interact with UI. |is_update| is true iff
  // it was the update UI.
  void OnNoInteraction(bool is_update);

  // Saves the outcome of HTML parsing based form classifier to upload proto.
  void SaveGenerationFieldDetectedByClassifier(
      const base::string16& generation_field);

  FormSaver* form_saver() { return form_saver_.get(); }

  // Clears references to matches derived from the associated FormFetcher data.
  // After calling this, the PasswordFormManager holds no references to objects
  // owned by the associated FormFetcher. This does not cause removing |this| as
  // a consumer of |form_fetcher_|.
  void ResetStoredMatches();

  // Takes ownership of |fetcher|. If |fetcher| is different from the current
  // |form_fetcher_| then also resets matches stored from the old fetcher and
  // adds itself as a consumer of the new one.
  void GrabFetcher(std::unique_ptr<FormFetcher> fetcher);

 protected:
  // FormFetcher::Consumer:
  void ProcessMatches(
      const std::vector<const autofill::PasswordForm*>& non_federated,
      size_t filtered_count) override;

 private:
  // The outcome of the form classifier.
  enum FormClassifierOutcome {
    kNoOutcome,
    kNoGenerationElement,
    kFoundGenerationElement
  };

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.).
  void ProcessFrameInternal(const base::WeakPtr<PasswordManagerDriver>& driver);

  // Trigger filling of HTTP auth dialog and update |manager_action_|.
  void ProcessLoginPrompt();

  // Given all non-blacklisted |matches|, computes their score and populates
  // |best_matches_|, |preferred_match_| and |non_best_matches_| accordingly.
  void ScoreMatches(const std::vector<const autofill::PasswordForm*>& matches);

  // Helper for Save in the case that best_matches.size() == 0, meaning
  // we have no prior record of this form/username/password and the user
  // has opted to 'Save Password'. The previously preferred login from
  // |best_matches_| will be reset.
  void SaveAsNewLogin();

  // Helper for OnGetPasswordStoreResults to score an individual result
  // against the observed_form_.
  uint32_t ScoreResult(const autofill::PasswordForm& candidate) const;

  // Returns true iff |form| is a non-blacklisted match for |observed_form_|.
  bool IsMatch(const autofill::PasswordForm& form) const;

  // Returns true iff |form| blacklists |observed_form_|.
  bool IsBlacklistMatch(const autofill::PasswordForm& form) const;

  // Helper for Save in the case there is at least one match for the pending
  // credentials. This sends needed signals to the autofill server, and also
  // triggers some UMA reporting.
  void ProcessUpdate();

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server to vote for the correct username field, and also so that
  // we will trigger password generation in the future. This function will
  // update generation_upload_status of |pending| if an upload is performed.
  void SendVoteOnCredentialsReuse(const autofill::PasswordForm& observed,
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

  // Searches for |username| in |other_possible_usernames| of |best_matches_|
  // and |not_best_matches_|. If the username value is found in
  // |other_possible_usernames| and the password value of the match is equal to
  // |password|, the match is saved to |username_correction_vote_|.
  void FindCorrectedUsernameElement(const base::string16& username,
                                    const base::string16& password);

  // Searches for |username| in |other_possible_usernames| of |match|. If the
  // username value is found, the match is saved to |username_correction_vote_|
  // and the function returns true.
  bool FindUsernameInOtherPossibleUsernames(const autofill::PasswordForm& match,
                                            const base::string16& username);

  // Returns true if |form| is a username update of a credential already in
  // |best_matches_|. Sets |pending_credentials_| to the appropriate
  // PasswordForm if it returns true.
  bool UpdatePendingCredentialsIfUsernameChanged(
      const autofill::PasswordForm& form);

  // Tries to set all votes (e.g. autofill field types, generation vote) to
  // a |FormStructure| and upload it to the server. Returns true on success.
  bool UploadPasswordVote(const autofill::PasswordForm& form_to_upload,
                          const autofill::ServerFieldType& password_type,
                          const std::string& login_form_signature);

  // Adds a vote on password generation usage to |form_structure|.
  void AddGeneratedVote(autofill::FormStructure* form_structure);

  // Adds a vote from HTML parsing based form classifier to |form_structure|.
  void AddFormClassifierVote(autofill::FormStructure* form_structure);

  // Create pending credentials from provisionally saved form and forms received
  // from password store.
  void CreatePendingCredentials();

  // Create pending credentials from provisionally saved form when this form
  // represents credentials that were not previosly saved.
  void CreatePendingCredentialsForNewCredentials();

  // If |best_matches| contains only one entry then return this entry. Otherwise
  // for empty |password| return nullptr and for non-empty |password| returns
  // the unique entry in |best_matches_| with the same password, if it exists,
  // and nullptr otherwise.
  const autofill::PasswordForm* FindBestMatchForUpdatePassword(
      const base::string16& password) const;

  // Try to find best matched to |form| from |best_matches_| by the rules:
  // 1. If there is an element in |best_matches_| with the same username then
  // return it;
  // 2. If |form| is created with Credential API return nullptr, i.e. we match
  // Credentials API forms only by username;
  // 3. If |form| has no |username_element| and no |new_password_element| (i.e.
  // a form contains only one field which is a password) and there is an element
  // from |best_matches_| with the same password as in |form| then return it;
  // 4. Otherwise return nullptr.
  const autofill::PasswordForm* FindBestSavedMatch(
      const autofill::PasswordForm* form) const;

  // Send appropriate votes based on what is currently being saved.
  void SendVotesOnSave();

  // Send a vote for sign-in forms with autofill types for a username field.
  void SendSignInVote(const FormData& form_data);

  // Sets |user_action_| and records some metrics.
  void SetUserAction(UserAction user_action);

  // Edits some fields in |pending_credentials_| before it can be used to
  // update the password store. It also goes through |not_best_matches|,
  // updates the password of those which share the old password and username
  // with |pending_credentials_| to the new password of |pending_credentials_|,
  // and adds copies of all such modified credentials to
  // |credentials_to_update|. If needed, this also returns a PasswordForm to be
  // used as the old primary key during the store update.
  base::Optional<autofill::PasswordForm> UpdatePendingAndGetOldKey(
      std::vector<autofill::PasswordForm>* credentials_to_update);

  // Set of nonblacklisted PasswordForms from the DB that best match the form
  // being managed by |this|, indexed by username. They are owned by
  // |form_fetcher_|.
  std::map<base::string16, const autofill::PasswordForm*> best_matches_;

  // Set of forms from PasswordStore that correspond to the current site and
  // that are not in |best_matches_|. They are owned by |form_fetcher_|.
  std::vector<const autofill::PasswordForm*> not_best_matches_;

  // Set of blacklisted forms from the PasswordStore that best match the current
  // form. They are owned by |form_fetcher_|, with the exception that if
  // |new_blacklisted_| is not null, the address of that form is also inside
  // |blacklisted_matches_|.
  std::vector<const autofill::PasswordForm*> blacklisted_matches_;

  // If the observed form gets blacklisted through |this|, the blacklist entry
  // gets stored in |new_blacklisted_| until data is potentially refreshed by
  // reading from PasswordStore again. |blacklisted_matches_| will contain
  // |new_blacklisted_.get()| in that case. The PasswordForm will usually get
  // accessed via |blacklisted_matches_|, this unique_ptr is only used to store
  // it (unlike the rest of forms being pointed to in |blacklisted_matches_|,
  // which are owned by |form_fetcher_|.
  std::unique_ptr<autofill::PasswordForm> new_blacklisted_;

  // The PasswordForm from the page or dialog managed by |this|.
  const autofill::PasswordForm observed_form_;

  // Stores a submitted form.
  std::unique_ptr<const autofill::PasswordForm> submitted_form_;

  // Stores if for creating |pending_credentials_| other possible usernames
  // option should apply.
  OtherPossibleUsernamesAction other_possible_username_action_;

  // If the user typed username that doesn't match any saved credentials, but
  // matches an entry from |other_possible_usernames| of a saved credential,
  // then |username_correction_vote_| stores the credential with matched
  // username. The matched credential is copied to |username_correction_vote_|,
  // but |username_correction_vote_.username_element| is set to the name of the
  // field where matched username was found.
  std::unique_ptr<autofill::PasswordForm> username_correction_vote_;

  // The origin url path of observed_form_ tokenized, for convenience when
  // scoring.
  const std::vector<std::string> form_path_segments_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |submitted_form_| and |best_matches_|.
  autofill::PasswordForm pending_credentials_;

  // Whether pending_credentials_ stores a new login or is an update
  // to an existing one.
  bool is_new_login_;

  // Whether the form was autofilled with credentials.
  bool has_autofilled_;

  // Whether this form has an auto generated password.
  bool has_generated_password_;

  // Whether password generation was manually triggered.
  bool is_manual_generation_;

  // A password field name that is used for generation.
  base::string16 generation_element_;

  // Whether generation popup was shown at least once.
  bool generation_popup_was_shown_;

  // The outcome of HTML parsing based form classifier.
  FormClassifierOutcome form_classifier_outcome_;

  // If |form_classifier_outcome_| == kFoundGenerationElement, the field
  // contains the name of the detected generation element.
  base::string16 generation_element_detected_by_classifier_;

  // Whether the saved password was overridden.
  bool password_overridden_;

  // A form is considered to be "retry" password if it has only one field which
  // is a current password field.
  // This variable is true if the password passed through ProvisionallySave() is
  // a password that is not part of any password form stored for this origin
  // and it was entered on a retry password form.
  bool retry_password_form_password_update_;

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

  // True if we consider this form to be a change password form without username
  // field. We use only client heuristics, so it could include signup forms.
  // The value of this variable is calculated based not only on information from
  // |observed_form_| but also on the credentials that the user submitted.
  bool is_possible_change_password_form_without_username_;

  // True if |submitted_form_| looks like SignUp form according to
  // local heuristics.
  bool does_look_like_signup_form_ = false;

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  // |this| is created for a form in some frame, which is represented by a
  // driver. Similar form can appear in more frames, represented with more
  // drivers. The drivers are needed to perform frame-specific operations
  // (filling etc.). These drivers are kept in |drivers_| to allow updating of
  // the filling information when needed.
  std::vector<base::WeakPtr<PasswordManagerDriver>> drivers_;

  // Records the action the user has taken while interacting with the password
  // form.
  UserAction user_action_;

  // FormSaver instance used by |this| to all tasks related to storing
  // credentials.
  std::unique_ptr<FormSaver> form_saver_;

  // When not null, then this is the object which |form_fetcher_| points to.
  std::unique_ptr<FormFetcher> owned_form_fetcher_;

  // FormFetcher instance which owns the login data from PasswordStore.
  FormFetcher* form_fetcher_;

  // True if the main frame's visible URL, at the time this PasswordFormManager
  // was created, is secure.
  bool is_main_frame_secure_ = false;

  // Takes care of recording metrics and events for this PasswordFormManager.
  PasswordFormMetricsRecorder metrics_recorder_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
