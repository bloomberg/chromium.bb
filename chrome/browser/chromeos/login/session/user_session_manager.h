// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

class GURL;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace user_manager {
class User;
}  // namespace user_manager

namespace chromeos {

namespace test {
class UserSessionManagerTestApi;
}  // namespace test

class EasyUnlockKeyManager;
class InputEventsBlocker;
class LoginDisplayHost;

class UserSessionManagerDelegate {
 public:
  // Called after profile is loaded and prepared for the session.
  // |browser_launched| will be true is browser has been launched, otherwise
  // it will return false and client is responsible on launching browser.
  virtual void OnProfilePrepared(Profile* profile,
                                 bool browser_launched) = 0;

 protected:
  virtual ~UserSessionManagerDelegate();
};

class UserSessionStateObserver {
 public:
  // Called when UserManager finishes restoring user sessions after crash.
  virtual void PendingUserSessionsRestoreFinished();

 protected:
  virtual ~UserSessionStateObserver();
};

// UserSessionManager is responsible for starting user session which includes:
// * load and initialize Profile (including custom Profile preferences),
// * mark user as logged in and notify observers,
// * initialize OAuth2 authentication session,
// * initialize and launch user session based on the user type.
// Also supports restoring active user sessions after browser crash:
// load profile, restore OAuth authentication session etc.
class UserSessionManager
    : public OAuth2LoginManager::Observer,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public base::SupportsWeakPtr<UserSessionManager>,
      public UserSessionManagerDelegate,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Context of StartSession calls.
  typedef enum {
    // Starting primary user session, through login UI.
    PRIMARY_USER_SESSION,

    // Starting secondary user session, through multi-profiles login UI.
    SECONDARY_USER_SESSION,

    // Starting primary user session after browser crash.
    PRIMARY_USER_SESSION_AFTER_CRASH,

    // Starting secondary user session after browser crash.
    SECONDARY_USER_SESSION_AFTER_CRASH,
  } StartSessionType;

  // Returns UserSessionManager instance.
  static UserSessionManager* GetInstance();

  // Called when user is logged in to override base::DIR_HOME path.
  static void OverrideHomedir();

  // Registers session related preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Invoked after the tmpfs is successfully mounted.
  // Asks session_manager to restart Chrome in Guest session mode.
  // |start_url| is an optional URL to be opened in Guest session browser.
  void CompleteGuestSessionLogin(const GURL& start_url);

