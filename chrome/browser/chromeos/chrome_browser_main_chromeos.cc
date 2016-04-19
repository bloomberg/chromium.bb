// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/linux_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/dbus/chrome_console_service_provider_delegate.h"
#include "chrome/browser/chromeos/dbus/chrome_display_power_service_provider_delegate.h"
#include "chrome/browser/chromeos/dbus/chrome_proxy_resolver_delegate.h"
#include "chrome/browser/chromeos/dbus/kiosk_info_service_provider.h"
#include "chrome/browser/chromeos/dbus/screen_lock_service_provider.h"
#include "chrome/browser/chromeos/display/quirks_manager_delegate_impl.h"
#include "chrome/browser/chromeos/events/event_rewriter.h"
#include "chrome/browser/chromeos/events/event_rewriter_controller.h"
#include "chrome/browser/chromeos/events/keyboard_driven_event_rewriter.h"
#include "chrome/browser/chromeos/extensions/default_app_order.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "chrome/browser/chromeos/options/cert_library.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/power/freezer_cgroup_process_manager.h"
#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"
#include "chrome/browser/chromeos/power/peripheral_battery_observer.h"
#include "chrome/browser/chromeos/power/power_button_observer.h"
#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/browser/chromeos/power/renderer_freezer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/network_connect_delegate_chromeos.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/services/console_service_provider.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "chromeos/dbus/services/display_power_service_provider.h"
#include "chromeos/dbus/services/liveness_service_provider.h"
#include "chromeos/dbus/services/proxy_resolution_service_provider.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/user_names.h"
#include "chromeos/login_event_recorder.h"
#include "chromeos/network/network_change_notifier_chromeos.h"
#include "chromeos/network/network_change_notifier_factory_chromeos.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector_stub.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/device_event_log/device_event_log.h"
#include "components/metrics/metrics_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_manager_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "media/audio/sounds/sounds_manager.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/ssl_server_socket.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/touch/touch_device.h"
#include "ui/events/event_utils.h"

#if defined(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#endif

// Exclude X11 dependents for ozone
#if defined(USE_X11)
#include "chrome/browser/chromeos/device_uma.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"
#endif

#if defined(MOJO_SHELL_CLIENT)
#include "chrome/browser/chromeos/chrome_interface_factory.h"
#endif

namespace chromeos {

namespace {

void ChromeOSVersionCallback(const std::string& version) {
  base::SetLinuxDistro(std::string("CrOS ") + version);
}

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
      !command_line.HasSwitch(switches::kForceLoginManagerInTests) &&
      app_manager->IsAutoLaunchEnabled() &&
      KioskAppLaunchError::Get() == KioskAppLaunchError::NONE;
}

// Creates an instance of the NetworkPortalDetector implementation or a stub.
void InitializeNetworkPortalDetector() {
  if (network_portal_detector::SetForTesting())
    return;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorStub());
  } else {
    network_portal_detector::SetNetworkPortalDetector(
        new NetworkPortalDetectorImpl(
            g_browser_process->system_request_context(), true));
  }
}

bool IsRunningAsMusClient() {
#if defined(MOJO_SHELL_CLIENT)
  return content::MojoShellConnection::Get() &&
         content::MojoShellConnection::Get()->UsingExternalShell();
#endif
  return false;
}

}  // namespace

