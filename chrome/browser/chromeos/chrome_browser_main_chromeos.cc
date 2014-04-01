// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "chrome/browser/chromeos/events/event_rewriter.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/chromeos/extensions/default_app_order.h"
#include "chrome/browser/chromeos/extensions/extension_system_event_observer.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/options/cert_library.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"
#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"
#include "chrome/browser/chromeos/power/power_button_observer.h"
#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_change_notifier_chromeos.h"
#include "chromeos/network/network_change_notifier_factory_chromeos.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm_token_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/power_save_blocker.h"
#include "content/public/common/main_function_params.h"
#include "grit/platform_locale_settings.h"
#include "media/audio/sounds/sounds_manager.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/touch/touch_device.h"
#include "ui/events/event_utils.h"

// Exclude X11 dependents for ozone
#if defined(USE_X11)
#include "chrome/browser/chromeos/device_uma.h"
#endif

namespace chromeos {

namespace {

void ChromeOSVersionCallback(const std::string& version) {
  base::SetLinuxDistro(std::string("CrOS ") + version);
}

// Login -----------------------------------------------------------------------

// Class is used to login using passed username and password.
// The instance will be deleted upon success or failure.
class StubLogin : public LoginStatusConsumer,
                  public LoginUtils::Delegate {
 public:
  StubLogin(std::string username, std::string password)
      : profile_prepared_(false) {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
    authenticator_.get()->AuthenticateToLogin(
        ProfileHelper::GetSigninProfile(),
        UserContext(username, password, std::string() /* auth_code */));
  }

  virtual ~StubLogin() {
    LoginUtils::Get()->DelegateDeleted(this);
  }

  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE {
    LOG(ERROR) << "Login Failure: " << error.GetErrorString();
    delete this;
  }

  virtual void OnLoginSuccess(const UserContext& user_context) OVERRIDE {
    if (!profile_prepared_) {
      // Will call OnProfilePrepared in the end.
      LoginUtils::Get()->PrepareProfile(user_context,
                                        std::string(),  // display_email
                                        false,          // has_cookies
                                        true,           // has_active_session
                                        this);
    } else {
      delete this;
    }
  }

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE {
    std::string login_user =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            chromeos::switches::kLoginUser);
    profile->GetPrefs()->SetString(prefs::kGoogleServicesUsername, login_user);
    profile_prepared_ = true;
    LoginUtils::Get()->DoBrowserLaunch(profile, NULL);
    delete this;
  }

  scoped_refptr<Authenticator> authenticator_;
  bool profile_prepared_;
};

bool ShouldAutoLaunchKioskApp(const CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
      !command_line.HasSwitch(switches::kForceLoginManagerInTests) &&
      app_manager->IsAutoLaunchEnabled() &&
      KioskAppLaunchError::Get() == KioskAppLaunchError::NONE;
}

void RunAutoLaunchKioskApp() {
  ShowLoginWizard(chromeos::WizardController::kAppLaunchSplashScreenName);

  // Login screen is skipped but 'login-prompt-visible' signal is still needed.
  VLOG(1) << "Kiosk app auto launch >> login-prompt-visible";
  DBusThreadManager::Get()->GetSessionManagerClient()->
      EmitLoginPromptVisible();
}

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line,
                                       Profile* profile) {
  if (ShouldAutoLaunchKioskApp(parsed_command_line)) {
    RunAutoLaunchKioskApp();
  } else if (parsed_command_line.HasSwitch(switches::kLoginManager)) {
    const std::string first_screen =
        parsed_command_line.HasSwitch(switches::kLoginScreen) ?
            WizardController::kLoginScreenName : std::string();
    ShowLoginWizard(first_screen);

    if (KioskModeSettings::Get()->IsKioskModeEnabled())
      InitializeKioskModeScreensaver();

    // Reset reboot after update flag when login screen is shown.
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    if (!connector->IsEnterpriseManaged()) {
      PrefService* local_state = g_browser_process->local_state();
      local_state->ClearPref(prefs::kRebootAfterUpdate);
    }
  } else if (parsed_command_line.HasSwitch(switches::kLoginUser) &&
             parsed_command_line.HasSwitch(switches::kLoginPassword)) {
    BootTimesLoader::Get()->RecordLoginAttempted();
    new StubLogin(
        parsed_command_line.GetSwitchValueASCII(switches::kLoginUser),
        parsed_command_line.GetSwitchValueASCII(switches::kLoginPassword));
  } else {
    if (!parsed_command_line.HasSwitch(::switches::kTestName)) {
      // Enable CrasAudioHandler logging when chrome restarts after crashing.
      if (chromeos::CrasAudioHandler::IsInitialized())
        chromeos::CrasAudioHandler::Get()->LogErrors();

      // We did not log in (we crashed or are debugging), so we need to
      // restore Sync.
      LoginUtils::Get()->RestoreAuthenticationSession(profile);
    }
  }
}

}  // namespace

