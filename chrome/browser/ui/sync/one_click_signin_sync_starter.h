// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_tracker.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;
class ProfileSyncService;

namespace policy {
class CloudPolicyClient;
}

// Waits for successful singin notification from the signin manager and then
// starts the sync machine.  Instances of this class delete themselves once
// the job is done.
class OneClickSigninSyncStarter : public SigninTracker::Observer {
 public:
  enum StartSyncMode {
    // Starts the process of signing the user in with the SigninManager, and
    // once completed automatically starts sync with all data types enabled.
    SYNC_WITH_DEFAULT_SETTINGS,

    // Starts the process of signing the user in with the SigninManager, and
    // once completed redirects the user to the settings page to allow them
    // to configure which data types to sync before sync is enabled.
    CONFIGURE_SYNC_FIRST,

    // The process should be aborted because the undo button has been pressed.
    UNDO_SYNC
  };

  // |profile| must not be NULL, however |browser| can be. When using the
  // OneClickSigninSyncStarter from a browser, provide both.
  // If |display_confirmation| is true, the user will be prompted to confirm the
  // signin before signin completes.
  OneClickSigninSyncStarter(Profile* profile,
                            Browser* browser,
                            const std::string& session_index,
                            const std::string& email,
                            const std::string& password,
                            StartSyncMode start_mode,
                            bool force_same_tab_navigation,
                            bool display_confirmation);

 private:
  virtual ~OneClickSigninSyncStarter();

  // Initializes the internals of the OneClickSigninSyncStarter object. Can also
  // be used to re-initialize the object to refer to a newly created profile.
  void Initialize(Profile* profile, Browser* browser);

  // SigninTracker::Observer override.
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;


#if defined(ENABLE_CONFIGURATION_POLICY)
  // Callback invoked once policy registration is complete. If registration
  // fails, |client| will be null.
  void OnRegisteredForPolicy(scoped_ptr<policy::CloudPolicyClient> client);

  // Callback invoked when a policy fetch request has completed. |success| is
  // true if policy was successfully fetched.
  void OnPolicyFetchComplete(bool success);

  // Called to create a new profile, which is then signed in with the
  // in-progress auth credentials currently stored in this object.
  void CreateNewSignedInProfile();

  // Helper function that loads policy with the passed CloudPolicyClient, then
  // completes the signin process.
  void LoadPolicyWithCachedClient();

  // Callback invoked once a profile is created, so we can complete the
  // credentials transfer and load policy.
  void CompleteSigninForNewProfile(Profile* profile,
                                   Profile::CreateStatus status);

  // Cancels the in-progress signin for this profile.
  void CancelSigninAndDelete();
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

  // Callback invoked to check whether the user needs policy or if a
  // confirmation is required (in which case we have to prompt the user first).
  void ConfirmSignin(const std::string& oauth_token);

  // Displays confirmation UI to the user if confirmation_required_ is true,
  // otherwise completes the pending signin process.
  void SigninAfterSAMLConfirmation();

  // Callback invoked once the user has responded to the signin confirmation UI.
  // If response == UNDO_SYNC, the signin is cancelled, otherwise the pending
  // signin is completed.
  void SigninConfirmationComplete(StartSyncMode response);

  ProfileSyncService* GetProfileSyncService();

  // Displays the sync configuration UI, then frees this object.
  void ConfigureSync();
  void ShowSyncSettingsPageOnSameTab();

  Profile* profile_;
  Browser* browser_;
  scoped_ptr<SigninTracker> signin_tracker_;
  StartSyncMode start_mode_;
  chrome::HostDesktopType desktop_type_;
  bool force_same_tab_navigation_;
  bool confirmation_required_;
  base::WeakPtrFactory<OneClickSigninSyncStarter> weak_pointer_factory_;

#if defined(ENABLE_CONFIGURATION_POLICY)
  // CloudPolicyClient reference we keep while determining whether to create
  // a new profile for an enterprise user or not.
  scoped_ptr<policy::CloudPolicyClient> policy_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarter);
};


#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
