// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace base {
class DictionaryValue;
class ListValue;
}

#if defined(ENABLE_SUPERVISED_USERS)
class SupervisedUserRegistrationUtility;
#endif

// Handler for the 'create profile' page.
class SigninCreateProfileHandler : public content::WebUIMessageHandler {
 public:
  SigninCreateProfileHandler();
  ~SigninCreateProfileHandler() override;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

 protected:
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ReturnDefaultProfileNameAndIcons);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ReturnSignedInProfiles);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CreateProfile);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CreateSupervisedUser);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ImportSupervisedUser);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           ImportSupervisedUserAlreadyOnDevice);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CustodianNotAuthenticated);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           CustodianHasAuthError);
  FRIEND_TEST_ALL_PREFIXES(SigninCreateProfileHandlerTest,
                           NotAllowedToCreateSupervisedUser);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  // Represents the final profile creation status. It is used to map
  // the status to the javascript method to be called.
  enum ProfileCreationStatus {
    PROFILE_CREATION_SUCCESS,
    PROFILE_CREATION_ERROR,
  };

  // Represents the type of the in progress profile creation operation.
  // It is used to map the type of the profile creation operation to the
  // correct UMA metric name.
  enum ProfileCreationOperationType {
#if defined(ENABLE_SUPERVISED_USERS)
    SUPERVISED_PROFILE_CREATION,
    SUPERVISED_PROFILE_IMPORT,
#endif
    NON_SUPERVISED_PROFILE_CREATION,
    NO_CREATION_IN_PROGRESS
  };

  // Callback for the "requestDefaultProfileIcons" message.
  // Sends the array of default profile icon URLs to WebUI.
  void RequestDefaultProfileIcons(const base::ListValue* args);

  // Sends an object to WebUI of the form: { "name": profileName } after
  // "requestDefaultProfileIcons" is fulfilled.
  void SendNewProfileDefaults();

  // Callback for the "requestSignedInProfiles" message.
  // Sends the email address of the signed-in user, or an empty string if the
  // user is not signed in. Also sends information about whether supervised
  // users may be created.
  void RequestSignedInProfiles(const base::ListValue* args);

  // Asynchronously creates and initializes a new profile.
  // The arguments are as follows:
  //   0: name (string)
  //   1: icon (string)
  //   2: a flag stating whether we should create a profile desktop shortcut
  //      (optional, boolean)
  //   3: a flag stating whether the user should be supervised
  //      (optional, boolean)
  //   4: a string representing the supervised user ID.
  //   5: a string representing the custodian profile path.
  void CreateProfile(const base::ListValue* args);

  // If a local error occurs during profile creation, then show an appropriate
  // error message. However, if profile creation succeeded and the
  // profile being created/imported is a supervised user profile,
  // then proceed with the registration step. Otherwise, update the UI
  // as the final task after a new profile has been created.
  void OnProfileCreated(bool create_shortcut,
                        const std::string& supervised_user_id,
                        Profile* custodian_profile,
                        Profile* profile,
                        Profile::CreateStatus status);

  void HandleProfileCreationSuccess(bool create_shortcut,
                                    const std::string& supervised_user_id,
                                    Profile* custodian_profile,
                                    Profile* profile);

  // Creates desktop shortcut and updates the UI to indicate success
  // when creating a profile.
  void CreateShortcutAndShowSuccess(bool create_shortcut,
                                    Profile* custodian_profile,
                                    Profile* profile);

  // Updates the UI to show an error when creating a profile.
  void ShowProfileCreationError(Profile* profile, const base::string16& error);

  // Updates the UI to show a non-fatal warning when creating a profile.
  void ShowProfileCreationWarning(const base::string16& warning);

  // Records UMA histograms relevant to profile creation.
  void RecordProfileCreationMetrics(Profile::CreateStatus status);

  base::string16 GetProfileCreationErrorMessageLocal() const;
#if defined(ENABLE_SUPERVISED_USERS)
  // The following error messages only apply to supervised profiles.
  base::string16 GetProfileCreateErrorMessageRemote() const;
  base::string16 GetProfileCreateErrorMessageSignin() const;