namespace internal {

// Wrapper class for initializing dbus related services and shutting them
// down. This gets instantiated in a scoped_ptr so that shutdown methods in the
// destructor will get called if and only if this has been instantiated.
class DBusServices {
 public:
  explicit DBusServices(const content::MainFunctionParams& parameters) {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      // Override this path on the desktop, so that the user policy key can be
      // stored by the stub SessionManagerClient.
      base::FilePath user_data_dir;
      if (PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
        PathService::Override(chromeos::DIR_USER_POLICY_KEYS,
                              user_data_dir.AppendASCII("stub_user_policy"));
      }
    }

    // Initialize DBusThreadManager for the browser. This must be done after
    // the main message loop is started, as it uses the message loop.
    DBusThreadManager::Initialize();
    CrosDBusService::Initialize();

    // Initialize PowerDataCollector after DBusThreadManager is initialized.
    PowerDataCollector::Initialize();

    LoginState::Initialize();
    SystemSaltGetter::Initialize();
    TPMTokenLoader::Initialize();
    CertLoader::Initialize();

    // This function and SystemKeyEventListener use InputMethodManager.
    chromeos::input_method::Initialize(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::FILE));
    disks::DiskMountManager::Initialize();
    cryptohome::AsyncMethodCaller::Initialize();
    cryptohome::HomedirMethods::Initialize();

    NetworkHandler::Initialize();
    CertLibrary::Initialize();

    // Initialize the network change notifier for Chrome OS. The network
    // change notifier starts to monitor changes from the power manager and
    // the network manager.
    NetworkChangeNotifierFactoryChromeos::GetInstance()->Initialize();

    // Likewise, initialize the upgrade detector for Chrome OS. The upgrade
    // detector starts to monitor changes from the update engine.
    UpgradeDetectorChromeos::GetInstance()->Init();

    if (base::SysInfo::IsRunningOnChromeOS()) {
      // Disable Num Lock on X start up for http://crosbug.com/29169.
      input_method::InputMethodManager::Get()->GetXKeyboard()->DisableNumLock();
    }

    // Initialize the device settings service so that we'll take actions per
    // signals sent from the session manager. This needs to happen before
    // g_browser_process initializes BrowserPolicyConnector.
    DeviceSettingsService::Initialize();
    DeviceSettingsService::Get()->SetSessionManager(
        DBusThreadManager::Get()->GetSessionManagerClient(),
        OwnerKeyUtil::Create());
  }

  ~DBusServices() {
    CertLibrary::Shutdown();
    NetworkHandler::Shutdown();

    cryptohome::AsyncMethodCaller::Shutdown();
    disks::DiskMountManager::Shutdown();
    input_method::Shutdown();

    SystemSaltGetter::Shutdown();
    LoginState::Shutdown();
    CertLoader::Shutdown();
    TPMTokenLoader::Shutdown();

    CrosDBusService::Shutdown();

    // Shutdown the PowerDataCollector before shutting down DBusThreadManager.
    PowerDataCollector::Shutdown();

    // NOTE: This must only be called if Initialize() was called.
    DBusThreadManager::Shutdown();
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(DBusServices);
};

}  //  namespace internal

// ChromeBrowserMainPartsChromeos ----------------------------------------------

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsLinux(parameters) {
}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  if (KioskModeSettings::Get()->IsKioskModeEnabled())
    ShutdownKioskModeScreensaver();

  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutDone", false);
  BootTimesLoader::Get()->WriteLogoutTimes();
}

// content::BrowserMainParts and ChromeBrowserMainExtraParts overrides ---------

void ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();

  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    singleton_command_line->AppendSwitch(::switches::kDisableSync);
    singleton_command_line->AppendSwitch(::switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }

  // If we're not running on real ChromeOS hardware (or under VM), and are not
  // showing the login manager or attempting a command line login, login with a
  // stub user.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !parsed_command_line().HasSwitch(switches::kLoginManager) &&
      !parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kGuestSession)) {
    singleton_command_line->AppendSwitchASCII(
        switches::kLoginUser, UserManager::kStubUser);
    if (!parsed_command_line().HasSwitch(switches::kLoginProfile)) {
      singleton_command_line->AppendSwitchASCII(switches::kLoginProfile,
                                                chrome::kTestUserProfileDir);
    }
    LOG(WARNING) << "Running as stub user with profile dir: "
                 << singleton_command_line->GetSwitchValuePath(
                     switches::kLoginProfile).value();
  }

#if defined(GOOGLE_CHROME_BUILD)
  const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel))
    chrome::VersionInfo::SetChannel(channel);
#endif

  ChromeBrowserMainPartsLinux::PreEarlyInitialization();
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  // Replace the default NetworkChangeNotifierFactory with ChromeOS specific
  // implementation. This must be done before BrowserMainLoop calls
  // net::NetworkChangeNotifier::Create() in MainMessageLoopStart().
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryChromeos());
  ChromeBrowserMainPartsLinux::PreMainMessageLoopStart();
}

void ChromeBrowserMainPartsChromeos::PostMainMessageLoopStart() {
  dbus_services_.reset(new internal::DBusServices(parameters()));

  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
}

// Threads are initialized between MainMessageLoopStart and MainMessageLoopRun.
// about_flags settings are applied in ChromeBrowserMainParts::PreCreateThreads.
void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
  // Set the crypto thread after the IO thread has been created/started.
  TPMTokenLoader::Get()->SetCryptoTaskRunner(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));

  CrasAudioHandler::Initialize(
      AudioDevicesPrefHandler::Create(g_browser_process->local_state()));

  // Start loading machine statistics here. StatisticsProvider::Shutdown()
  // will ensure that loading is aborted on early exit.
  bool load_oem_statistics = !StartupUtils::IsOobeCompleted();
  system::StatisticsProvider::GetInstance()->StartLoadingMachineStatistics(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE),
      load_oem_statistics);

  base::FilePath downloads_directory;
  CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_directory));
  imageburner::BurnManager::Initialize(
      downloads_directory, g_browser_process->system_request_context());

  // Listen for system key events so that the user will be able to adjust the
  // volume on the login screen, if Chrome is running on Chrome OS
  // (i.e. not Linux desktop), and in non-test mode.
  // Note: SystemKeyEventListener depends on the DBus thread.
  if (base::SysInfo::IsRunningOnChromeOS() &&
      !parameters().ui_task) {  // ui_task is non-NULL when running tests.
    SystemKeyEventListener::Initialize();
  }

  DeviceOAuth2TokenServiceFactory::Initialize();

  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // Now that the file thread exists we can record our stats.
  BootTimesLoader::Get()->RecordChromeMainStats();

  // Trigger prefetching of ownership status.
  DeviceSettingsService::Get()->Load();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  UserManager::Initialize();

  // Initialize the screen locker now so that it can receive
  // LOGIN_USER_CHANGED notification from UserManager.
  if (KioskModeSettings::Get()->IsKioskModeEnabled())
    KioskModeIdleLogout::Initialize();
  else
    ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // ProfileHelper has to be initialized after UserManager instance is created.
  g_browser_process->platform_part()->profile_helper()->Initialize();

  // TODO(abarth): Should this move to InitializeNetworkOptions()?
  // Allow access to file:// on ChromeOS for tests.
  if (parsed_command_line().HasSwitch(::switches::kAllowFileAccess))
    ChromeNetworkDelegate::AllowAccessToAllFiles();

  // There are two use cases for kLoginUser:
  //   1) if passed in tandem with kLoginPassword, to drive a "StubLogin"
  //   2) if passed alone, to signal that the indicated user has already
  //      logged in and we should behave accordingly.
  // This handles case 2.
  bool immediate_login =
      parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kLoginPassword);
  if (immediate_login){
    // Redirects Chrome logging to the user data dir.
    logging::RedirectChromeLogging(parsed_command_line());

    // Load the default app order synchronously for restarting case.
    app_order_loader_.reset(
        new default_app_order::ExternalLoader(false /* async */));
  }

  if (!app_order_loader_) {
    app_order_loader_.reset(
        new default_app_order::ExternalLoader(true /* async */));
  }

  media::SoundsManager::Create();

  // Initialize magnification manager before ash tray is created. And this must
  // be placed after UserManager::SessionStarted();
  AccessibilityManager::Initialize();
  MagnificationManager::Initialize();

  // Add observers for WallpaperManager. This depends on PowerManagerClient,
  // TimezoneSettings and CrosSettings.
  WallpaperManager::Get()->AddObservers();

  cros_version_loader_.GetVersion(VersionLoader::VERSION_FULL,
                                  base::Bind(&ChromeOSVersionCallback),
                                  &tracker_);

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests and kiosk app launch by default.
  // Individual tests may enable them if they want.
  if (parsed_command_line().HasSwitch(::switches::kTestType) ||
      ShouldAutoLaunchKioskApp(parsed_command_line())) {
    WizardController::SetZeroDelays();
  }

  power_prefs_.reset(new PowerPrefs(
      DBusThreadManager::Get()->GetPowerPolicyController()));

  // In Aura builds this will initialize ash::Shell.
  ChromeBrowserMainPartsLinux::PreProfileInit();

  if (immediate_login) {
    std::string username =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginUser);
    UserManager* user_manager = UserManager::Get();
    // In case of multi-profiles --login-profile will contain user_id_hash.
    std::string username_hash =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginProfile);
    user_manager->UserLoggedIn(username, username_hash, true);
    VLOG(1) << "Relaunching browser for user: " << username
            << " with hash: " << username_hash;
  }
}