namespace internal {

// Wrapper class for initializing dbus related services and shutting them
// down. This gets instantiated in a scoped_ptr so that shutdown methods in the
// destructor will get called if and only if this has been instantiated.
class DBusServices {
 public:
  explicit DBusServices(const content::MainFunctionParams& parameters) {
    // Initialize DBusThreadManager for the browser. This must be done after
    // the main message loop is started, as it uses the message loop.
    DBusThreadManager::Initialize();

    bluez::BluezDBusManager::Initialize(
        DBusThreadManager::Get()->GetSystemBus(),
        chromeos::DBusThreadManager::Get()->IsUsingStub(
            chromeos::DBusClientBundle::BLUETOOTH));

    PowerPolicyController::Initialize(
        DBusThreadManager::Get()->GetPowerManagerClient());

    ScopedVector<CrosDBusService::ServiceProviderInterface> service_providers;
    service_providers.push_back(ProxyResolutionServiceProvider::Create(
        base::WrapUnique(new ChromeProxyResolverDelegate())));
    service_providers.push_back(new DisplayPowerServiceProvider(
        base::WrapUnique(new ChromeDisplayPowerServiceProviderDelegate)));
    service_providers.push_back(new LivenessServiceProvider);
    service_providers.push_back(new ScreenLockServiceProvider);
    service_providers.push_back(new ConsoleServiceProvider(
        base::WrapUnique(new ChromeConsoleServiceProviderDelegate)));
    service_providers.push_back(new KioskInfoService);
    CrosDBusService::Initialize(std::move(service_providers));

    // Initialize PowerDataCollector after DBusThreadManager is initialized.
    PowerDataCollector::Initialize();

    LoginState::Initialize();
    SystemSaltGetter::Initialize();
    TPMTokenLoader::Initialize();
    CertLoader::Initialize();

    disks::DiskMountManager::Initialize();
    cryptohome::AsyncMethodCaller::Initialize();
    cryptohome::HomedirMethods::Initialize();

    NetworkHandler::Initialize();
    CertLibrary::Initialize();

    // Initialize the network change notifier for Chrome OS. The network
    // change notifier starts to monitor changes from the power manager and
    // the network manager.
    NetworkChangeNotifierFactoryChromeos::GetInstance()->Initialize();

    // Initialize the NetworkConnect handler.
    network_connect_delegate_.reset(new NetworkConnectDelegateChromeOS);
    ui::NetworkConnect::Initialize(network_connect_delegate_.get());

    // Likewise, initialize the upgrade detector for Chrome OS. The upgrade
    // detector starts to monitor changes from the update engine.
    UpgradeDetectorChromeos::GetInstance()->Init();

    // Initialize the device settings service so that we'll take actions per
    // signals sent from the session manager. This needs to happen before
    // g_browser_process initializes BrowserPolicyConnector.
    DeviceSettingsService::Initialize();
    DeviceSettingsService::Get()->SetSessionManager(
        DBusThreadManager::Get()->GetSessionManagerClient(),
        OwnerSettingsServiceChromeOSFactory::GetInstance()->GetOwnerKeyUtil());
  }

  ~DBusServices() {
    ui::NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    CertLibrary::Shutdown();
    NetworkHandler::Shutdown();
    cryptohome::AsyncMethodCaller::Shutdown();
    disks::DiskMountManager::Shutdown();
    SystemSaltGetter::Shutdown();
    LoginState::Shutdown();
    CertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    CrosDBusService::Shutdown();
    PowerDataCollector::Shutdown();
    PowerPolicyController::Shutdown();
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();

    // NOTE: This must only be called if Initialize() was called.
    DBusThreadManager::Shutdown();
  }

 private:
  std::unique_ptr<NetworkConnectDelegateChromeOS> network_connect_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DBusServices);
};

}  //  namespace internal

// ChromeBrowserMainPartsChromeos ----------------------------------------------

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsLinux(parameters) {
}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutDone", false);
  BootTimesRecorder::Get()->WriteLogoutTimes();
}

// content::BrowserMainParts and ChromeBrowserMainExtraParts overrides ---------

void ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  base::CommandLine* singleton_command_line =
      base::CommandLine::ForCurrentProcess();

  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    singleton_command_line->AppendSwitch(::switches::kDisableSync);
    singleton_command_line->AppendSwitch(::switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }

  // If we're not running on real Chrome OS hardware (or under VM), and are not
  // showing the login manager or attempting a command line login, login with a
  // stub user.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !parsed_command_line().HasSwitch(switches::kLoginManager) &&
      !parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kGuestSession)) {
    singleton_command_line->AppendSwitchASCII(
        switches::kLoginUser,
        cryptohome::Identification(login::StubAccountId()).id());
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
    chrome::SetChannel(channel);
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
  // device_event_log must be initialized after the message loop.
  device_event_log::Initialize(0 /* default max entries */);

  base::FilePath user_data_dir;
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    // Override some paths with stub locations so that cloud policy and
    // enterprise enrollment work on desktop builds, for ease of
    // development.
    chromeos::RegisterStubPathOverrides(user_data_dir);
  }

  dbus_services_.reset(new internal::DBusServices(parameters()));

  // Enable support for SSL server sockets, which must be done while still
  // single-threaded.  This is required for remote assistance host on Chrome OS.
  net::EnableSSLServerSockets();

  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
}

