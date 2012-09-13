// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"

#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/contacts/contact_manager.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/imageburner/burn_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/low_memory_observer.h"
#include "chrome/browser/chromeos/mtp/media_transfer_protocol_manager.h"
#include "chrome/browser/chromeos/net/cros_network_change_notifier_factory.h"
#include "chrome/browser/chromeos/net/network_change_notifier_chromeos.h"
#include "chrome/browser/chromeos/oom_priority_manager.h"
#include "chrome/browser/chromeos/power/brightness_observer.h"
#include "chrome/browser/chromeos/power/output_observer.h"
#include "chrome/browser/chromeos/power/power_button_controller_delegate_chromeos.h"
#include "chrome/browser/chromeos/power/power_button_observer.h"
#include "chrome/browser/chromeos/power/power_state_override.h"
#include "chrome/browser/chromeos/power/resume_observer.h"
#include "chrome/browser/chromeos/power/screen_dimming_observer.h"
#include "chrome/browser/chromeos/power/screen_lock_observer.h"
#include "chrome/browser/chromeos/power/user_activity_notifier.h"
#include "chrome/browser/chromeos/power/video_activity_notifier.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/system_monitor/media_transfer_protocol_device_observer_chromeos.h"
#include "chrome/browser/system_monitor/removable_device_notifications_chromeos.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/main_function_params.h"
#include "grit/platform_locale_settings.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

class MessageLoopObserver : public MessageLoopForUI::Observer {
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
  }
};

static base::LazyInstance<MessageLoopObserver> g_message_loop_observer =
    LAZY_INSTANCE_INITIALIZER;

// Login -----------------------------------------------------------------------

// Class is used to login using passed username and password.
// The instance will be deleted upon success or failure.
class StubLogin : public chromeos::LoginStatusConsumer,
                  public chromeos::LoginUtils::Delegate {
 public:
  StubLogin(std::string username, std::string password)
      : pending_requests_(false),
        profile_prepared_(false) {
    authenticator_ = chromeos::LoginUtils::Get()->CreateAuthenticator(this);
    authenticator_.get()->AuthenticateToLogin(
        g_browser_process->profile_manager()->GetDefaultProfile(),
        username,
        password,
        std::string(),
        std::string());
  }

  ~StubLogin() {
    chromeos::LoginUtils::Get()->DelegateDeleted(this);
  }

  void OnLoginFailure(const chromeos::LoginFailure& error) {
    LOG(ERROR) << "Login Failure: " << error.GetErrorString();
    delete this;
  }

  void OnLoginSuccess(const std::string& username,
                      const std::string& password,
                      bool pending_requests,
                      bool using_oauth) {
    pending_requests_ = pending_requests;
    if (!profile_prepared_) {
      // Will call OnProfilePrepared in the end.
      chromeos::LoginUtils::Get()->PrepareProfile(username,
                                                  std::string(),
                                                  password,
                                                  pending_requests,
                                                  using_oauth,
                                                  false,
                                                  this);
    } else if (!pending_requests) {
      delete this;
    }
  }

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) {
    profile_prepared_ = true;
    chromeos::LoginUtils::Get()->DoBrowserLaunch(profile, NULL);
    if (!pending_requests_)
      delete this;
  }

  scoped_refptr<chromeos::Authenticator> authenticator_;
  bool pending_requests_;
  bool profile_prepared_;
};

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line,
                                       Profile* profile) {
  // Login should always use dual display if there is an external display.
  chromeos::OutputConfigurator* output_configurator =
      ash::Shell::GetInstance()->output_configurator();
  if (output_configurator->connected_output_count() > 1)
    output_configurator->SetDisplayMode(chromeos::STATE_DUAL_PRIMARY_ONLY);

  if (parsed_command_line.HasSwitch(switches::kLoginManager)) {
    std::string first_screen =
        parsed_command_line.GetSwitchValueASCII(switches::kLoginScreen);
    std::string size_arg =
        parsed_command_line.GetSwitchValueASCII(
            switches::kLoginScreenSize);
    gfx::Size size(0, 0);
    // Allow the size of the login window to be set explicitly. If not set,
    // default to the entire screen. This is mostly useful for testing.
    if (size_arg.size()) {
      std::vector<std::string> dimensions;
      base::SplitString(size_arg, ',', &dimensions);
      if (dimensions.size() == 2) {
        int width, height;
        if (base::StringToInt(dimensions[0], &width) &&
            base::StringToInt(dimensions[1], &height))
          size.SetSize(width, height);
      }
    }

    chromeos::ShowLoginWizard(first_screen, size);

    if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
      chromeos::InitializeKioskModeScreensaver();
  } else if (parsed_command_line.HasSwitch(switches::kLoginUser) &&
      parsed_command_line.HasSwitch(switches::kLoginPassword)) {
    chromeos::BootTimesLoader::Get()->RecordLoginAttempted();
    new StubLogin(
        parsed_command_line.GetSwitchValueASCII(switches::kLoginUser),
        parsed_command_line.GetSwitchValueASCII(switches::kLoginPassword));
  } else {
    if (!parsed_command_line.HasSwitch(switches::kTestName)) {
      // We did not log in (we crashed or are debugging), so we need to
      // restore Sync.
      chromeos::LoginUtils::Get()->RestoreAuthenticationSession(profile);
    }
  }
}

