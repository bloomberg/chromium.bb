// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/user_image_downloader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

namespace {


// Prefix for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
// Suffix for Auth token received from ClientLogin request.
const char kAuthSuffix[] = "\n";

}  // namespace

class LoginUtilsImpl : public LoginUtils {
 public:
  LoginUtilsImpl()
      : browser_launch_enabled_(true) {
  }

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Invoked after the tmpfs is successfully mounted.
  // Launches a browser in the off the record (incognito) mode.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url);

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

  // Used to postpone browser launch via DoBrowserLaunch() if some post
  // login screen is to be shown.
  virtual void EnableBrowserLaunch(bool enable);

  // Returns if browser launch enabled now or not.
  virtual bool IsBrowserLaunchEnabled() const;

 private:
  // Indicates if DoBrowserLaunch will actually launch the browser or not.
  bool browser_launch_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  LoginUtilsWrapper() {}

  LoginUtils* get() {
    AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::CompleteLogin(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  BootTimesLoader* btl = BootTimesLoader::Get();

  VLOG(1) << "Completing login for " << username;
  btl->AddLoginTimeMarker("CompletingLogin", false);

  if (CrosLibrary::Get()->EnsureLoaded()) {
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(username, "");
    btl->AddLoginTimeMarker("StartedSession", false);
  }

  bool first_login = !UserManager::Get()->IsKnownUser(username);
  UserManager::Get()->UserLoggedIn(username);
  btl->AddLoginTimeMarker("UserLoggedIn", false);

  // Now get the new profile.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Switch log file as soon as possible.
  logging::RedirectChromeLogging(
      user_data_dir.Append(profile_manager->GetCurrentProfileDir()),
      *(CommandLine::ForCurrentProcess()),
      logging::DELETE_OLD_LOG_FILE);
  btl->AddLoginTimeMarker("LoggingRedirected", false);

  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  // Take the credentials passed in and try to exchange them for
  // full-fledged Google authentication cookies.  This is
  // best-effort; it's possible that we'll fail due to network
  // troubles or some such.  Either way, |cf| will call
  // DoBrowserLaunch on the UI thread when it's done, and then
  // delete itself.
  CookieFetcher* cf = new CookieFetcher(profile);
  cf->AttemptFetch(credentials.data);
  btl->AddLoginTimeMarker("CookieFetchStarted", false);

  // Init extension event routers; this normally happens in browser_main
  // but on Chrome OS it has to be deferred until the user finishes
  // logging in and the profile is not OTR.
  if (profile->GetExtensionsService() &&
      profile->GetExtensionsService()->extensions_enabled()) {
    profile->GetExtensionsService()->InitEventRouters();
  }
  btl->AddLoginTimeMarker("ExtensionsServiceStarted", false);

  // Supply credentials for sync and others to use. Load tokens from disk.
  TokenService* token_service = profile->GetTokenService();
  token_service->Initialize(GaiaConstants::kChromeOSSource,
                            profile);
  token_service->LoadTokensFromDB();
  token_service->UpdateCredentials(credentials);
  if (token_service->AreCredentialsValid()) {
    token_service->StartFetchingTokens();
  }
  btl->AddLoginTimeMarker("TokensGotten", false);

  // Set the CrOS user by getting this constructor run with the
  // user's email on first retrieval.
  profile->GetProfileSyncService(username);
  btl->AddLoginTimeMarker("SyncStarted", false);

  // Attempt to take ownership; this will fail if device is already owned.
  OwnershipService::GetSharedInstance()->StartTakeOwnershipAttempt(
      UserManager::Get()->logged_in_user().email());
  // Own TPM device if, for any reason, it has not been done in EULA
  // wizard screen.
  if (CrosLibrary::Get()->EnsureLoaded()) {
    CryptohomeLibrary* cryptohome = CrosLibrary::Get()->GetCryptohomeLibrary();
    if (cryptohome->TpmIsEnabled() && !cryptohome->TpmIsBeingOwned()) {
      if (cryptohome->TpmIsOwned()) {
        cryptohome->TpmClearStoredPassword();
      } else {
        cryptohome->TpmCanAttemptOwnership();
      }
    }
  }
  btl->AddLoginTimeMarker("TPMOwned", false);

  static const char kFallbackInputMethodLocale[] = "en-US";
  if (first_login) {
    std::string locale(g_browser_process->GetApplicationLocale());
    // Add input methods based on the application locale when the user first
    // logs in. For instance, if the user chooses Japanese as the UI
    // language at the first login, we'll add input methods associated with
    // Japanese, such as mozc.
    if (locale != kFallbackInputMethodLocale) {
      StringPrefMember language_preload_engines;
      language_preload_engines.Init(prefs::kLanguagePreloadEngines,
                                    profile->GetPrefs(), NULL);
      StringPrefMember language_preferred_languages;
      language_preferred_languages.Init(prefs::kLanguagePreferredLanguages,
                                        profile->GetPrefs(), NULL);

      std::string preload_engines(language_preload_engines.GetValue());
      std::vector<std::string> input_method_ids;
      input_method::GetInputMethodIdsFromLanguageCode(
          locale, input_method::kAllInputMethods, &input_method_ids);
      if (!input_method_ids.empty()) {
        if (!preload_engines.empty())
          preload_engines += ',';
        preload_engines += input_method_ids[0];
      }
      language_preload_engines.SetValue(preload_engines);

      // Add the UI language to the preferred languages the user first logs in.
      std::string preferred_languages(locale);
      preferred_languages += ",";
      preferred_languages += kFallbackInputMethodLocale;
      language_preferred_languages.SetValue(preferred_languages);
      btl->AddLoginTimeMarker("IMESTarted", false);
    }
  }
  DoBrowserLaunch(profile);
}

void LoginUtilsImpl::CompleteOffTheRecordLogin(const GURL& start_url) {
  VLOG(1) << "Completing off the record login";

  UserManager::Get()->OffTheRecordUserLoggedIn();

  if (CrosLibrary::Get()->EnsureLoaded()) {
    // For guest session we ask session manager to restart Chrome with --bwsi
    // flag. We keep only some of the arguments of this process.
    static const char* kForwardSwitches[] = {
        switches::kLoggingLevel,
        switches::kEnableLogging,
        switches::kUserDataDir,
        switches::kScrollPixels,
        switches::kEnableGView,
        switches::kNoFirstRun,
        switches::kLoginProfile
    };
    const CommandLine& browser_command_line =
        *CommandLine::ForCurrentProcess();
    CommandLine command_line(browser_command_line.GetProgram());
    command_line.CopySwitchesFrom(browser_command_line,
                                  kForwardSwitches,
                                  arraysize(kForwardSwitches));
    command_line.AppendSwitch(switches::kGuestSession);
    command_line.AppendSwitch(switches::kIncognito);
    command_line.AppendSwitch(switches::kEnableTabbedOptions);
    command_line.AppendSwitchASCII(
        switches::kLoginUser,
        UserManager::Get()->logged_in_user().email());
    if (start_url.is_valid())
      command_line.AppendArg(start_url.spec());
    CrosLibrary::Get()->GetLoginLibrary()->RestartJob(
        getpid(),
        command_line.command_line_string());
  }
}

Authenticator* LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kParallelAuth))
    return new ParallelAuthenticator(consumer);
  else
    return new GoogleAuthenticator(consumer);
}

void LoginUtilsImpl::EnableBrowserLaunch(bool enable) {
  browser_launch_enabled_ = enable;
}

bool LoginUtilsImpl::IsBrowserLaunchEnabled() const {
  return browser_launch_enabled_;
}

LoginUtils* LoginUtils::Get() {
  return Singleton<LoginUtilsWrapper>::get()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  Singleton<LoginUtilsWrapper>::get()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile) {
  BootTimesLoader::Get()->AddLoginTimeMarker("BrowserLaunched", false);
  // Browser launch was disabled due to some post login screen.
  if (!LoginUtils::Get()->IsBrowserLaunchEnabled())
    return;

  // Update command line in case loose values were added.
  CommandLine::ForCurrentProcess()->InitFromArgv(
      CommandLine::ForCurrentProcess()->argv());

  VLOG(1) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             FilePath(),
                             true,
                             &return_code);
}

}  // namespace chromeos