// Threads are initialized between MainMessageLoopStart and MainMessageLoopRun.
// about_flags settings are applied in ChromeBrowserMainParts::PreCreateThreads.
void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
#if defined(MOJO_SHELL_CLIENT)
  if (IsRunningAsMusClient()) {
    interface_factory_.reset(new ChromeInterfaceFactory);
    content::MojoShellConnection::Get()->AddListener(interface_factory_.get());
  }
#endif

  // Set the crypto thread after the IO thread has been created/started.
  TPMTokenLoader::Get()->SetCryptoTaskRunner(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));

  CrasAudioHandler::Initialize(
      new AudioDevicesPrefHandlerImpl(g_browser_process->local_state()));

  quirks::QuirksManager::Initialize(
      std::unique_ptr<quirks::QuirksManager::Delegate>(
          new quirks::QuirksManagerDelegateImpl()),
      content::BrowserThread::GetBlockingPool(),
      g_browser_process->local_state(),
      g_browser_process->system_request_context());

  // Start loading machine statistics here. StatisticsProvider::Shutdown()
  // will ensure that loading is aborted on early exit.
  bool load_oem_statistics = !StartupUtils::IsOobeCompleted();
  system::StatisticsProvider::GetInstance()->StartLoadingMachineStatistics(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE),
      load_oem_statistics);

  base::FilePath downloads_directory;
  CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_directory));

  DeviceOAuth2TokenServiceFactory::Initialize();

  wake_on_wifi_manager_.reset(new WakeOnWifiManager());

  arc_service_launcher_.reset(new arc::ArcServiceLauncher());
  arc_service_launcher_->Initialize();

  chromeos::ResourceReporter::GetInstance()->StartMonitoring();

  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // Now that the file thread exists we can record our stats.
  BootTimesRecorder::Get()->RecordChromeMainStats();
  LoginEventRecorder::Get()->SetDelegate(BootTimesRecorder::Get());

  // Trigger prefetching of ownership status.
  DeviceSettingsService::Get()->Load();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  g_browser_process->platform_part()->InitializeChromeUserManager();

  ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // AccessibilityManager and SystemKeyEventListener use InputMethodManager.
  input_method::Initialize();

  // ProfileHelper has to be initialized after UserManager instance is created.
  ProfileHelper::Get()->Initialize();

  // TODO(abarth): Should this move to InitializeNetworkOptions()?
  // Allow access to file:// on ChromeOS for tests.
  if (parsed_command_line().HasSwitch(::switches::kAllowFileAccess))
    ChromeNetworkDelegate::AllowAccessToAllFiles();

  // If kLoginUser is passed this indicates that user has already
  // logged in and we should behave accordingly.
  bool immediate_login =
      parsed_command_line().HasSwitch(switches::kLoginUser);
  if (immediate_login) {
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

  if (!IsRunningAsMusClient()) {
    // Initialize magnification manager before ash tray is created. And this
    // must be placed after UserManager::SessionStarted();
    // TODO(sad): These components expects the ash::Shell instance to be
    // created. However, when running as a mus-client, an ash::Shell instance is
    // not created. These accessibility services should instead be exposed as
    // separate services. crbug.com/557401
    AccessibilityManager::Initialize();
    MagnificationManager::Initialize();
  }

  wallpaper::WallpaperManagerBase::SetPathIds(
      chrome::DIR_USER_DATA,
      chrome::DIR_CHROMEOS_WALLPAPERS,
      chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS);

  // Add observers for WallpaperManager. This depends on PowerManagerClient,
  // TimezoneSettings and CrosSettings.
  WallpaperManager::Initialize();
  WallpaperManager::Get()->AddObservers();

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&version_loader::GetVersion,
                 version_loader::VERSION_FULL),
      base::Bind(&ChromeOSVersionCallback));

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests and kiosk app launch by default.
  // Individual tests may enable them if they want.
  if (parsed_command_line().HasSwitch(::switches::kTestType) ||
      ShouldAutoLaunchKioskApp(parsed_command_line())) {
    WizardController::SetZeroDelays();
  }

  power_prefs_.reset(new PowerPrefs(PowerPolicyController::Get()));

  // In Aura builds this will initialize ash::Shell.
  ChromeBrowserMainPartsLinux::PreProfileInit();

  if (immediate_login) {
    const std::string cryptohome_id =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginUser);
    const AccountId account_id(
        cryptohome::Identification::FromString(cryptohome_id).GetAccountId());

    user_manager::UserManager* user_manager = user_manager::UserManager::Get();

    if (policy::IsDeviceLocalAccountUser(account_id.GetUserEmail(), nullptr) &&
        !user_manager->IsKnownUser(account_id)) {
      // When a device-local account is removed, its policy is deleted from disk
      // immediately. If a session using this account happens to be in progress,
      // the session is allowed to continue with policy served from an in-memory
      // cache. If Chrome crashes later in the session, the policy becomes
      // completely unavailable. Exit the session in that case, rather than
      // allowing it to continue without policy.
      chrome::AttemptUserExit();
      return;
    }

    // In case of multi-profiles --login-profile will contain user_id_hash.
    std::string user_id_hash =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginProfile);
    user_manager->UserLoggedIn(account_id, user_id_hash, true);
    VLOG(1) << "Relaunching browser for user: " << account_id.Serialize()
            << " with hash: " << user_id_hash;
  }
}