// ChromeBrowserMainPartsChromeos ----------------------------------------------

ChromeBrowserMainPartsChromeos::ChromeBrowserMainPartsChromeos(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsLinux(parameters) {
}

ChromeBrowserMainPartsChromeos::~ChromeBrowserMainPartsChromeos() {
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    chromeos::ShutdownKioskModeScreensaver();
  cryptohome::AsyncMethodCaller::Shutdown();
  chromeos::mtp::MediaTransferProtocolManager::Shutdown();
  chromeos::disks::DiskMountManager::Shutdown();

  // CrosLibrary is shut down before DBusThreadManager even though the former
  // is initialized before the latter becuase some of its libraries depend
  // on DBus clients.
  // TODO(hashimoto): Resolve this situation by removing CrosLibrary.
  // (crosbug.com/26160)
  if (!parameters().ui_task && chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Shutdown();

  chromeos::input_method::InputMethodManager::Shutdown();

  chromeos::CrosDBusService::Shutdown();
  chromeos::DBusThreadManager::Shutdown();

  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutDone",
                                                        false);
  chromeos::BootTimesLoader::Get()->WriteLogoutTimes();
}

// content::BrowserMainParts and ChromeBrowserMainExtraParts overrides ---------

void ChromeBrowserMainPartsChromeos::PreEarlyInitialization() {
  CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();

  if (parsed_command_line().HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    singleton_command_line->AppendSwitch(switches::kDisableSync);
    singleton_command_line->AppendSwitch(switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }

  // If we're not running on real ChromeOS hardware (or under VM), and are not
  // showing the login manager or attempting a command line login, login with a
  // stub user.
  if (!base::chromeos::IsRunningOnChromeOS() &&
      !parsed_command_line().HasSwitch(switches::kLoginManager) &&
      !parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kGuestSession)) {
    singleton_command_line->AppendSwitchASCII(
        switches::kLoginUser, chromeos::UserManager::kStubUser);
    if (!parsed_command_line().HasSwitch(switches::kLoginProfile)) {
      // This must be kept in sync with TestingProfile::kTestUserProfileDir.
      singleton_command_line->AppendSwitchASCII(
          switches::kLoginProfile, "test-user");
    }
    LOG(INFO) << "Running as stub user with profile dir: "
              << singleton_command_line->GetSwitchValuePath(
                     switches::kLoginProfile).value();
  }

  ChromeBrowserMainPartsLinux::PreEarlyInitialization();
}

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopStart() {
  // Initialize CrosLibrary only for the browser, unless running tests
  // (which do their own CrosLibrary setup).
  if (!parameters().ui_task) {
    const bool use_stub = !base::chromeos::IsRunningOnChromeOS();
    chromeos::CrosLibrary::Initialize(use_stub);
  }
  // Replace the default NetworkChangeNotifierFactory with ChromeOS specific
  // implementation.
  net::NetworkChangeNotifier::SetFactory(
      new chromeos::CrosNetworkChangeNotifierFactory());

  ChromeBrowserMainPartsLinux::PreMainMessageLoopStart();
}

void ChromeBrowserMainPartsChromeos::PostMainMessageLoopStart() {
  MessageLoopForUI* message_loop = MessageLoopForUI::current();
  message_loop->AddObserver(g_message_loop_observer.Pointer());

  // Initialize DBusThreadManager for the browser. This must be done after
  // the main message loop is started, as it uses the message loop.
  chromeos::DBusThreadManager::Initialize();
  // Add observers for WallpaperManager. WallpaperManager is initialized before
  // DBusThreadManager.
  chromeos::WallpaperManager::Get()->AddObservers();

  chromeos::CrosDBusService::Initialize();

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager.
  chromeos::DeviceSettingsService::Get()->Initialize(
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
      chromeos::OwnerKeyUtil::Create());

  chromeos::disks::DiskMountManager::Initialize();
  chromeos::mtp::MediaTransferProtocolManager::Initialize();
  cryptohome::AsyncMethodCaller::Initialize();

  // Initialize the network change notifier for Chrome OS. The network
  // change notifier starts to monitor changes from the power manager and
  // the network manager.
  chromeos::CrosNetworkChangeNotifierFactory::GetInstance()->Init();

  // Likewise, initialize the upgrade detector for Chrome OS. The upgrade
  // detector starts to monitor changes from the update engine.
  UpgradeDetectorChromeos::GetInstance()->Init();

  // This function and SystemKeyEventListener use InputMethodManager.
  chromeos::input_method::InputMethodManager::Initialize();

  if (base::chromeos::IsRunningOnChromeOS()) {
    // Disable Num Lock on X start up for http://crosbug.com/29169.
    chromeos::input_method::InputMethodManager::GetInstance()->
        GetXKeyboard()->SetNumLockEnabled(false);
  }

  ChromeBrowserMainPartsLinux::PostMainMessageLoopStart();
}

// Threads are initialized MainMessageLoopStart and MainMessageLoopRun.

void ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun() {
  chromeos::AudioHandler::Initialize();
  chromeos::imageburner::BurnManager::Initialize();

  // Listen for system key events so that the user will be able to adjust the
  // volume on the login screen, if Chrome is running on Chrome OS
  // (i.e. not Linux desktop), and in non-test mode.
  // Note: SystemKeyEventListener depends on the DBus thread.
  if (base::chromeos::IsRunningOnChromeOS() &&
      !parameters().ui_task) {  // ui_task is non-NULL when running tests.
    chromeos::SystemKeyEventListener::Initialize();
  }

  ChromeBrowserMainPartsLinux::PreMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::PreProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately before Profile creation().

  // Now that the file thread exists we can record our stats.
  chromeos::BootTimesLoader::Get()->RecordChromeMainStats();

  // Trigger prefetching of ownership status.
  chromeos::DeviceSettingsService::Get()->Load();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before CreateProfile().

  // Initialize the screen locker now so that it can receive
  // LOGIN_USER_CHANGED notification from UserManager.
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    chromeos::KioskModeIdleLogout::Initialize();
  } else {
    chromeos::ScreenLocker::InitClass();
  }

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // TODO(abarth): Should this move to InitializeNetworkOptions()?
  // Allow access to file:// on ChromeOS for tests.
  if (parsed_command_line().HasSwitch(switches::kAllowFileAccess))
    ChromeNetworkDelegate::AllowAccessToAllFiles();

  if (parsed_command_line().HasSwitch(switches::kEnableContacts)) {
    contact_manager_.reset(new contacts::ContactManager());
    contact_manager_->Init();
  }

  // There are two use cases for kLoginUser:
  //   1) if passed in tandem with kLoginPassword, to drive a "StubLogin"
  //   2) if passed alone, to signal that the indicated user has already
  //      logged in and we should behave accordingly.
  // This handles case 2.
  if (parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kLoginPassword)) {
    std::string username =
        parsed_command_line().GetSwitchValueASCII(switches::kLoginUser);
    VLOG(1) << "Relaunching browser for user: " << username;
    chromeos::UserManager::Get()->UserLoggedIn(username, true);

    // Redirects Chrome logging to the user data dir.
    logging::RedirectChromeLogging(parsed_command_line());

    // Initialize user policy before creating the profile so the profile
    // initialization code sees policy settings.
    // Guest accounts are not subject to user policy.
    if (!chromeos::UserManager::Get()->IsLoggedInAsGuest()) {
      g_browser_process->browser_policy_connector()->InitializeUserPolicy(
          username, false  /* wait_for_policy_fetch */);
    }
    chromeos::UserManager::Get()->SessionStarted();
  }

  // In Aura builds this will initialize ash::Shell.
  ChromeBrowserMainPartsLinux::PreProfileInit();
}

