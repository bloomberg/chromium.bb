// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/user_image_downloader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/proxy_config_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

namespace {

// Affixes for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
const char kAuthSuffix[] = "\n";

// Increase logging level for Guest mode to avoid LOG(INFO) messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// Resets the proxy configuration service for the default request context.
class ResetDefaultProxyConfigServiceTask : public Task {
 public:
  ResetDefaultProxyConfigServiceTask(
      net::ProxyConfigService* proxy_config_service)
      : proxy_config_service_(proxy_config_service) {}
  virtual ~ResetDefaultProxyConfigServiceTask() {}

  // Task override.
  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
    DCHECK(getter);
    if (getter) {
      getter->GetURLRequestContext()->proxy_service()->ResetConfigService(
          proxy_config_service_.release());
    }
  }

 private:
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;

  DISALLOW_COPY_AND_ASSIGN(ResetDefaultProxyConfigServiceTask);
};

}  // namespace

class LoginUtilsImpl : public LoginUtils {
 public:
  LoginUtilsImpl()
      : browser_launch_enabled_(true),
        background_view_(NULL) {
  }

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests);

  // Invoked after the tmpfs is successfully mounted.
  // Launches a browser in the off the record (incognito) mode.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url);

  // Invoked when the user is logging in for the first time, or is logging in as
  // a guest user.
  virtual void SetFirstLoginPrefs(PrefService* prefs);

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

  // Used to postpone browser launch via DoBrowserLaunch() if some post
  // login screen is to be shown.
  virtual void EnableBrowserLaunch(bool enable);

  // Returns if browser launch enabled now or not.
  virtual bool IsBrowserLaunchEnabled() const;

  // Warms the url used by authentication.
  virtual void PrewarmAuthentication();

  // Given the credentials try to exchange them for
  // full-fledged Google authentication cookies.
  virtual void FetchCookies(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Supply credentials for sync and others to use.
  virtual void FetchTokens(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Sets the current background view.
  virtual void SetBackgroundView(chromeos::BackgroundView* background_view);

  // Gets the current background view.
  virtual chromeos::BackgroundView* GetBackgroundView();

 protected:
  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine *command_line);

 private:
  // Check user's profile for kApplicationLocale setting.
  void RespectLocalePreference(Profile* pref);

  // Indicates if DoBrowserLaunch will actually launch the browser or not.
  bool browser_launch_enabled_;

  // The current background view.
  chromeos::BackgroundView* background_view_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  static LoginUtilsWrapper* GetInstance() {
    return Singleton<LoginUtilsWrapper>::get();
  }

  LoginUtils* get() {
    base::AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  friend struct DefaultSingletonTraits<LoginUtilsWrapper>;

  LoginUtilsWrapper() {}

  base::Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::CompleteLogin(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
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
  logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));
  btl->AddLoginTimeMarker("LoggingRedirected", false);

  Profile* profile = NULL;
  {
    // Loading user profile causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11104
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    // The default profile will have been changed because the ProfileManager
    // will process the notification that the UserManager sends out.
    profile = profile_manager->GetDefaultProfile(user_data_dir);
  }
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  // Change the proxy configuration service of the default request context to
  // use the preference configuration from the logged-in profile. This ensures
  // that requests done through the default context use the proxy configuration
  // provided by configuration policy.
  //
  // Note: Many of the clients of the default request context should probably be
  // fixed to use the request context of the profile they are associated with.
  // This includes preconnect, autofill, metrics service to only name a few;
  // see http://code.google.com/p/chromium/issues/detail?id=64339 for details.
  //
  // TODO(mnissler) Revisit when support for device-specific policy arrives, at
  // which point the default request context can directly be managed through
  // device policy.
  net::ProxyConfigService* proxy_config_service =
      new PrefProxyConfigService(
          profile->GetProxyConfigTracker(),
          new chromeos::ProxyConfigService(
              profile->GetChromeOSProxyConfigServiceImpl()));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          new ResetDefaultProxyConfigServiceTask(
                              proxy_config_service));

  // Since we're doing parallel authentication, only new user sign in
  // would perform online auth before calling CompleteLogin.
  // For existing users there's usually a pending online auth request.
  // Cookies will be fetched after it's is succeeded.
  if (!pending_requests) {
    FetchCookies(profile, credentials);
  }

  // Init extension event routers; this normally happens in browser_main
  // but on Chrome OS it has to be deferred until the user finishes
  // logging in and the profile is not OTR.
  if (profile->GetExtensionService() &&
      profile->GetExtensionService()->extensions_enabled()) {
    profile->GetExtensionService()->InitEventRouters();
  }
  btl->AddLoginTimeMarker("ExtensionsServiceStarted", false);

  // Supply credentials for sync and others to use. Load tokens from disk.
  TokenService* token_service = profile->GetTokenService();
  token_service->Initialize(GaiaConstants::kChromeOSSource,
                            profile);
  token_service->LoadTokensFromDB();

  // For existing users there's usually a pending online auth request.
  // Tokens will be fetched after it's is succeeded.
  if (!pending_requests) {
    FetchTokens(profile, credentials);
  }
  btl->AddLoginTimeMarker("TokensGotten", false);

  // Set the CrOS user by getting this constructor run with the
  // user's email on first retrieval.
  profile->GetProfileSyncService(username)->SetPassphrase(password,
                                                          false,
                                                          true);
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

  RespectLocalePreference(profile);

  if (first_login) {
    SetFirstLoginPrefs(profile->GetPrefs());
  }

  // Enable/disable plugins based on user preferences.
  PluginUpdater::GetInstance()->DisablePluginGroupsFromPrefs(profile);
  btl->AddLoginTimeMarker("PluginsStateUpdated", false);

  // We suck. This is a hack since we do not have the enterprise feature
  // done yet to pull down policies from the domain admin. We'll take this
  // out when we get that done properly.
  // TODO(xiyuan): Remove this once enterprise feature is ready.
  if (EndsWith(username, "@google.com", true)) {
    PrefService* pref_service = profile->GetPrefs();
    pref_service->SetBoolean(prefs::kEnableScreenLock, true);
  }

  DoBrowserLaunch(profile);
}