class GuestLanguageSetCallbackData {
 public:
  explicit GuestLanguageSetCallbackData(Profile* profile) : profile(profile) {
  }

  // Must match SwitchLanguageCallback type.
  static void Callback(
      const std::unique_ptr<GuestLanguageSetCallbackData>& self,
      const locale_util::LanguageSwitchResult& result);

  Profile* profile;
};

// static
void GuestLanguageSetCallbackData::Callback(
    const std::unique_ptr<GuestLanguageSetCallbackData>& self,
    const locale_util::LanguageSwitchResult& result) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  scoped_refptr<input_method::InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  // For guest mode, we should always use the first login input methods.
  // This is to keep consistency with UserSessionManager::SetFirstLoginPrefs().
  // See crbug.com/530808.
  std::vector<std::string> input_methods;
  manager->GetInputMethodUtil()->GetFirstLoginInputMethodIds(
      result.loaded_locale, ime_state->GetCurrentInputMethod(), &input_methods);
  ime_state->ReplaceEnabledInputMethods(input_methods);

  // Active layout must be hardware "login layout".
  // The previous one must be "locale default layout".
  // First, enable all hardware input methods.
  input_methods = manager->GetInputMethodUtil()->GetHardwareInputMethodIds();
  for (size_t i = 0; i < input_methods.size(); ++i)
    ime_state->EnableInputMethod(input_methods[i]);

  // Second, enable locale based input methods.
  const std::string locale_default_input_method =
      manager->GetInputMethodUtil()->GetLanguageDefaultInputMethodId(
          result.loaded_locale);
  if (!locale_default_input_method.empty()) {
    PrefService* user_prefs = self->profile->GetPrefs();
    user_prefs->SetString(prefs::kLanguagePreviousInputMethod,
                          locale_default_input_method);
    ime_state->EnableInputMethod(locale_default_input_method);
  }

  // Finally, activate the first login input method.
  const std::vector<std::string>& login_input_methods =
      manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
  ime_state->ChangeInputMethod(login_input_methods[0],
                               false /* show_message */);
}

void SetGuestLocale(Profile* const profile) {
  std::unique_ptr<GuestLanguageSetCallbackData> data(
      new GuestLanguageSetCallbackData(profile));
  locale_util::SwitchLanguageCallback callback(base::Bind(
      &GuestLanguageSetCallbackData::Callback, base::Passed(std::move(data))));
  const user_manager::User* const user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  UserSessionManager::GetInstance()->RespectLocalePreference(
      profile, user, callback);
}