void ChromeBrowserMainPartsChromeos::PostProfileInit() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just after CreateProfile().

  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();

  if (parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kLoginPassword)) {
    // Pass the TokenService pointer to the policy connector so user policy can
    // grab a token and register with the policy server.
    // TODO(mnissler): Remove once OAuth is the only authentication mechanism.
    connector->SetUserPolicyTokenService(
        TokenServiceFactory::GetForProfile(profile()));

    // Make sure we flip every profile to not share proxies if the user hasn't
    // specified so explicitly.
    const PrefService::Preference* use_shared_proxies_pref =
        profile()->GetPrefs()->FindPreference(prefs::kUseSharedProxies);
    if (use_shared_proxies_pref->IsDefaultValue())
      profile()->GetPrefs()->SetBoolean(prefs::kUseSharedProxies, false);
  }

  // Make sure the NetworkConfigurationUpdater is ready so that it pushes ONC
  // configuration before login.
  connector->GetNetworkConfigurationUpdater();

  // Make sure that wallpaper boot transition and other delays in OOBE
  // are disabled for tests by default.
  // Individual tests may enable them if they want.
  if (parsed_command_line().HasSwitch(switches::kTestType))
    chromeos::WizardController::SetZeroDelays();

  // Tests should be able to tune login manager before showing it.
  // Thus only show login manager in normal (non-testing) mode.
  if (!parameters().ui_task)
    OptionallyRunChromeOSLoginManager(parsed_command_line(), profile());

  // These observers must be initialized after the profile because
  // they use the profile to dispatch extension events.
  //
  // Initialize the brightness observer so that we'll display an onscreen
  // indication of brightness changes during login.
  brightness_observer_.reset(new chromeos::BrightnessObserver());
  media_transfer_protocol_device_observer_.reset(
      new chromeos::mtp::MediaTransferProtocolDeviceObserverCros());
  output_observer_.reset(new chromeos::OutputObserver());
  resume_observer_.reset(new chromeos::ResumeObserver());
  screen_lock_observer_.reset(new chromeos::ScreenLockObserver());
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    power_state_override_.reset(new chromeos::PowerStateOverride());

  removable_device_notifications_ =
      new chromeos::RemovableDeviceNotificationsCros();

  ChromeBrowserMainPartsLinux::PostProfileInit();
}