class GuestLanguageSetCallbackData {
 public:
  explicit GuestLanguageSetCallbackData(Profile* profile) : profile(profile) {
  }

  // Must match SwitchLanguageCallback type.
  static void Callback(const scoped_ptr<GuestLanguageSetCallbackData>& self,
                       const std::string& locale,
                       const std::string& loaded_locale,
                       bool success);

  Profile* profile;
};

// static
void GuestLanguageSetCallbackData::Callback(
    const scoped_ptr<GuestLanguageSetCallbackData>& self,
    const std::string& locale,
    const std::string& loaded_locale,
    bool success) {
  input_method::InputMethodManager* const ime_manager =
      input_method::InputMethodManager::Get();
  // Active layout must be hardware "login layout".
  // The previous one must be "locale default layout".
  // First, enable all hardware input methods.
  const std::vector<std::string>& input_methods =
      ime_manager->GetInputMethodUtil()->GetHardwareInputMethodIds();
  for (size_t i = 0; i < input_methods.size(); ++i)
    ime_manager->EnableInputMethod(input_methods[i]);

  // Second, enable locale based input methods.
  const std::string locale_default_input_method =
      ime_manager->GetInputMethodUtil()->
          GetLanguageDefaultInputMethodId(loaded_locale);
  if (!locale_default_input_method.empty()) {
    PrefService* user_prefs = self->profile->GetPrefs();
    user_prefs->SetString(prefs::kLanguagePreviousInputMethod,
                          locale_default_input_method);
    ime_manager->EnableInputMethod(locale_default_input_method);
  }

  // Finally, activate the first login input method.
  const std::vector<std::string>& login_input_methods =
      ime_manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
  ime_manager->ChangeInputMethod(login_input_methods[0]);
}

void SetGuestLocale(UserManager* const usermanager, Profile* const profile) {
  scoped_ptr<GuestLanguageSetCallbackData> data(
      new GuestLanguageSetCallbackData(profile));
  scoped_ptr<locale_util::SwitchLanguageCallback> callback(
      new locale_util::SwitchLanguageCallback(base::Bind(
          &GuestLanguageSetCallbackData::Callback, base::Passed(data.Pass()))));
  User* const user = usermanager->GetUserByProfile(profile);
  usermanager->RespectLocalePreference(profile, user, callback.Pass());
}