void ChromeBrowserMainPartsChromeos::PostProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just after CreateProfile().

  if (chromeos::ProfileHelper::IsSigninProfile(profile())) {
    // Flush signin profile if it is just created (new device or after recovery)
    // to ensure it is correctly persisted.
    if (profile()->IsNewProfile())
      ProfileHelper::Get()->FlushProfile(profile());
  } else {
    // Force loading of signin profile if it was not loaded before. It is
    // possible when we are restoring session or skipping login screen for some
    // other reason.
    chromeos::ProfileHelper::GetSigninProfile();
  }

  BootTimesRecorder::Get()->OnChromeProcessStart();

  // Initialize the network portal detector for Chrome OS. The network
  // portal detector starts to listen for notifications from
  // NetworkStateHandler and initiates captive portal detection for
  // active networks. Should be called before call to CreateSessionManager,
  // because it depends on NetworkPortalDetector.
  InitializeNetworkPortalDetector();
  {
#if defined(GOOGLE_CHROME_BUILD)
    bool is_official_build = true;
#else
    bool is_official_build = false;
#endif
    // Enable portal detector if EULA was previously accepted or if
    // this is an unofficial build.
    if (!is_official_build || StartupUtils::IsEulaAccepted())
      network_portal_detector::GetInstance()->Enable(true);
  }

  // Initialize input methods.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  UserSessionManager* session_manager = UserSessionManager::GetInstance();
  DCHECK(manager);
  DCHECK(session_manager);

  manager->SetState(session_manager->GetDefaultIMEState(profile()));

  bool is_running_test = parameters().ui_task != nullptr;
  g_browser_process->platform_part()->InitializeSessionManager(
      parsed_command_line(), profile(), is_running_test);
  g_browser_process->platform_part()->SessionManager()->Start();

  // Guest user profile is never initialized with locale settings,
  // so we need special handling for Guest session.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    SetGuestLocale(profile());

  peripheral_battery_observer_.reset(new PeripheralBatteryObserver());

  renderer_freezer_.reset(
      new RendererFreezer(std::unique_ptr<RendererFreezer::Delegate>(
          new FreezerCgroupProcessManager())));

  g_browser_process->platform_part()->InitializeAutomaticRebootManager();
  g_browser_process->platform_part()->InitializeDeviceDisablingManager();

  // This observer cannot be created earlier because it requires the shell to be
  // available.
  idle_action_warning_observer_.reset(new IdleActionWarningObserver());

  ChromeBrowserMainPartsLinux::PostProfileInit();
}

void ChromeBrowserMainPartsChromeos::PreBrowserStart() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before MetricsService::LogNeedForCleanShutdown().

  // Start the external metrics service, which collects metrics from Chrome OS
  // and passes them to the browser process.
  external_metrics_ = new chromeos::ExternalMetrics;
  external_metrics_->Start();

#if defined(USE_X11)
  // Listen for system key events so that the user will be able to adjust the
  // volume on the login screen, if Chrome is running on Chrome OS
  // (i.e. not Linux desktop), and in non-test mode.
  // Note: SystemKeyEventListener depends on the DBus thread.
  if (base::SysInfo::IsRunningOnChromeOS() &&
      !parameters().ui_task) {  // ui_task is non-NULL when running tests.
    SystemKeyEventListener::Initialize();
  }

  // Listen for XI_HierarchyChanged events. Note: if this is moved to
  // PreMainMessageLoopRun() then desktopui_PageCyclerTests fail for unknown
  // reasons, see http://crosbug.com/24833.
  XInputHierarchyChangedEventListener::GetInstance();

  // Start the CrOS input device UMA watcher
  DeviceUMA::GetInstance();
#endif

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately after ChildProcess::WaitForDebugger().

  if (ui::ShouldDefaultToNaturalScroll()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kNaturalScrollDefault);
    system::InputDeviceSettings::Get()->SetTapToClick(true);
  }

  ChromeBrowserMainPartsLinux::PreBrowserStart();
}