void ChromeBrowserMainPartsChromeos::PreBrowserStart() {
  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- just before MetricsService::LogNeedForCleanShutdown().

  g_browser_process->metrics_service()->StartExternalMetrics();

  // Listen for XI_HierarchyChanged events. Note: if this is moved to
  // PreMainMessageLoopRun() then desktopui_PageCyclerTests fail for unknown
  // reasons, see http://crosbug.com/24833.
  chromeos::XInputHierarchyChangedEventListener::GetInstance();

  // -- This used to be in ChromeBrowserMainParts::PreMainMessageLoopRun()
  // -- immediately after ChildProcess::WaitForDebugger().

  // Start the out-of-memory priority manager here so that we give the most
  // amount of time for the other services to start up before we start
  // adjusting the oom priority.
  g_browser_process->oom_priority_manager()->Start();

  ChromeBrowserMainPartsLinux::PreBrowserStart();
}

void ChromeBrowserMainPartsChromeos::PostBrowserStart() {
  // FILE thread is created in ChromeBrowserMainParts::PreMainMessageLoopRun().

  // Get the statistics provider instance here to start loading statistcs
  // on the background FILE thread.
  chromeos::system::StatisticsProvider::GetInstance();

  // These are dependent on the ash::Shell singleton already having been
  // initialized.
  power_button_observer_.reset(new chromeos::PowerButtonObserver);
  user_activity_notifier_.reset(new chromeos::UserActivityNotifier);
  video_activity_notifier_.reset(new chromeos::VideoActivityNotifier);
  screen_dimming_observer_.reset(new chromeos::ScreenDimmingObserver);

  ChromeBrowserMainPartsLinux::PostBrowserStart();
}