void LoginUtilsImpl::FetchCookies(
    Profile* profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Take the credentials passed in and try to exchange them for
  // full-fledged Google authentication cookies.  This is
  // best-effort; it's possible that we'll fail due to network
  // troubles or some such.
  // CookieFetcher will delete itself once done.
  CookieFetcher* cf = new CookieFetcher(profile);
  cf->AttemptFetch(credentials.data);
  BootTimesLoader::Get()->AddLoginTimeMarker("CookieFetchStarted", false);
}

void LoginUtilsImpl::FetchTokens(
    Profile* profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  TokenService* token_service = profile->GetTokenService();
  token_service->UpdateCredentials(credentials);
  if (token_service->AreCredentialsValid()) {
    token_service->StartFetchingTokens();
  }
}

void LoginUtilsImpl::RespectLocalePreference(Profile* profile) {
  DCHECK(profile != NULL);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs != NULL);
  if (g_browser_process == NULL)
    return;

  std::string pref_locale = prefs->GetString(prefs::kApplicationLocale);
  if (pref_locale.empty())
    pref_locale = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (pref_locale.empty())
    pref_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!pref_locale.empty());
  profile->ChangeAppLocale(pref_locale, Profile::APP_LOCALE_CHANGED_VIA_LOGIN);
  // Here we don't enable keyboard layouts. Input methods are set up when
  // the user first logs in. Then the user may customize the input methods.
  // Hence changing input methods here, just because the user's UI language
  // is different from the login screen UI language, is not desirable. Note
  // that input method preferences are synced, so users can use their
  // farovite input methods as soon as the preferences are synced.
  LanguageSwitchMenu::SwitchLanguage(pref_locale);
}

void LoginUtilsImpl::CompleteOffTheRecordLogin(const GURL& start_url) {
  VLOG(1) << "Completing off the record login";

  UserManager::Get()->OffTheRecordUserLoggedIn();

  if (CrosLibrary::Get()->EnsureLoaded()) {
    // For guest session we ask session manager to restart Chrome with --bwsi
    // flag. We keep only some of the arguments of this process.
    const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
    CommandLine command_line(browser_command_line.GetProgram());
    std::string cmd_line_str =
        GetOffTheRecordCommandLine(start_url,
                                   browser_command_line,
                                   &command_line);

    CrosLibrary::Get()->GetLoginLibrary()->RestartJob(getpid(), cmd_line_str);
  }
}

