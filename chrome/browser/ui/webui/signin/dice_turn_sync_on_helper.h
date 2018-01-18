// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_metrics.h"

class Browser;
class BrowserList;
class ProfileOAuth2TokenService;
class SigninManager;

namespace browser_sync {
class ProfileSyncService;
}

namespace syncer {
class SyncSetupInProgressHandle;
}

// Handles details of signing the user in with SigninManager and turning on
// sync for an account that is already present in the token service.
class DiceTurnSyncOnHelper : public BrowserListObserver,
                             public LoginUIService::Observer {
 public:
  // Behavior when the signin is aborted (by an error or cancelled by the user).
  enum class SigninAbortedMode {
    // The token is revoked and the account is signed out of the web.
    REMOVE_ACCOUNT,
    // The account is kept.
    KEEP_ACCOUNT
  };

  // Create a helper that turns sync on for an account that is already present
  // in the token service.
  DiceTurnSyncOnHelper(Profile* profile,
                       Browser* browser,
                       signin_metrics::AccessPoint signin_access_point,
                       signin_metrics::Reason signin_reason,
                       const std::string& account_id,
                       SigninAbortedMode signin_aborted_mode);

 private:
  enum class ProfileMode {
    // Attempts to sign the user in |profile_|. Note that if the account to be
    // signed in is a managed account, then a profile confirmation dialog is
    // shown and the user has the possibility to create a new profile before
    // signing in.
    CURRENT_PROFILE,

    // Creates a new profile and signs the user in this new profile.
    NEW_PROFILE
  };

  // User input handler for the signin confirmation dialog.
  class SigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
   public:
    explicit SigninDialogDelegate(
        base::WeakPtr<DiceTurnSyncOnHelper> sync_starter);
    ~SigninDialogDelegate() override;
    void OnCancelSignin() override;
    void OnContinueSignin() override;
    void OnSigninWithNewProfile() override;

   private:
    base::WeakPtr<DiceTurnSyncOnHelper> sync_starter_;
  };
  friend class SigninDialogDelegate;

  // DiceTurnSyncOnHelper deletes itself.
  ~DiceTurnSyncOnHelper() override;

  // Handles can offer sign-in errors.  It returns true if there is an error,
  // and false otherwise.
  bool HasCanOfferSigninError();

  // Callback used with ConfirmEmailDialogDelegate.
  void ConfirmEmailAction(SigninEmailConfirmationDialog::Action action);

  // Turns sync on with the current profile or a new profile.
  void TurnSyncOnWithProfileMode(ProfileMode profile_mode);

  // Callback invoked once policy registration is complete. If registration
  // fails, |dm_token| and |client_id| will be empty.
  void OnRegisteredForPolicy(const std::string& dm_token,
                             const std::string& client_id);

  // Helper function that loads policy with the cached |dm_token_| and
  // |client_id|, then completes the signin process.
  void LoadPolicyWithCachedCredentials();

  // Callback invoked when a policy fetch request has completed. |success| is
  // true if policy was successfully fetched.
  void OnPolicyFetchComplete(bool success);

  // Called to create a new profile, which is then signed in with the
  // in-progress auth credentials currently stored in this object.
  void CreateNewSignedInProfile();

  // Callback invoked once a profile is created, so we can complete the
  // credentials transfer, load policy, and open the first window.
  void CompleteInitForNewProfile(Profile* new_profile,
                                 Profile::CreateStatus status);

  // Returns the ProfileSyncService, or nullptr if sync is not allowed.
  browser_sync::ProfileSyncService* GetProfileSyncService();

  // Completes the signin in SigninManager and displays the Sync confirmation
  // UI.
  void SigninAndShowSyncConfirmationUI();

  // LoginUIService::Observer override. Deletes this object.
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  // BrowserListObserver override.
  void OnBrowserRemoved(Browser* browser) override;

  // Aborts the flow and deletes this object.
  void AbortAndDelete();

  Profile* profile_;
  Browser* browser_;
  SigninManager* signin_manager_;
  ProfileOAuth2TokenService* token_service_;
  const signin_metrics::AccessPoint signin_access_point_;
  const signin_metrics::Reason signin_reason_;

  // Whether the refresh token should be deleted if the Sync flow is aborted.
  const SigninAbortedMode signin_aborted_mode_;

  // Account information.
  const AccountInfo account_info_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Policy credentials we keep while determining whether to create
  // a new profile for an enterprise user or not.
  std::string dm_token_;
  std::string client_id_;

  ScopedObserver<BrowserList, BrowserListObserver>
      scoped_browser_list_observer_;
  ScopedObserver<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observer_;

  base::WeakPtrFactory<DiceTurnSyncOnHelper> weak_pointer_factory_;
  DISALLOW_COPY_AND_ASSIGN(DiceTurnSyncOnHelper);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_
