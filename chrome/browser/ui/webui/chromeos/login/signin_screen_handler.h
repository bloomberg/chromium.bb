// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_ui.h"

class BrowsingDataRemover;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class CaptivePortalWindowProxy;
class NetworkStateInformer;
class User;

// An interface for WebUILoginDisplay to call SigninScreenHandler.
class LoginDisplayWebUIHandler {
 public:
  virtual void ClearAndEnablePassword() = 0;
  virtual void OnLoginSuccess(const std::string& username) = 0;
  virtual void OnUserRemoved(const std::string& username) = 0;
  virtual void OnUserImageChanged(const User& user) = 0;
  virtual void OnPreferencesChanged() = 0;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;
  virtual void ShowGaiaPasswordChanged(const std::string& username) = 0;
  // Show siginin screen for the given credentials.
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) = 0;
  virtual void ResetSigninScreenHandlerDelegate() = 0;
 protected:
  virtual ~LoginDisplayWebUIHandler() {}
};

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // Confirms sign up by provided |username| and |password| specified.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const std::string& username,
                             const std::string& password) = 0;

  // Sign in using |username| and |password| specified.
  // Used for both known and new users.
  virtual void Login(const std::string& username,
                     const std::string& password) = 0;

  // Sign in into a demo user session.
  virtual void LoginAsDemoUser() = 0;

  // Sign in into Guest session.
  virtual void LoginAsGuest() = 0;

  // Signs out if the screen is currently locked.
  virtual void Signout() = 0;

  // Create a new Google account.
  virtual void CreateAccount() = 0;

  // Called when user pod with |username| is selected at login screen.
  // |username| is the email address of the selected user.
  virtual void UserSelected(const std::string& username) = 0;

  // Attempts to remove given user.
  virtual void RemoveUser(const std::string& username) = 0;

  // Shows Enterprise Enrollment screen.
  virtual void ShowEnterpriseEnrollmentScreen() = 0;

  // Let the delegate know about the handler it is supposed to be using.
  virtual void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) = 0;

  // Returns users list to be shown.
  virtual const UserList& GetUsers() const = 0;

  // Whether login as guest is available.
  virtual bool IsShowGuest() const = 0;

  // Whether login as guest is available.
  virtual bool IsShowUsers() const = 0;

  // Whether new user pod is available.
  virtual bool IsShowNewUser() const = 0;

  // Sets the displayed email for the next login attempt. If it succeeds,
  // user's displayed email value will be updated to |email|.
  virtual void SetDisplayEmail(const std::string& email) = 0;

 protected:
  virtual ~SigninScreenHandlerDelegate() {}
};

// A class that handles the WebUI hooks in sign-in screen in OobeDisplay
// and LoginDisplay.
class SigninScreenHandler : public BaseScreenHandler,
                            public LoginDisplayWebUIHandler,
                            public BrowsingDataRemover::Observer,
                            public SystemKeyEventListener::CapsLockObserver,
                            public content::NotificationObserver {
 public:
  SigninScreenHandler();
  virtual ~SigninScreenHandler();

  // Shows the sign in screen. |oobe_ui| indicates whether the signin
  // screen is for OOBE or usual sign-in flow.
  void Show(bool oobe_ui);

  // Sets delegate to be used by the handler. It is guaranteed that valid
  // delegate is set before Show() method will be called.
  void SetDelegate(SigninScreenHandlerDelegate* delegate);

  // Called when network is connected.
  void OnNetworkReady();

 private:
  friend class ReportDnsCacheClearedOnUIThread;

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // BaseLoginUIHandler implementation.
  virtual void ClearAndEnablePassword() OVERRIDE;
  virtual void OnLoginSuccess(const std::string& username) OVERRIDE;
  virtual void OnUserRemoved(const std::string& username) OVERRIDE;
  virtual void OnUserImageChanged(const User& user) OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) OVERRIDE;
  virtual void ShowGaiaPasswordChanged(const std::string& username) OVERRIDE;
  virtual void ResetSigninScreenHandlerDelegate() OVERRIDE;

  // BrowsingDataRemover::Observer overrides.
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // SystemKeyEventListener::CapsLockObserver overrides.
  virtual void OnCapsLockChange(bool enabled) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowSigninScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |silent_load| is true
  // for cases when extension should be loaded in the background and it
  // shouldn't grab the focus. |offline| is true when offline version of the
  // extension should be used.
  void LoadAuthExtension(bool force, bool silent_load, bool offline);

  // Updates authentication extension. Called when device settings that affect
  // sign-in (allow BWSI and allow whitelist) are changed.
  void UpdateAuthExtension();

  // WebUI message handlers.
  void HandleCompleteLogin(const base::ListValue* args);
  void HandleGetUsers(const base::ListValue* args);
  void HandleAuthenticateUser(const base::ListValue* args);
  void HandleLaunchDemoUser(const base::ListValue* args);
  void HandleLaunchIncognito(const base::ListValue* args);
  void HandleFixCaptivePortal(const base::ListValue* args);
  void HandleShowCaptivePortal(const base::ListValue* args);
  void HandleHideCaptivePortal(const base::ListValue* args);
  void HandleOfflineLogin(const base::ListValue* args);
  void HandleShutdownSystem(const base::ListValue* args);
  void HandleUserSelected(const base::ListValue* args);
  void HandleRemoveUser(const base::ListValue* args);
  void HandleShowAddUser(const base::ListValue* args);
  void HandleToggleEnrollmentScreen(const base::ListValue* args);
  void HandleLaunchHelpApp(const base::ListValue* args);
  void HandleCreateAccount(const base::ListValue* args);
  void HandleAccountPickerReady(const base::ListValue* args);
  void HandleLoginWebuiReady(const base::ListValue* args);
  void HandleLoginRequestNetworkState(const base::ListValue* args);
  void HandleLoginAddNetworkStateObserver(const base::ListValue* args);
  void HandleLoginRemoveNetworkStateObserver(const base::ListValue* args);
  void HandleSignOutUser(const base::ListValue* args);
  void HandleUserImagesLoaded(const base::ListValue* args);
  void HandleNetworkErrorShown(const base::ListValue* args);
  void HandleOpenProxySettings(const base::ListValue* args);

  // Sends user list to account picker.
  void SendUserList(bool animated);

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies();

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Decides whether an auth extension should be pre-loaded. If it should,
  // pre-loads it.
  void MaybePreloadAuthExtension();

  // A delegate that glues this handler with backend LoginDisplay.
  SigninScreenHandlerDelegate* delegate_;

  // Whether screen should be shown right after initialization.
  bool show_on_init_;

  // Keeps whether screen should be shown for OOBE.
  bool oobe_ui_;

  // Is focus still stolen from Gaia page?
  bool focus_stolen_;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // Is account picker being shown for the first time.
  bool is_account_picker_showing_first_time_;

  // True if dns cache cleanup is done.
  bool dns_cleared_;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Network state informer used to keep offline message screen up.
  scoped_ptr<NetworkStateInformer> network_state_informer_;

  // Email to pre-populate with.
  std::string email_;
  // Emails of the users, whose passwords have recently been changed.
  std::set<std::string> password_changed_for_;

  // Proxy which manages showing of the window for captive portal entering.
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;

  BrowsingDataRemover* cookie_remover_;

  base::WeakPtrFactory<SigninScreenHandler> weak_factory_;

  // CapsLock state change notifier instance;
  SystemKeyEventListener* key_event_listener_;

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