// Shut down services before the browser process, etc are destroyed.
void ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun() {
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("UIMessageLoopEnded",
                                                        true);

  g_browser_process->oom_priority_manager()->Stop();

  // Stops LoginUtils background fetchers. This is needed because IO thread is
  // going to stop soon after this function. The pending background jobs could
  // cause it to crash during shutdown.
  chromeos::LoginUtils::Get()->StopBackgroundFetchers();

  // Shutdown the upgrade detector for Chrome OS. The upgrade detector
  // stops monitoring changes from the update engine.
  if (UpgradeDetectorChromeos::GetInstance())
    UpgradeDetectorChromeos::GetInstance()->Shutdown();

  // Shutdown the network change notifier for Chrome OS. The network
  // change notifier stops monitoring changes from the power manager and
  // the network manager.
  if (chromeos::CrosNetworkChangeNotifierFactory::GetInstance())
    chromeos::CrosNetworkChangeNotifierFactory::GetInstance()->Shutdown();

  // Tell DeviceSettingsService to stop talking to session_manager.
  chromeos::DeviceSettingsService::Get()->Shutdown();

  // We should remove observers attached to D-Bus clients before
  // DBusThreadManager is shut down.
  screen_lock_observer_.reset();
  resume_observer_.reset();
  brightness_observer_.reset();
  media_transfer_protocol_device_observer_.reset();
  output_observer_.reset();

  // The XInput2 event listener needs to be shut down earlier than when
  // Singletons are finally destroyed in AtExitManager.
  chromeos::XInputHierarchyChangedEventListener::GetInstance()->Stop();

  // chromeos::SystemKeyEventListener::Shutdown() is always safe to call,
  // even if Initialize() wasn't called.
  chromeos::SystemKeyEventListener::Shutdown();
  chromeos::imageburner::BurnManager::Shutdown();
  chromeos::AudioHandler::Shutdown();

  chromeos::WebSocketProxyController::Shutdown();

  // Let classes unregister themselves as observers of the ash::Shell singleton
  // before the shell is destroyed.
  user_activity_notifier_.reset();
  video_activity_notifier_.reset();

  // Detach D-Bus clients before DBusThreadManager is shut down.
  power_button_observer_.reset();
  screen_dimming_observer_.reset();

  // Delete ContactManager while |g_browser_process| is still alive.
  contact_manager_.reset();

  ChromeBrowserMainPartsLinux::PostMainMessageLoopRun();
}

void ChromeBrowserMainPartsChromeos::SetupPlatformFieldTrials() {
  SetupLowMemoryHeadroomFieldTrial();
}

void ChromeBrowserMainPartsChromeos::SetupLowMemoryHeadroomFieldTrial() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  // Only enable this experiment on Canary and Dev, since it's possible
  // that this will make the machine unstable.
  // Note that to have this code execute in a developer build,
  // then chrome::VersionInfo::CHANNEL_UNKNOWN needs to be added here.
  if (channel == chrome::VersionInfo::CHANNEL_CANARY ||
      channel == chrome::VersionInfo::CHANNEL_DEV) {
    const base::FieldTrial::Probability kDivisor = 7;
    // 1 in 7 probability of being in each group.  If the default value for the
    // kernel matches one of the experiment groups, then they will have
    // identical results.
    const base::FieldTrial::Probability kEnableProbability = 1;
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::FactoryGetFieldTrial(
            "LowMemoryMargin", kDivisor, "default", 2012, 7, 30, NULL);
    int disable = trial->AppendGroup("off", kEnableProbability);
    int margin_0mb = trial->AppendGroup("0mb", kEnableProbability);
    int margin_25mb = trial->AppendGroup("25mb", kEnableProbability);
    int margin_50mb = trial->AppendGroup("50mb", kEnableProbability);
    int margin_100mb = trial->AppendGroup("100mb", kEnableProbability);
    int margin_200mb = trial->AppendGroup("200mb", kEnableProbability);
    if (trial->group() == disable) {
      LOG(WARNING) << "low_mem: Part of 'off' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(-1);
    } else if (trial->group() == margin_0mb) {
      LOG(WARNING) << "low_mem: Part of '0MB' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(0);
    } else if (trial->group() == margin_25mb) {
      LOG(WARNING) << "low_mem: Part of '25MB' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(25);
    } else if (trial->group() == margin_50mb) {
      LOG(WARNING) << "low_mem: Part of '50MB' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(50);
    } else if (trial->group() == margin_100mb) {
      LOG(WARNING) << "low_mem: Part of '100MB' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(100);
    } else if (trial->group() == margin_200mb) {
      LOG(WARNING) << "low_mem: Part of '200MB' experiment";
      chromeos::LowMemoryObserver::SetLowMemoryMargin(200);
    } else {
      LOG(WARNING) << "low_mem: Part of 'default' experiment";
    }
  }
}
