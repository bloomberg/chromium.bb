// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CREATE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CREATE_PROFILE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"


namespace base {
class DictionaryValue;
class ListValue;
}

class SupervisedUserRegistrationUtility;

namespace options {

// Handler for the 'create profile' overlay.
class CreateProfileHandler: public OptionsPageUIHandler {
 public:
  CreateProfileHandler();
  virtual ~CreateProfileHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Represents the final profile creation status. It is used to map
  // the status to the javascript method to be called.
  enum ProfileCreationStatus {
    PROFILE_CREATION_SUCCESS,
    PROFILE_CREATION_ERROR,
  };

  // Represents errors that could occur during a profile creation.
  // It is used to map error types to messages that will be displayed
  // to the user.
  enum ProfileCreationErrorType {
    REMOTE_ERROR,
    LOCAL_ERROR,
    SIGNIN_ERROR
  };

  // Represents the type of the in progress profile creation operation.
  // It is used to map the type of the profile creation operation to the
  // correct UMA metric name.
  enum ProfileCreationOperationType {
    SUPERVISED_PROFILE_CREATION,
    SUPERVISED_PROFILE_IMPORT,
    NON_SUPERVISED_PROFILE_CREATION,
    NO_CREATION_IN_PROGRESS
  };

  // Asynchronously creates and initializes a new profile.
  // The arguments are as follows:
  //   0: name (string)
  //   1: icon (string)
  //   2: a flag stating whether we should create a profile desktop shortcut
  //      (optional, boolean)
  //   3: a flag stating whether the user should be supervised
  //      (optional, boolean)
  //   4: a string representing the supervised user ID.
  void CreateProfile(const base::ListValue* args);

  // If a local error occurs during profile creation, then show an appropriate
  // error message. However, if profile creation succeeded and the
  // profile being created/imported is a supervised user profile,
  // then proceed with the registration step. Otherwise, update the UI
  // as the final task after a new profile has been created.
  void OnProfileCreated(bool create_shortcut,
                        chrome::HostDesktopType desktop_type,
                        const std::string& supervised_user_id,
                        Profile* profile,
                        Profile::CreateStatus status);

  void HandleProfileCreationSuccess(bool create_shortcut,
                                    chrome::HostDesktopType desktop_type,
                                    const std::string& supervised_user_id,
                                    Profile* profile);

  // After a new supervised-user profile has been created, registers the user
  // with the management server.
  void RegisterSupervisedUser(bool create_shortcut,
                              chrome::HostDesktopType desktop_type,
                              const std::string& supervised_user_id,
                              Profile* new_profile);

  // Called back with the result of the supervised user registration.
  void OnSupervisedUserRegistered(bool create_shortcut,
                                  chrome::HostDesktopType desktop_type,
                                  Profile* profile,
                                  const GoogleServiceAuthError& error);

  // Creates desktop shortcut and updates the UI to indicate success
  // when creating a profile.
  void CreateShortcutAndShowSuccess(bool create_shortcut,
                                    chrome::HostDesktopType desktop_type,
                                    Profile* profile);

  // Updates the UI to show an error when creating a profile.
  void ShowProfileCreationError(Profile* profile, const base::string16& error);

  // Updates the UI to show a non-fatal warning when creating a profile.
  void ShowProfileCreationWarning(const base::string16& warning);

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

  // Records UMA histograms relevant to profile creation.
  void RecordProfileCreationMetrics(Profile::CreateStatus status);

  // Records UMA histograms relevant to supervised user profiles
  // creation and registration.
  void RecordSupervisedProfileCreationMetrics(
      GoogleServiceAuthError::State error_state);

  base::string16 GetProfileCreationErrorMessage(
      ProfileCreationErrorType error) const;
  std::string GetJavascriptMethodName(ProfileCreationStatus status) const;

  bool IsValidExistingSupervisedUserId(
      const std::string& existing_supervised_user_id) const;

  // Used to allow cancelling a profile creation (particularly a supervised-user
  // registration) in progress. Set when profile creation is begun, and
  // cleared when all the callbacks have been run and creation is complete.
  base::FilePath profile_path_being_created_;

  // Used to track how long profile creation takes.
  base::TimeTicks profile_creation_start_time_;

  scoped_ptr<SupervisedUserRegistrationUtility>
      supervised_user_registration_utility_;

  // Indicates the type of the in progress profile creation operation.
  // The value is only relevant while we are creating/importing a profile.
  ProfileCreationOperationType profile_creation_type_;

  base::WeakPtrFactory<CreateProfileHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreateProfileHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CREATE_PROFILE_HANDLER_H_