std::string LoginUtilsImpl::GetOffTheRecordCommandLine(
    const GURL& start_url,
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  static const char* kForwardSwitches[] = {
      switches::kEnableLogging,
      switches::kUserDataDir,
      switches::kScrollPixels,
      switches::kEnableGView,
      switches::kNoFirstRun,
      switches::kLoginProfile,
      switches::kCompressSystemFeedback,
      switches::kDisableSeccompSandbox,
#if defined(HAVE_XINPUT2)
      switches::kTouchDevices,
#endif
  };
  command_line->CopySwitchesFrom(base_command_line,
                                 kForwardSwitches,
                                 arraysize(kForwardSwitches));
  command_line->AppendSwitch(switches::kGuestSession);
  command_line->AppendSwitch(switches::kIncognito);
  command_line->AppendSwitchASCII(switches::kLoggingLevel,
                                 kGuestModeLoggingLevel);

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  // Override the value of the homepage that is set in first run mode.
  // TODO(altimofeev): extend action of the |kNoFirstRun| to cover this case.
  command_line->AppendSwitchASCII(
      switches::kHomePage,
      GURL(chrome::kChromeUINewTabURL).spec());

  std::string cmd_line_str = command_line->command_line_string();
  // Special workaround for the arguments that should be quoted.
  // Copying switches won't be needed when Guest mode won't need restart
  // http://crosbug.com/6924
  if (base_command_line.HasSwitch(switches::kRegisterPepperPlugins)) {
    cmd_line_str += base::StringPrintf(
        kSwitchFormatString,
        switches::kRegisterPepperPlugins,
        base_command_line.GetSwitchValueNative(
            switches::kRegisterPepperPlugins).c_str());
  }

  return cmd_line_str;
}

void LoginUtilsImpl::SetFirstLoginPrefs(PrefService* prefs) {
  VLOG(1) << "Setting first login prefs";
  BootTimesLoader* btl = BootTimesLoader::Get();
  std::string locale = g_browser_process->GetApplicationLocale();

  // First, we'll set kLanguagePreloadEngines.
  InputMethodLibrary* library = CrosLibrary::Get()->GetInputMethodLibrary();
  std::vector<std::string> input_method_ids;
  input_method::GetFirstLoginInputMethodIds(locale,
                                            library->current_input_method(),
                                            &input_method_ids);
  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines,
                                prefs, NULL);
  language_preload_engines.SetValue(JoinString(input_method_ids, ','));
  btl->AddLoginTimeMarker("IMEStarted", false);

  // Second, we'll set kLanguagePreferredLanguages.
  std::vector<std::string> language_codes;
  // The current locale should be on the top.
  language_codes.push_back(locale);

  // Add input method IDs based on the input methods, as there may be
  // input methods that are unrelated to the current locale. Example: the
  // hardware keyboard layout xkb:us::eng is used for logging in, but the
  // UI language is set to French. In this case, we should set "fr,en"
  // to the preferred languages preference.
  std::vector<std::string> candidates;
  input_method::GetLanguageCodesFromInputMethodIds(
      input_method_ids, &candidates);
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Skip if it's already in language_codes.
    if (std::count(language_codes.begin(), language_codes.end(),
                   candidate) == 0) {
      language_codes.push_back(candidate);
    }
  }
  // Save the preferred languages in the user's preferences.
  StringPrefMember language_preferred_languages;
  language_preferred_languages.Init(prefs::kLanguagePreferredLanguages,
                                    prefs, NULL);
  language_preferred_languages.SetValue(JoinString(language_codes, ','));
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

// We use a special class for this so that it can be safely leaked if we
// never connect. At shutdown the order is not well defined, and it's possible
// for the infrastructure needed to unregister might be unstable and crash.
class WarmingObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  WarmingObserver() {
    NetworkLibrary *netlib = CrosLibrary::Get()->GetNetworkLibrary();
    netlib->AddNetworkManagerObserver(this);
  }

  // If we're now connected, prewarm the auth url.
  void OnNetworkManagerChanged(NetworkLibrary* netlib) {
    if (netlib->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaAuthFetcher::kClientLoginUrl),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded);
      netlib->RemoveNetworkManagerObserver(this);
      delete this;
    }
  }
};

void LoginUtilsImpl::PrewarmAuthentication() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    NetworkLibrary *network = CrosLibrary::Get()->GetNetworkLibrary();
    if (network->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaAuthFetcher::kClientLoginUrl),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded);
    } else {
      new WarmingObserver();
    }
  }
}

void LoginUtilsImpl::SetBackgroundView(BackgroundView* background_view) {
  background_view_ = background_view;
}

BackgroundView* LoginUtilsImpl::GetBackgroundView() {
  return background_view_;
}

LoginUtils* LoginUtils::Get() {
  return LoginUtilsWrapper::GetInstance()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  LoginUtilsWrapper::GetInstance()->reset(mock);
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