void ChromeBrowserMainPartsChromeos::PostBrowserStart() {
  system::InputDeviceSettings::Get()->InitTouchDevicesStatusFromLocalPrefs();

  if (!IsRunningAsMusClient()) {
    // These are dependent on the ash::Shell singleton already having been
    // initialized. Consequently, these cannot be used when running as a mus
    // client.
    // TODO(oshima): Remove ash dependency in PowerButtonObserver.
    // crbug.com/408832.
    power_button_observer_.reset(new PowerButtonObserver);
    data_promo_notification_.reset(new DataPromoNotification());

    keyboard_event_rewriters_.reset(new EventRewriterController());
    keyboard_event_rewriters_->AddEventRewriter(
        std::unique_ptr<ui::EventRewriter>(new KeyboardDrivenEventRewriter()));
    keyboard_event_rewriters_->AddEventRewriter(
        std::unique_ptr<ui::EventRewriter>(new SpokenFeedbackEventRewriter()));
    keyboard_event_rewriters_->AddEventRewriter(
        std::unique_ptr<ui::EventRewriter>(new EventRewriter(
            ash::Shell::GetInstance()->sticky_keys_controller())));
    keyboard_event_rewriters_->Init();
  }

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  chromeos::ResourceReporter::GetInstance()->StopMonitoring();

  BootTimesRecorder::Get()->AddLogoutTimeMarker("UIMessageLoopEnded", true);

  arc_service_launcher_->Shutdown();

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
  peripheral_battery_observer_.reset();
  power_prefs_.reset();
  renderer_freezer_.reset();
  wake_on_wifi_manager_.reset();
  ScreenLocker::ShutDownClass();
  keyboard_event_rewriters_.reset();
#if defined(USE_X11)
  // The XInput2 event listener needs to be shut down earlier than when
  // Singletons are finally destroyed in AtExitManager.
  XInputHierarchyChangedEventListener::GetInstance()->Stop();

  DeviceUMA::GetInstance()->Stop();

  // SystemKeyEventListener::Shutdown() is always safe to call,
  // even if Initialize() wasn't called.
  SystemKeyEventListener::Shutdown();
#endif

  // Detach D-Bus clients before DBusThreadManager is shut down.
  power_button_observer_.reset();
  idle_action_warning_observer_.reset();

  if (!IsRunningAsMusClient())
    MagnificationManager::Shutdown();

  media::SoundsManager::Shutdown();

  system::StatisticsProvider::GetInstance()->Shutdown();

  // Let the UserManager and WallpaperManager unregister itself as an observer
  // of the CrosSettings singleton before it is destroyed. This also ensures
  // that the UserManager has no URLRequest pending (see
  // http://crbug.com/276659).
  g_browser_process->platform_part()->user_manager()->Shutdown();
  WallpaperManager::Shutdown();

  // Let the DeviceDisablingManager unregister itself as an observer of the
  // CrosSettings singleton before it is destroyed.
  g_browser_process->platform_part()->ShutdownDeviceDisablingManager();

  // Let the AutomaticRebootManager unregister itself as an observer of several
  // subsystems.
  g_browser_process->platform_part()->ShutdownAutomaticRebootManager();

  // Clean up dependency on CrosSettings and stop pending data fetches.
  KioskAppManager::Shutdown();

  // Make sure that there is no pending URLRequests.
  UserSessionManager::GetInstance()->Shutdown();

  // Give BrowserPolicyConnectorChromeOS a chance to unregister any observers
  // on services that are going to be deleted later but before its Shutdown()
  // is called.
  g_browser_process->platform_part()->browser_policy_connector_chromeos()->
      PreShutdown();

  // We first call PostMainMessageLoopRun and then destroy UserManager, because
  // Ash needs to be closed before UserManager is destroyed.
  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();

  if (!IsRunningAsMusClient())
    AccessibilityManager::Shutdown();

  input_method::Shutdown();

  // Stops all in-flight OAuth2 token fetchers before the IO thread stops.
  DeviceOAuth2TokenServiceFactory::Shutdown();

  // Shutdown after PostMainMessageLoopRun() which should destroy all observers.
  CrasAudioHandler::Shutdown();

  quirks::QuirksManager::Shutdown();

  // Called after
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() to be
  // executed after execution of chrome::CloseAsh(), because some
  // parts of WebUI depends on NetworkPortalDetector.
  network_portal_detector::Shutdown();

  g_browser_process->platform_part()->DestroyChromeUserManager();

  g_browser_process->platform_part()->ShutdownSessionManager();
}

void ChromeBrowserMainPartsChromeos::PostDestroyThreads() {
  // Destroy DBus services immediately after threads are stopped.
  dbus_services_.reset();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();

  // Destroy DeviceSettingsService after g_browser_process.
  DeviceSettingsService::Shutdown();
}

}  //  namespace chromeos