void ChromeBrowserMainPartsChromeos::PostProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just after CreateProfile().

  // Restarting Chrome inside existing user session. Possible cases:
  // 1. Chrome is restarted after crash.
  // 2. Chrome is started in browser_tests skipping the login flow
  // 3. Chrome is started on dev machine
  //    i.e. not on Chrome OS device w/o login flow.
  if (parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kLoginPassword)) {
    std::string login_user = parsed_command_line().GetSwitchValueASCII(
        chromeos::switches::kLoginUser);
    if (!base::SysInfo::IsRunningOnChromeOS() &&
        login_user == UserManager::kStubUser) {
      // For dev machines and stub user emulate as if sync has been initialized.
      profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                       login_user);
    }

    // This is done in LoginUtils::OnProfileCreated during normal login.
    LoginUtils::Get()->InitRlzDelayed(profile());

    // Send the PROFILE_PREPARED notification and call SessionStarted()
    // so that the Launcher and other Profile dependent classes are created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile()));
    UserManager::Get()->SessionStarted();

    // Now is the good time to retrieve other logged in users for this session.
    // First user has been already marked as logged in and active in
    // PreProfileInit(). Chrome should tread other user in a session as active
    // in the background.
    UserManager::Get()->RestoreActiveSessions();
  }

  // Initialize the network portal detector for Chrome OS. The network
  // portal detector starts to listen for notifications from
  // NetworkStateHandler and initiates captive portal detection for
  // active networks. Shoule be called before call to
  // OptionallyRunChromeOSLoginManager, because it depends on
  // NetworkPortalDetector.
  NetworkPortalDetector::Initialize();
  {
    NetworkPortalDetector* detector = NetworkPortalDetector::Get();
#if defined(GOOGLE_CHROME_BUILD)
    bool is_official_build = true;
#else
    bool is_official_build = false;
#endif
    // Enable portal detector if EULA was previously accepted or if
    // this is an unofficial build.
    if (!is_official_build || StartupUtils::IsEulaAccepted())
      detector->Enable(true);
  }

  // Tests should be able to tune login manager before showing it.
  // Thus only show login manager in normal (non-testing) mode.
  if (!parameters().ui_task ||
      parsed_command_line().HasSwitch(switches::kForceLoginManagerInTests)) {
    OptionallyRunChromeOSLoginManager(parsed_command_line(), profile());
  }

  // Guest user profile is never initialized with locale settings,
  // so we need special handling for Guest session.
  UserManager* const usermanager = UserManager::Get();
  if (usermanager->IsLoggedInAsGuest())
    SetGuestLocale(usermanager, profile());

  // These observers must be initialized after the profile because
  // they use the profile to dispatch extension events.
  extension_system_event_observer_.reset(new ExtensionSystemEventObserver());
  if (KioskModeSettings::Get()->IsKioskModeEnabled()) {
    retail_mode_power_save_blocker_ = content::PowerSaveBlocker::Create(
        content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
        "Retail mode");
  }

  peripheral_battery_observer_.reset(new PeripheralBatteryObserver());

  g_browser_process->platform_part()->InitializeAutomaticRebootManager();

  // This observer cannot be created earlier because it requires the shell to be
  // available.
  idle_action_warning_observer_.reset(new IdleActionWarningObserver());

  ChromeBrowserMainPartsLinux::PostProfileInit();
}

void ChromeBrowserMainPartsChromeos::PreBrowserStart() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before MetricsService::LogNeedForCleanShutdown().

  g_browser_process->metrics_service()->StartExternalMetrics();

  // Listen for XI_HierarchyChanged events. Note: if this is moved to
  // PreMainMessageLoopRun() then desktopui_PageCyclerTests fail for unknown
  // reasons, see http://crosbug.com/24833.
  XInputHierarchyChangedEventListener::GetInstance();

#if defined(USE_X11)
  // Start the CrOS input device UMA watcher
  DeviceUMA::GetInstance();
#endif

  event_rewriter_.reset(new EventRewriter());

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately after ChildProcess::WaitForDebugger().

  // Start the out-of-memory priority manager here so that we give the most
  // amount of time for the other services to start up before we start
  // adjusting the oom priority.
  g_browser_process->platform_part()->oom_priority_manager()->Start();

  if (ui::ShouldDefaultToNaturalScroll()) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kNaturalScrollDefault);
    system::InputDeviceSettings::Get()->SetTapToClick(true);
  }

  ChromeBrowserMainPartsLinux::PreBrowserStart();
}