#endif

  base::StringValue GetWebUIListenerName(ProfileCreationStatus status) const;

  // Used to allow canceling a profile creation (particularly a supervised-user
  // registration) in progress. Set when profile creation is begun, and
  // cleared when all the callbacks have been run and creation is complete.
  base::FilePath profile_path_being_created_;

  // Used to track how long profile creation takes.
  base::TimeTicks profile_creation_start_time_;

  // Indicates the type of the in progress profile creation operation.
  // The value is only relevant while we are creating/importing a profile.
  ProfileCreationOperationType profile_creation_type_;

  // Asynchronously creates and initializes a new profile.
  virtual void DoCreateProfile(const base::string16& name,
                               const std::string& icon_url,
                               bool create_shortcut,
                               const std::string& supervised_user_id,
                               Profile* custodian_profile);

#if defined(ENABLE_SUPERVISED_USERS)
  // Extracts the supervised user ID and the custodian user profile path from
  // the args passed into CreateProfile.
  bool GetSupervisedCreateProfileArgs(const base::ListValue* args,
                                      std::string* supervised_user_id,
                                      base::FilePath* custodian_profile_path);

  // Callback that runs once the custodian profile has been loaded. It sets
  // |profile_creation_type_| if necessary, and calls |DoCreateProfile| if the
  // supervised user id specified in |args| is valid.
  void LoadCustodianProfileCallback(const base::string16& name,
                                     const std::string& icon_url,
                                     bool create_shortcut,
                                     const std::string& supervised_user_id,
                                     Profile* custodian_profile,
                                     Profile::CreateStatus status);

  // Cancels creation of a supervised-user profile currently in progress, as
  // indicated by profile_path_being_created_, removing the object and files
  // and canceling supervised-user registration. This is the handler for the
  // "cancelCreateProfile" message. |args| is not used.
  void HandleCancelProfileCreation(const base::ListValue* args);

  // Internal implementation. This may safely be called whether profile creation
  // or registration is in progress or not. |user_initiated| should be true if
  // the cancellation was deliberately requested by the user, and false if it
  // was caused implicitly, e.g. by shutting down the browser.
  void CancelProfileRegistration(bool user_initiated);

  // After a new supervised-user profile has been created, registers the user
  // with the management server.
  virtual void RegisterSupervisedUser(bool create_shortcut,
                                      const std::string& managed_user_id,
                                      Profile* custodian_profile,
                                      Profile* new_profile);

  // Called back with the result of the supervised user registration.
  void OnSupervisedUserRegistered(bool create_shortcut,
                                  Profile* custodian_profile,
                                  Profile* profile,
                                  const GoogleServiceAuthError& error);

  // Records UMA histograms relevant to supervised user profiles
  // creation and registration.
  void RecordSupervisedProfileCreationMetrics(
      GoogleServiceAuthError::State error_state);

  // Creates the supervised user with the given |supervised_user_id| if the user
  // doesn't already exist on the machine.
  void DoCreateProfileIfPossible(const base::string16& name,
                                 const std::string& icon_url,
                                 bool create_shortcut,
                                 const std::string& supervised_user_id,
                                 Profile* custodian_profile,
                                 const base::DictionaryValue* dict);

  // Opens a new window for |profile|.
  virtual void OpenNewWindowForProfile(Profile* profile,
                                       Profile::CreateStatus status);

  // Callback for the "switchToProfile" message. Opens a new window for the
  // profile. The profile file path is passed as a string argument.
  void SwitchToProfile(const base::ListValue* args);

  std::unique_ptr<SupervisedUserRegistrationUtility>
      supervised_user_registration_utility_;
#endif

  // Returns true if profile has signed into chrome.
  bool IsAccountConnected(Profile* profile) const;
  // Returns true if profile has authentication error.
  bool HasAuthError(Profile* profile) const;

  base::WeakPtrFactory<SigninCreateProfileHandler> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninCreateProfileHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_CREATE_PROFILE_HANDLER_H_