  // Creates and returns the authenticator to use.
  // Single Authenticator instance is used for entire login process,
  // even for multiple retries. Authenticator instance holds reference to
  // login profile and is later used during fetching of OAuth tokens.
  scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer);

  // Start user session given |user_context|.
  // OnProfilePrepared() will be called on |delegate| once Profile is ready.
  void StartSession(const UserContext& user_context,
                    StartSessionType start_session_type,
                    bool has_auth_cookies,
                    bool has_active_session,
                    UserSessionManagerDelegate* delegate);

  // Invalidates |delegate|, which was passed to StartSession method call.
  void DelegateDeleted(UserSessionManagerDelegate* delegate);

  // Perform additional actions once system wide notification
  // "UserLoggedIn" has been sent.
  void PerformPostUserLoggedInActions();

  // Restores authentication session after crash.
  void RestoreAuthenticationSession(Profile* profile);

  // Usually is called when Chrome is restarted after a crash and there's an
  // active session. First user (one that is passed with --login-user) Chrome
  // session has been already restored at this point. This method asks session
  // manager for all active user sessions, marks them as logged in
  // and notifies observers.
  void RestoreActiveSessions();

  // Returns true iff browser has been restarted after crash and
  // UserSessionManager finished restoring user sessions.
  bool UserSessionsRestored() const;

  // Returns true iff browser has been restarted after crash and
  // user sessions restoration is in progress.
  bool UserSessionsRestoreInProgress() const;

  // Initialize RLZ.
  void InitRlz(Profile* profile);

  // Get the NSS cert database for the user represented with |profile|
  // and start certificate loader with it.
  void InitializeCerts(Profile* profile);

  // Starts loading CRL set.
  void InitializeCRLSetFetcher(const user_manager::User* user);

  // Starts loading EV Certificates whitelist.
  void InitializeEVCertificatesWhitelistComponent(
      const user_manager::User* user);

  // Invoked when the user is logging in for the first time, or is logging in to
  // an ephemeral session type, such as guest or a public session.
  void SetFirstLoginPrefs(Profile* profile,
                          const std::string& public_session_locale,
                          const std::string& public_session_input_method);

  // Gets/sets Chrome OAuth client id and secret for kiosk app mode. The default
  // values can be overridden with kiosk auth file.
  bool GetAppModeChromeClientOAuthInfo(
      std::string* chrome_client_id,
      std::string* chrome_client_secret);
  void SetAppModeChromeClientOAuthInfo(
      const std::string& chrome_client_id,
      const std::string& chrome_client_secret);

  // Thin wrapper around StartupBrowserCreator::LaunchBrowser().  Meant to be
  // used in a Task posted to the UI thread.  Once the browser is launched the
  // login host is deleted.
  void DoBrowserLaunch(Profile* profile, LoginDisplayHost* login_host);

  // Changes browser locale (selects best suitable locale from different
  // user settings). Returns true if callback will be called.
  bool RespectLocalePreference(
      Profile* profile,
      const user_manager::User* user,
      const locale_util::SwitchLanguageCallback& callback) const;

  // Restarts Chrome if needed. This happens when user session has custom
  // flags/switches enabled. Another case when owner has setup custom flags,
  // they are applied on login screen as well but not to user session.
  // |early_restart| is true if this restart attempt happens before user profile
  // is fully initialized.
  // Might not return if restart is possible right now.
  // Returns true if restart was scheduled.
  // Returns false if no restart is needed.
  bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                           bool early_restart);

  // Returns true if Easy unlock keys needs to be updated.
  bool NeedsToUpdateEasyUnlockKeys() const;

  // Returns true if there are pending Easy unlock key operations and
  // |callback| will be invoked when it is done.
  bool CheckEasyUnlockKeyOps(const base::Closure& callback);

  void AddSessionStateObserver(chromeos::UserSessionStateObserver* observer);
  void RemoveSessionStateObserver(chromeos::UserSessionStateObserver* observer);

  void ActiveUserChanged(const user_manager::User* active_user) override;

  // Returns default IME state for user session.
  scoped_refptr<input_method::InputMethodManager::State> GetDefaultIMEState(
      Profile* profile);

  // Note this could return NULL if not enabled.
  EasyUnlockKeyManager* GetEasyUnlockKeyManager();

  // Update Easy unlock cryptohome keys for given user context.
  void UpdateEasyUnlockKeys(const UserContext& user_context);

  // Removes a profile from the per-user input methods states map.
  void RemoveProfileForTesting(Profile* profile);

  bool has_auth_cookies() const { return has_auth_cookies_; }

 private:
  friend class test::UserSessionManagerTestApi;
  friend struct DefaultSingletonTraits<UserSessionManager>;

  typedef std::set<std::string> SigninSessionRestoreStateSet;

  UserSessionManager();
  ~UserSessionManager() override;

  // OAuth2LoginManager::Observer overrides:
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // UserSessionManagerDelegate overrides:
  // Used when restoring user sessions after crash.
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  void ChildAccountStatusReceivedCallback();

  void StopChildStatusObserving();

  void CreateUserSession(const UserContext& user_context,
                         bool has_auth_cookies);
  void PreStartSession();
  void StartCrosSession();
  void NotifyUserLoggedIn();
  void PrepareProfile();

  // Callback for asynchronous profile creation.
  void OnProfileCreated(const UserContext& user_context,
                        bool is_incognito_profile,
                        Profile* profile,
                        Profile::CreateStatus status);

  // Callback for Profile::CREATE_STATUS_CREATED profile state.
  // Initializes basic preferences for newly created profile. Any other
  // early profile initialization that needs to happen before
  // ProfileManager::DoFinalInit() gets called is done here.
  void InitProfilePreferences(Profile* profile,
                              const UserContext& user_context);

  // Callback for Profile::CREATE_STATUS_INITIALIZED profile state.
  // Profile is created, extensions and promo resources are initialized.
  void UserProfileInitialized(Profile* profile,
                              bool is_incognito_profile,
                              const std::string& user_id);

  // Callback to resume profile creation after transferring auth data from
  // the authentication profile.
  void CompleteProfileCreateAfterAuthTransfer(Profile* profile);

  // Finalized profile preparation.
  void FinalizePrepareProfile(Profile* profile);

  // Starts out-of-box flow with the specified screen.
  void ActivateWizard(const std::string& screen_name);

  // Adds first-time login URLs.
  void InitializeStartUrls() const;

  // Perform session initialization and either move to additional login flows
  // such as TOS (public sessions), priority pref sync UI (new users) or
  // launch browser.
  // Returns true if browser has been launched or false otherwise.
  bool InitializeUserSession(Profile* profile);

  // Initializes member variables needed for session restore process via
  // OAuthLoginManager.
  void InitSessionRestoreStrategy();

  // Restores GAIA auth cookies for the created user profile from OAuth2 token.
  void RestoreAuthSessionImpl(Profile* profile,
                              bool restore_from_auth_cookies);

  // Initializes RLZ. If |disabled| is true, RLZ pings are disabled.
  void InitRlzImpl(Profile* profile, bool disabled);

  // Callback to process RetrieveActiveSessions() request results.
  void OnRestoreActiveSessions(
      const SessionManagerClient::ActiveSessionsMap& sessions,
      bool success);

  // Called by OnRestoreActiveSessions() when there're user sessions in
  // |pending_user_sessions_| that has to be restored one by one.
  // Also called after first user session from that list is restored and so on.
  // Process continues till |pending_user_sessions_| map is not empty.
  void RestorePendingUserSessions();

  // Notifies observers that user pending sessions restore has finished.
  void NotifyPendingUserSessionsRestoreFinished();

  // Attempts restarting the browser process and esures that this does
  // not happen while we are still fetching new OAuth refresh tokens.
  void AttemptRestart(Profile* profile);

  // Callback invoked when Easy unlock key operations are finished.
  void OnEasyUnlockKeyOpsFinished(const std::string& user_id,
                                  bool success);

  // Internal implementation of DoBrowserLaunch. Initially should be called with
  // |locale_pref_checked| set to false which will result in postponing browser
  // launch till user locale is applied if needed. After locale check has
  // completed this method is called with |locale_pref_checked| set to true.
  void DoBrowserLaunchInternal(Profile* profile,
                               LoginDisplayHost* login_host,
                               bool locale_pref_checked);

  // Switch to the locale that |profile| wishes to use and invoke |callback|.
  void RespectLocalePreferenceWrapper(Profile* profile,
                                      const base::Closure& callback);

  static void RunCallbackOnLocaleLoaded(
      const base::Closure& callback,
      InputEventsBlocker* input_events_blocker,
      const locale_util::LanguageSwitchResult& result);

  // Test API methods.

  // Injects |user_context| that will be used to create StubAuthenticator
  // instance when CreateAuthenticator() is called.
  void InjectStubUserContext(const UserContext& user_context);

  // Controls whether browser instance should be launched after sign in
  // (used in tests).
  void set_should_launch_browser_in_tests(bool should_launch_browser) {
    should_launch_browser_ = should_launch_browser;
  }

  UserSessionManagerDelegate* delegate_;

  // Authentication/user context.
  UserContext user_context_;
  scoped_refptr<Authenticator> authenticator_;
  StartSessionType start_session_type_;

  // Injected user context for stub authenticator.
  scoped_ptr<UserContext> injected_user_context_;

  // True if the authentication context's cookie jar contains authentication
  // cookies from the authentication extension login flow.
  bool has_auth_cookies_;

  // Active user session restoration related members.

  // True if user sessions has been restored after crash.
  // On a normal boot then login into user sessions this will be false.
  bool user_sessions_restored_;

  // True if user sessions restoration after crash is in progress.
  bool user_sessions_restore_in_progress_;

  // User sessions that have to be restored after browser crash.
  // [user_id] > [user_id_hash]
  SessionManagerClient::ActiveSessionsMap pending_user_sessions_;

  ObserverList<chromeos::UserSessionStateObserver> session_state_observer_list_;

  // OAuth2 session related members.

  // True if we should restart chrome right after session restore.
  bool exit_after_session_restore_;

  // Sesion restore strategy.
  OAuth2LoginManager::SessionRestoreStrategy session_restore_strategy_;

  // Set of user_id for those users that we should restore authentication
  // session when notified about online state change.
  SigninSessionRestoreStateSet pending_signin_restore_sessions_;

  // Kiosk mode related members.
  // Chrome oauth client id and secret - override values for kiosk mode.
  std::string chrome_client_id_;
  std::string chrome_client_secret_;

  // Per-user-session Input Methods states.
  std::map<Profile*, scoped_refptr<input_method::InputMethodManager::State>,
      ProfileCompare> default_ime_states_;

  // Manages Easy unlock cryptohome keys.
  scoped_ptr<EasyUnlockKeyManager> easy_unlock_key_manager_;
  bool running_easy_unlock_key_ops_;
  base::Closure easy_unlock_key_ops_finished_callback_;

  // Whether should launch browser, tests may override this value.
  bool should_launch_browser_;

  // Child account status is necessary for InitializeStartUrls call.
  bool waiting_for_child_account_status_;

  base::WeakPtrFactory<UserSessionManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserSessionManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_H_