void ChromeBrowserMainPartsChromeos::PostBrowserStart() {
  // These are dependent on the ash::Shell singleton already having been
  // initialized.
  power_button_observer_.reset(new PowerButtonObserver);
  data_promo_notification_.reset(new DataPromoNotification()),

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  BootTimesLoader::Get()->AddLogoutTimeMarker("UIMessageLoopEnded", true);

  g_browser_process->platform_part()->oom_priority_manager()->Stop();

  // Destroy the application name notifier for Kiosk mode.
  KioskModeIdleAppNameNotification::Shutdown();

  // Shutdown the upgrade detector for Chrome OS. The upgrade detector
  // stops monitoring changes from the update engine.
  if (UpgradeDetectorChromeos::GetInstance())
    UpgradeDetectorChromeos::GetInstance()->Shutdown();

  // Shutdown the network change notifier for Chrome OS. The network
  // change notifier stops monitoring changes from the power manager and
  // the network manager.
  if (NetworkChangeNotifierFactoryChromeos::GetInstance())
    NetworkChangeNotifierFactoryChromeos::GetInstance()->Shutdown();

  // Destroy UI related classes before destroying services that they may
  // depend on.
  data_promo_notification_.reset();

  // Tell DeviceSettingsService to stop talking to session_manager. Do not
  // shutdown DeviceSettingsService yet, it might still be accessed by
  // BrowserPolicyConnector (owned by g_browser_process).
  DeviceSettingsService::Get()->UnsetSessionManager();

  // We should remove observers attached to D-Bus clients before
  // DBusThreadManager is shut down.
  extension_system_event_observer_.reset();
  retail_mode_power_save_blocker_.reset();
  peripheral_battery_observer_.reset();
  power_prefs_.reset();
  event_rewriter_.reset();

  // Let the ScreenLocker unregister itself from SessionManagerClient before
  // DBusThreadManager is shut down.
  if (!KioskModeSettings::Get()->IsKioskModeEnabled())
    ScreenLocker::ShutDownClass();

  // The XInput2 event listener needs to be shut down earlier than when
  // Singletons are finally destroyed in AtExitManager.
  XInputHierarchyChangedEventListener::GetInstance()->Stop();

#if defined(USE_X11)
  DeviceUMA::GetInstance()->Stop();
#endif

  // SystemKeyEventListener::Shutdown() is always safe to call,
  // even if Initialize() wasn't called.
  SystemKeyEventListener::Shutdown();
  imageburner::BurnManager::Shutdown();
  CrasAudioHandler::Shutdown();

  // Detach D-Bus clients before DBusThreadManager is shut down.
  power_button_observer_.reset();
  idle_action_warning_observer_.reset();

  MagnificationManager::Shutdown();
  AccessibilityManager::Shutdown();

  media::SoundsManager::Shutdown();

  system::StatisticsProvider::GetInstance()->Shutdown();

  // Let the UserManager and WallpaperManager unregister itself as an observer
  // of the CrosSettings singleton before it is destroyed. This also ensures
  // that the UserManager has no URLRequest pending (see
  // http://crbug.com/276659).
  UserManager::Get()->Shutdown();
  WallpaperManager::Get()->Shutdown();

  // Let the AutomaticRebootManager unregister itself as an observer of several
  // subsystems.
  g_browser_process->platform_part()->ShutdownAutomaticRebootManager();

  // Clean up dependency on CrosSettings and stop pending data fetches.
  KioskAppManager::Shutdown();

  // We first call PostMainMessageLoopRun and then destroy UserManager, because
  // Ash needs to be closed before UserManager is destroyed.
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  // Stops all in-flight OAuth2 token fetchers before the IO thread stops.
  DeviceOAuth2TokenServiceFactory::Shutdown();

  // Called after
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() to be
  // executed after execution of chrome::CloseAsh(), because some
  // parts of WebUI depends on NetworkPortalDetector.
  NetworkPortalDetector::Shutdown();

  UserManager::Destroy();
}

void ChromeBrowserMainPartsChromeos::PostDestroyThreads() {
  // Destroy DBus services immediately after threads are stopped.
  dbus_services_.reset();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();

  // Destroy DeviceSettingsService after g_browser_process.
  DeviceSettingsService::Shutdown();
}

}  //  namespace chromeos
