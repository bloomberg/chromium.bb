// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_startup.h"
#include "chrome/browser/first_run/first_run_browser_process.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/instant/instant_field_trial.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/language_usage_metrics.h"
#include "chrome/browser/metrics/field_trial_synchronizer.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/metrics/tracking_synchronizer.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/browser/web_resource/gpu_blacklist_updater.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/common/child_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/main_function_params.h"
#include "grit/app_locale_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/base/sdch_manager.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/websockets/websocket_job.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_LINUX_BREAKPAD)
#include "base/linux_util.h"
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/first_run/upgrade_util_linux.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/oom_priority_manager.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include "base/environment.h"  // For PreRead experiment.
#include "base/win/windows_version.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/browser_util_win.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/first_run/try_chrome_dialog_view.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/fragmentation_checker_win.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/ui/views/user_data_dir_dialog.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/browser/user_metrics.h"
#include "net/base/net_util.h"
#include "printing/printed_document.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include <Security/Security.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/mac/install_from_dmg.h"
#include "chrome/browser/mac/keystone_glue.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/views/focus/accelerator_handler.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/gfx/gtk_util.h"
#endif

#if defined(TOUCH_UI)
#include "ui/base/touch/touch_factory.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/chrome_browser_main_x11.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/desktop.h"
#endif

using content::BrowserThread;

namespace net {
class NetLog;
}  // namespace net

namespace {

// This function provides some ways to test crash and assertion handling
// behavior of the program.
void HandleTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kBrowserAssertTest)) {
    DCHECK(false);
  }

  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kBrowserCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }

#if defined(OS_CHROMEOS)
  // Test loading libcros and exit. We return 0 if the library could be loaded,
  // and 1 if it can't be. This is for validation that the library is installed
  // and versioned properly for Chrome to find.
  if (command_line.HasSwitch(switches::kTestLoadLibcros))
    exit(!chromeos::CrosLibrary::Get()->libcros_loaded());
#endif
}

void AddFirstRunNewTabs(BrowserInit* browser_init,
                        const std::vector<GURL>& new_tabs) {
  for (std::vector<GURL>::const_iterator it = new_tabs.begin();
       it != new_tabs.end(); ++it) {
    if (it->is_valid())
      browser_init->AddFirstRunTab(*it);
  }
}

#if defined(USE_LINUX_BREAKPAD)
class GetLinuxDistroTask : public Task {
 public:
  explicit GetLinuxDistroTask() {}

  virtual void Run() {
    base::GetLinuxDistro();  // Initialize base::linux_distro if needed.
  }

  DISALLOW_COPY_AND_ASSIGN(GetLinuxDistroTask);
};
#endif  // USE_LINUX_BREAKPAD

void InitializeNetworkOptions(const CommandLine& parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kEnableFileCookies)) {
    // Enable cookie storage for file:// URLs.  Must do this before the first
    // Profile (and therefore the first CookieMonster) is created.
    net::CookieMonster::EnableFileScheme();
  }

  if (parsed_command_line.HasSwitch(switches::kEnableMacCookies))
    net::URLRequest::EnableMacCookies();

  if (parsed_command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    net::HttpStreamFactory::set_ignore_certificate_errors(true);

  if (parsed_command_line.HasSwitch(switches::kHostRules))
    net::HttpStreamFactory::SetHostMappingRules(
        parsed_command_line.GetSwitchValueASCII(switches::kHostRules));

  if (parsed_command_line.HasSwitch(switches::kEnableIPPooling))
    net::SpdySessionPool::enable_ip_pooling(true);

  if (parsed_command_line.HasSwitch(switches::kDisableIPPooling))
    net::SpdySessionPool::enable_ip_pooling(false);

  if (parsed_command_line.HasSwitch(switches::kMaxSpdySessionsPerDomain)) {
    int value;
    base::StringToInt(parsed_command_line.GetSwitchValueASCII(
            switches::kMaxSpdySessionsPerDomain),
        &value);
    net::SpdySessionPool::set_max_sessions_per_domain(value);
  }

  SetDnsCertProvenanceCheckerFactory(CreateChromeDnsCertProvenanceChecker);

  if (parsed_command_line.HasSwitch(switches::kEnableWebSocketOverSpdy)) {
    // Enable WebSocket over SPDY.
    net::WebSocketJob::set_websocket_over_spdy_enabled(true);
  }

  if (parsed_command_line.HasSwitch(switches::kEnableHttpPipelining))
    net::HttpStreamFactory::set_http_pipelining_enabled(true);
}

void InitializeURLRequestThrottlerManager(net::NetLog* net_log) {
  net::URLRequestThrottlerManager::GetInstance()->set_enable_thread_checks(
      true);

  // TODO(joi): Passing the NetLog here is temporary; once I switch the
  // URLRequestThrottlerManager to be part of the URLRequestContext it will
  // come from there. Doing it this way for now (2011/5/12) to try to fail
  // fast in case A/B experiment gives unexpected results.
  net::URLRequestThrottlerManager::GetInstance()->set_net_log(net_log);
}

// Creates key child threads. We need to do this explicitly since
// BrowserThread::PostTask silently deletes a posted task if the target message
// loop isn't created.
void CreateChildThreads(BrowserProcessImpl* process) {
  process->db_thread();
  process->file_thread();
  process->process_launcher_thread();
  process->cache_thread();
  process->io_thread();
#if defined(OS_CHROMEOS)
  process->web_socket_proxy_thread();
#endif
  // Create watchdog thread after creating all other threads because it will
  // watch the other threads and they must be running.
  process->watchdog_thread();
}

// Returns the new local state object, guaranteed non-NULL.
PrefService* InitializeLocalState(const CommandLine& parsed_command_line,
                                  bool is_first_run) {
  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = file_util::PathExists(local_state_path);

  // Load local state.  This includes the application locale so we know which
  // locale dll to load.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // TODO(brettw,*): this comment about ResourceBundle was here since
  // initial commit.  This comment seems unrelated, bit-rotten and
  // a candidate for removal.
  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  local_state->RegisterStringPref(prefs::kApplicationLocale, std::string());
#if defined(OS_CHROMEOS)
  local_state->RegisterStringPref(prefs::kOwnerLocale, std::string());
  local_state->RegisterStringPref(prefs::kHardwareKeyboardLayout,
                                  std::string());
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_CHROMEOS)
  local_state->RegisterBooleanPref(prefs::kMetricsReportingEnabled,
      GoogleUpdateSettings::GetCollectStatsConsent());
#endif  // !defined(OS_CHROMEOS)

  if (is_first_run) {
#if defined(OS_WIN)
    // During first run we read the google_update registry key to find what
    // language the user selected when downloading the installer. This
    // becomes our default language in the prefs.
    // Other platforms obey the system locale.
    std::wstring install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang)) {
      local_state->SetString(prefs::kApplicationLocale,
                             WideToASCII(install_lang));
    }
#endif  // defined(OS_WIN)
  }

  // If the local state file for the current profile doesn't exist and the
  // parent profile command line flag is present, then we should inherit some
  // local state from the parent profile.
  // Checking that the local state file for the current profile doesn't exist
  // is the most robust way to determine whether we need to inherit or not
  // since the parent profile command line flag can be present even when the
  // current profile is not a new one, and in that case we do not want to
  // inherit and reset the user's setting.
  if (!local_state_file_exists &&
      parsed_command_line.HasSwitch(switches::kParentProfile)) {
    FilePath parent_profile =
        parsed_command_line.GetSwitchValuePath(switches::kParentProfile);
    scoped_ptr<PrefService> parent_local_state(
        PrefService::CreatePrefService(parent_profile, NULL, false));
    parent_local_state->RegisterStringPref(prefs::kApplicationLocale,
                                           std::string());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state->GetString(prefs::kApplicationLocale));
  }

#if defined(OS_CHROMEOS)
  if (parsed_command_line.HasSwitch(switches::kLoginManager)) {
    std::string owner_locale = local_state->GetString(prefs::kOwnerLocale);
    // Ensure that we start with owner's locale.
    if (!owner_locale.empty() &&
        local_state->GetString(prefs::kApplicationLocale) != owner_locale &&
        !local_state->IsManagedPreference(prefs::kApplicationLocale)) {
      local_state->SetString(prefs::kApplicationLocale, owner_locale);
      local_state->ScheduleSavePersistentPrefs();
    }
  }
#endif

  return local_state;
}

// Initializes the profile, possibly doing some user prompting to pick a
// fallback profile. Returns the newly created profile, or NULL if startup
// should not continue.
Profile* CreateProfile(const content::MainFunctionParams& parameters,
                       const FilePath& user_data_dir,
                       const CommandLine& parsed_command_line) {
  Profile* profile;
  if (ProfileManager::IsMultipleProfilesEnabled() &&
      parsed_command_line.HasSwitch(switches::kProfileDirectory)) {
    g_browser_process->local_state()->SetString(prefs::kProfileLastUsed,
        parsed_command_line.GetSwitchValueASCII(
            switches::kProfileDirectory));
  }
#if defined(OS_CHROMEOS)
  // TODO(ivankr): http://crbug.com/83792
  profile = g_browser_process->profile_manager()->GetDefaultProfile(
      user_data_dir);
#else
  profile = g_browser_process->profile_manager()->GetLastUsedProfile(
      user_data_dir);
#endif
  if (profile)
    return profile;

#if defined(OS_WIN)
#if defined(USE_AURA)
  // TODO(beng):
  NOTIMPLEMENTED();
#else
  // Ideally, we should be able to run w/o access to disk.  For now, we
  // prompt the user to pick a different user-data-dir and restart chrome
  // with the new dir.
  // http://code.google.com/p/chromium/issues/detail?id=11510
  FilePath new_user_data_dir = UserDataDirDialog::RunUserDataDirDialog(
      user_data_dir);
  if (!parameters.ui_task && browser_shutdown::delete_resources_on_shutdown) {
    // Only delete the resources if we're not running tests. If we're running
    // tests the resources need to be reused as many places in the UI cache
    // SkBitmaps from the ResourceBundle.
    ResourceBundle::CleanupSharedInstance();
  }

  if (!new_user_data_dir.empty()) {
    // Because of the way CommandLine parses, it's sufficient to append a new
    // --user-data-dir switch.  The last flag of the same name wins.
    // TODO(tc): It would be nice to remove the flag we don't want, but that
    // sounds risky if we parse differently than CommandLineToArgvW.
    CommandLine new_command_line = parameters.command_line;
    new_command_line.AppendSwitchPath(switches::kUserDataDir,
                                      new_user_data_dir);
    base::LaunchProcess(new_command_line, base::LaunchOptions(), NULL);
  }
#endif
#else
  // TODO(port): fix this.  See comments near the definition of
  // user_data_dir.  It is better to CHECK-fail here than it is to
  // silently exit because of missing code in the above test.
  CHECK(profile) << "Cannot get default profile.";
#endif

  return NULL;
}

#if defined(OS_CHROMEOS)

// Class is used to login using passed username and password.
// The instance will be deleted upon success or failure.
class StubLogin : public chromeos::LoginStatusConsumer,
                  public chromeos::LoginUtils::Delegate {
 public:
  StubLogin(std::string username, std::string password) {
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
                      const GaiaAuthConsumer::ClientLoginResult& credentials,
                      bool pending_requests,
                      bool using_oauth) {
    // Will call OnProfilePrepared in the end.
    chromeos::LoginUtils::Get()->PrepareProfile(username,
                                                password,
                                                credentials,
                                                pending_requests,
                                                using_oauth,
                                                false,
                                                this);
  }

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) {
    chromeos::LoginUtils::DoBrowserLaunch(profile, NULL);
    delete this;
  }

  scoped_refptr<chromeos::Authenticator> authenticator_;
};

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line,
                                       Profile* profile) {
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
    browser::ShowLoginWizard(first_screen, size);
  } else if (parsed_command_line.HasSwitch(switches::kLoginUser) &&
      parsed_command_line.HasSwitch(switches::kLoginPassword)) {
    chromeos::BootTimesLoader::Get()->RecordLoginAttempted();
    new StubLogin(
        parsed_command_line.GetSwitchValueASCII(switches::kLoginUser),
        parsed_command_line.GetSwitchValueASCII(switches::kLoginPassword));
  } else {
    if (!parsed_command_line.HasSwitch(switches::kTestName)) {
      // We did not log in (we crashed or are debugging), so we need to
      // set the user name for sync.
      chromeos::LoginUtils::Get()->RestoreAuthenticationSession(
          chromeos::UserManager::Get()->logged_in_user().email(), profile);
    }
  }
}

#else

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line,
                                       Profile* profile) {
  // Dummy empty function for non-ChromeOS builds to avoid extra ifdefs below.
}

#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                          SecKeychainCallbackInfo *info, void *context) {
  return noErr;
}
#endif

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
void RegisterTranslateableItems(void) {
  struct {
    const char* stock_id;
    int resource_id;
  } translations[] = {
    { GTK_STOCK_COPY, IDS_COPY },
    { GTK_STOCK_CUT, IDS_CUT },
    { GTK_STOCK_PASTE, IDS_PASTE },
    { GTK_STOCK_DELETE, IDS_DELETE },
    { GTK_STOCK_SELECT_ALL, IDS_SELECT_ALL },
    { NULL, -1 }
  }, *trans;

  for (trans = translations; trans->stock_id; trans++) {
    GtkStockItem stock_item;
    if (gtk_stock_lookup(trans->stock_id, &stock_item)) {
      std::string trans_label = gfx::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(trans->resource_id));
      stock_item.label = g_strdup(trans_label.c_str());
      gtk_stock_add(&stock_item, 1);
      g_free(stock_item.label);
    }
  }
}
#endif  // defined(OS_CHROMEOS)

void SetSocketReusePolicy(int warmest_socket_trial_group,
                          const int socket_policy[],
                          int num_groups) {
  const int* result = std::find(socket_policy, socket_policy + num_groups,
                                warmest_socket_trial_group);
  DCHECK_NE(result, socket_policy + num_groups)
      << "Not a valid socket reuse policy group";
  net::SetSocketReusePolicy(result - socket_policy);
}

#if defined(USE_LINUX_BREAKPAD)
bool IsCrashReportingEnabled(const PrefService* local_state) {
  // Check whether we should initialize the crash reporter. It may be disabled
  // through configuration policy or user preference. It must be disabled for
  // Guest mode on Chrome OS in Stable channel.
  // The kHeadless environment variable overrides the decision, but only if the
  // crash service is under control of the user. It is used by QA testing
  // infrastructure to switch on generation of crash reports.
#if defined(OS_CHROMEOS)
  bool is_guest_session =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession);
  bool is_stable_channel =
      chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_STABLE;
  bool breakpad_enabled =
      !(is_guest_session && is_stable_channel) &&
      chromeos::UserCrosSettingsProvider::cached_reporting_enabled();
  if (!breakpad_enabled)
    breakpad_enabled = getenv(env_vars::kHeadless) != NULL;
#else
  const PrefService::Preference* metrics_reporting_enabled =
      local_state->FindPreference(prefs::kMetricsReportingEnabled);
  CHECK(metrics_reporting_enabled);
  bool breakpad_enabled =
      local_state->GetBoolean(prefs::kMetricsReportingEnabled);
  if (!breakpad_enabled && metrics_reporting_enabled->IsUserModifiable())
    breakpad_enabled = getenv(env_vars::kHeadless) != NULL;
#endif  // #if defined(OS_CHROMEOS)
  return breakpad_enabled;
}
#endif  // #if defined(USE_LINUX_BREAKPAD)

// This code is specific to the Windows-only PreReadExperiment field-trial.
void AddPreReadHistogramTime(const char* name, base::TimeDelta time) {
  const base::TimeDelta kMin(base::TimeDelta::FromMilliseconds(1));
  const base::TimeDelta kMax(base::TimeDelta::FromHours(1));
  static const size_t kBuckets(100);

  // FactoryTimeGet will always return a pointer to the same histogram object,
  // keyed on its name. There's no need for us to store it explicitly anywhere.
  base::Histogram* counter = base::Histogram::FactoryTimeGet(
      name, kMin, kMax, kBuckets, base::Histogram::kUmaTargetedHistogramFlag);

  counter->AddTime(time);
}

}  // namespace

namespace chrome_browser {
// This error message is not localized because we failed to load the
// localization data files.
const char kMissingLocaleDataTitle[] = "Missing File Error";
const char kMissingLocaleDataMessage[] =
    "Unable to find locale data files. Please reinstall.";
}  // namespace chrome_browser

// BrowserMainParts ------------------------------------------------------------

ChromeBrowserMainParts::ChromeBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(content::RESULT_CODE_NORMAL_EXIT),
      shutdown_watcher_(new ShutdownWatcherHelper()),
      record_search_engine_(false),
      translate_manager_(NULL),
      profile_(NULL),
      run_message_loop_(true),
      notify_result_(ProcessSingleton::PROCESS_NONE) {
  // If we're running tests (ui_task is non-null).
  if (parameters.ui_task)
    browser_defaults::enable_help_app = false;
}

ChromeBrowserMainParts::~ChromeBrowserMainParts() {
}

// This will be called after the command-line has been mutated by about:flags
MetricsService* ChromeBrowserMainParts::SetupMetricsAndFieldTrials(
    PrefService* local_state) {
  // Must initialize metrics after labs have been converted into switches,
  // but before field trials are set up (so that client ID is available for
  // one-time randomized field trials).
  MetricsService* metrics = InitializeMetrics(
      parsed_command_line_, local_state);

  // Initialize FieldTrialList to support FieldTrials that use one-time
  // randomization. The client ID will be empty if the user has not opted
  // to send metrics.
  field_trial_list_.reset(new base::FieldTrialList(metrics->GetClientId()));

  SetupFieldTrials(metrics->recording_active(),
                   local_state->IsManagedPreference(
                       prefs::kMaxConnectionsPerProxy));

  // Initialize FieldTrialSynchronizer system. This is a singleton and is used
  // for posting tasks via NewRunnableMethod. Its deleted when it goes out of
  // scope. Even though NewRunnableMethod does AddRef and Release, the object
  // will not be deleted after the Task is executed.
  field_trial_synchronizer_ = new FieldTrialSynchronizer();

  return metrics;
}

// This is an A/B test for the maximum number of persistent connections per
// host. Currently Chrome, Firefox, and IE8 have this value set at 6. Safari
// uses 4, and Fasterfox (a plugin for Firefox that supposedly configures it to
// run faster) uses 8. We would like to see how much of an effect this value has
// on browsing. Too large a value might cause us to run into SYN flood detection
// mechanisms.
void ChromeBrowserMainParts::ConnectionFieldTrial() {
  const base::FieldTrial::Probability kConnectDivisor = 100;
  const base::FieldTrial::Probability kConnectProbability = 1;  // 1% prob.

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> connect_trial(
      new base::FieldTrial(
          "ConnCountImpact", kConnectDivisor, "conn_count_6", 2011, 6, 30));

  // This (6) is the current default value. Having this group declared here
  // makes it straightforward to modify |kConnectProbability| such that the same
  // probability value will be assigned to all the other groups, while
  // preserving the remainder of the of probability space to the default value.
  const int connect_6 = connect_trial->kDefaultGroupNumber;

  const int connect_5 = connect_trial->AppendGroup("conn_count_5",
                                                   kConnectProbability);
  const int connect_7 = connect_trial->AppendGroup("conn_count_7",
                                                   kConnectProbability);
  const int connect_8 = connect_trial->AppendGroup("conn_count_8",
                                                   kConnectProbability);
  const int connect_9 = connect_trial->AppendGroup("conn_count_9",
                                                   kConnectProbability);

  const int connect_trial_group = connect_trial->group();

  if (connect_trial_group == connect_5) {
    net::ClientSocketPoolManager::set_max_sockets_per_group(5);
  } else if (connect_trial_group == connect_6) {
    net::ClientSocketPoolManager::set_max_sockets_per_group(6);
  } else if (connect_trial_group == connect_7) {
    net::ClientSocketPoolManager::set_max_sockets_per_group(7);
  } else if (connect_trial_group == connect_8) {
    net::ClientSocketPoolManager::set_max_sockets_per_group(8);
  } else if (connect_trial_group == connect_9) {
    net::ClientSocketPoolManager::set_max_sockets_per_group(9);
  } else {
    NOTREACHED();
  }
}

// A/B test for determining a value for unused socket timeout. Currently the
// timeout defaults to 10 seconds. Having this value set too low won't allow us
// to take advantage of idle sockets. Setting it to too high could possibly
// result in more ERR_CONNECTION_RESETs, since some servers will kill a socket
// before we time it out. Since these are "unused" sockets, we won't retry the
// connection and instead show an error to the user. So we need to be
// conservative here. We've seen that some servers will close the socket after
// as short as 10 seconds. See http://crbug.com/84313 for more details.
void ChromeBrowserMainParts::SocketTimeoutFieldTrial() {
  const base::FieldTrial::Probability kIdleSocketTimeoutDivisor = 100;
  // 1% probability for all experimental settings.
  const base::FieldTrial::Probability kSocketTimeoutProbability = 1;

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> socket_timeout_trial(
      new base::FieldTrial("IdleSktToImpact", kIdleSocketTimeoutDivisor,
          "idle_timeout_10", 2011, 6, 30));
  const int socket_timeout_10 = socket_timeout_trial->kDefaultGroupNumber;

  const int socket_timeout_5 =
      socket_timeout_trial->AppendGroup("idle_timeout_5",
                                        kSocketTimeoutProbability);
  const int socket_timeout_20 =
      socket_timeout_trial->AppendGroup("idle_timeout_20",
                                        kSocketTimeoutProbability);

  const int idle_to_trial_group = socket_timeout_trial->group();

  if (idle_to_trial_group == socket_timeout_5) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(5));
  } else if (idle_to_trial_group == socket_timeout_10) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(10));
  } else if (idle_to_trial_group == socket_timeout_20) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(20));
  } else {
    NOTREACHED();
  }
}

void ChromeBrowserMainParts::ProxyConnectionsFieldTrial() {
  const base::FieldTrial::Probability kProxyConnectionsDivisor = 100;
  // 25% probability
  const base::FieldTrial::Probability kProxyConnectionProbability = 1;

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> proxy_connection_trial(
      new base::FieldTrial("ProxyConnectionImpact", kProxyConnectionsDivisor,
          "proxy_connections_32", 2011, 6, 30));

  // This (32 connections per proxy server) is the current default value.
  // Declaring it here allows us to easily re-assign the probability space while
  // maintaining that the default group always has the remainder of the "share",
  // which allows for cleaner and quicker changes down the line if needed.
  const int proxy_connections_32 = proxy_connection_trial->kDefaultGroupNumber;

  // The number of max sockets per group cannot be greater than the max number
  // of sockets per proxy server.  We tried using 8, and it can easily
  // lead to total browser stalls.
  const int proxy_connections_16 =
      proxy_connection_trial->AppendGroup("proxy_connections_16",
                                          kProxyConnectionProbability);
  const int proxy_connections_64 =
      proxy_connection_trial->AppendGroup("proxy_connections_64",
                                          kProxyConnectionProbability);

  const int proxy_connections_trial_group = proxy_connection_trial->group();

  if (proxy_connections_trial_group == proxy_connections_16) {
    net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(16);
  } else if (proxy_connections_trial_group == proxy_connections_32) {
    net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(32);
  } else if (proxy_connections_trial_group == proxy_connections_64) {
    net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(64);
  } else {
    NOTREACHED();
  }
}

// When --use-spdy not set, users will be in A/B test for spdy.
// group A (npn_with_spdy): this means npn and spdy are enabled. In case server
//                          supports spdy, browser will use spdy.
// group B (npn_with_http): this means npn is enabled but spdy won't be used.
//                          Http is still used for all requests.
//           default group: no npn or spdy is involved. The "old" non-spdy
//                          chrome behavior.
void ChromeBrowserMainParts::SpdyFieldTrial() {
  if (parsed_command_line().HasSwitch(switches::kUseSpdy)) {
    std::string spdy_mode =
        parsed_command_line().GetSwitchValueASCII(switches::kUseSpdy);
    net::HttpNetworkLayer::EnableSpdy(spdy_mode);
  } else {
#if !defined(OS_CHROMEOS)
    bool is_spdy_trial = false;
    const base::FieldTrial::Probability kSpdyDivisor = 100;
    base::FieldTrial::Probability npnhttp_probability = 5;

    // After June 30, 2013 builds, it will always be in default group.
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial(
            "SpdyImpact", kSpdyDivisor, "npn_with_spdy", 2013, 6, 30));

    // npn with spdy support is the default.
    int npn_spdy_grp = trial->kDefaultGroupNumber;

    // npn with only http support, no spdy.
    int npn_http_grp = trial->AppendGroup("npn_with_http", npnhttp_probability);

    int trial_grp = trial->group();
    if (trial_grp == npn_http_grp) {
      is_spdy_trial = true;
      net::HttpNetworkLayer::EnableSpdy("npn-http");
    } else if (trial_grp == npn_spdy_grp) {
      is_spdy_trial = true;
      net::HttpNetworkLayer::EnableSpdy("npn");
    } else {
      CHECK(!is_spdy_trial);
    }
#else
    // Always enable SPDY on Chrome OS
    net::HttpNetworkLayer::EnableSpdy("npn");
#endif  // !defined(OS_CHROMEOS)
  }

  // Setup SPDY CWND Field trial.
  const base::FieldTrial::Probability kSpdyCwndDivisor = 100;
  const base::FieldTrial::Probability kSpdyCwnd16 = 20;     // fixed at 16
  const base::FieldTrial::Probability kSpdyCwnd10 = 20;     // fixed at 10
  const base::FieldTrial::Probability kSpdyCwndMin16 = 20;  // no less than 16
  const base::FieldTrial::Probability kSpdyCwndMin10 = 20;  // no less than 10

  // After June 30, 2013 builds, it will always be in default group
  // (cwndDynamic).
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial(
          "SpdyCwnd", kSpdyCwndDivisor, "cwndDynamic", 2013, 6, 30));

  trial->AppendGroup("cwnd10", kSpdyCwnd10);
  trial->AppendGroup("cwnd16", kSpdyCwnd16);
  trial->AppendGroup("cwndMin16", kSpdyCwndMin16);
  trial->AppendGroup("cwndMin10", kSpdyCwndMin10);

  if (parsed_command_line().HasSwitch(switches::kMaxSpdyConcurrentStreams)) {
    int value = 0;
    base::StringToInt(parsed_command_line().GetSwitchValueASCII(
            switches::kMaxSpdyConcurrentStreams),
        &value);
    if (value > 0)
      net::SpdySession::set_max_concurrent_streams(value);
  }
}

// If --socket-reuse-policy is not specified, run an A/B test for choosing the
// warmest socket.
void ChromeBrowserMainParts::WarmConnectionFieldTrial() {
  const CommandLine& command_line = parsed_command_line();
  if (command_line.HasSwitch(switches::kSocketReusePolicy)) {
    std::string socket_reuse_policy_str = command_line.GetSwitchValueASCII(
        switches::kSocketReusePolicy);
    int policy = -1;
    base::StringToInt(socket_reuse_policy_str, &policy);

    const int policy_list[] = { 0, 1, 2 };
    VLOG(1) << "Setting socket_reuse_policy = " << policy;
    SetSocketReusePolicy(policy, policy_list, arraysize(policy_list));
    return;
  }

  const base::FieldTrial::Probability kWarmSocketDivisor = 100;
  const base::FieldTrial::Probability kWarmSocketProbability = 33;

  // After January 30, 2013 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> warmest_socket_trial(
      new base::FieldTrial(
          "WarmSocketImpact", kWarmSocketDivisor, "last_accessed_socket",
          2013, 1, 30));

  // Default value is USE_LAST_ACCESSED_SOCKET.
  const int last_accessed_socket = warmest_socket_trial->kDefaultGroupNumber;
  const int warmest_socket = warmest_socket_trial->AppendGroup(
      "warmest_socket", kWarmSocketProbability);
  const int warm_socket = warmest_socket_trial->AppendGroup(
      "warm_socket", kWarmSocketProbability);

  const int warmest_socket_trial_group = warmest_socket_trial->group();

  const int policy_list[] = { warmest_socket, warm_socket,
                              last_accessed_socket };
  SetSocketReusePolicy(warmest_socket_trial_group, policy_list,
                       arraysize(policy_list));
}

// If neither --enable-connect-backup-jobs or --disable-connect-backup-jobs is
// specified, run an A/B test for automatically establishing backup TCP
// connections when a certain timeout value is exceeded.
void ChromeBrowserMainParts::ConnectBackupJobsFieldTrial() {
  if (parsed_command_line().HasSwitch(switches::kEnableConnectBackupJobs)) {
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        true);
  } else if (parsed_command_line().HasSwitch(
        switches::kDisableConnectBackupJobs)) {
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        false);
  } else {
    const base::FieldTrial::Probability kConnectBackupJobsDivisor = 100;
    // 1% probability.
    const base::FieldTrial::Probability kConnectBackupJobsProbability = 1;
    // After June 30, 2011 builds, it will always be in default group.
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial("ConnnectBackupJobs",
            kConnectBackupJobsDivisor, "ConnectBackupJobsEnabled", 2011, 6,
                30));
    const int connect_backup_jobs_enabled = trial->kDefaultGroupNumber;
    trial->AppendGroup("ConnectBackupJobsDisabled",
                       kConnectBackupJobsProbability);
    const int trial_group = trial->group();
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        trial_group == connect_backup_jobs_enabled);
  }
}

void ChromeBrowserMainParts::PredictorFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 1000;
  // For each option (i.e., non-default), we have a fixed probability.
  // 0.1% probability.
  const base::FieldTrial::Probability kProbabilityPerGroup = 1;

  // After June 30, 2011 builds, it will always be in default group
  // (default_enabled_prefetch).
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial("DnsImpact", kDivisor,
                           "default_enabled_prefetch", 2011, 10, 30));

  // First option is to disable prefetching completely.
  int disabled_prefetch = trial->AppendGroup("disabled_prefetch",
                                              kProbabilityPerGroup);

  // We're running two experiments at the same time.  The first set of trials
  // modulates the delay-time until we declare a congestion event (and purge
  // our queue).  The second modulates the number of concurrent resolutions
  // we do at any time.  Users are in exactly one trial (or the default) during
  // any one run, and hence only one experiment at a time.
  // Experiment 1:
  // Set congestion detection at 250, 500, or 750ms, rather than the 1 second
  // default.
  int max_250ms_prefetch = trial->AppendGroup("max_250ms_queue_prefetch",
                                              kProbabilityPerGroup);
  int max_500ms_prefetch = trial->AppendGroup("max_500ms_queue_prefetch",
                                              kProbabilityPerGroup);
  int max_750ms_prefetch = trial->AppendGroup("max_750ms_queue_prefetch",
                                              kProbabilityPerGroup);
  // Set congestion detection at 2 seconds instead of the 1 second default.
  int max_2s_prefetch = trial->AppendGroup("max_2s_queue_prefetch",
                                           kProbabilityPerGroup);
  // Experiment 2:
  // Set max simultaneous resoultions to 2, 4, or 6, and scale the congestion
  // limit proportionally (so we don't impact average probability of asserting
  // congesion very much).
  int max_2_concurrent_prefetch = trial->AppendGroup(
      "max_2 concurrent_prefetch", kProbabilityPerGroup);
  int max_4_concurrent_prefetch = trial->AppendGroup(
      "max_4 concurrent_prefetch", kProbabilityPerGroup);
  int max_6_concurrent_prefetch = trial->AppendGroup(
      "max_6 concurrent_prefetch", kProbabilityPerGroup);

  if (trial->group() != disabled_prefetch) {
    // Initialize the DNS prefetch system.
    size_t max_parallel_resolves =
        chrome_browser_net::Predictor::kMaxSpeculativeParallelResolves;
    int max_queueing_delay_ms =
        chrome_browser_net::Predictor::kMaxSpeculativeResolveQueueDelayMs;

    if (trial->group() == max_2_concurrent_prefetch)
      max_parallel_resolves = 2;
    else if (trial->group() == max_4_concurrent_prefetch)
      max_parallel_resolves = 4;
    else if (trial->group() == max_6_concurrent_prefetch)
      max_parallel_resolves = 6;
    chrome_browser_net::Predictor::set_max_parallel_resolves(
        max_parallel_resolves);

    if (trial->group() == max_250ms_prefetch) {
      max_queueing_delay_ms =
         (250 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
         max_parallel_resolves;
    } else if (trial->group() == max_500ms_prefetch) {
      max_queueing_delay_ms =
          (500 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    } else if (trial->group() == max_750ms_prefetch) {
      max_queueing_delay_ms =
          (750 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    } else if (trial->group() == max_2s_prefetch) {
      max_queueing_delay_ms =
          (2000 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    }
    chrome_browser_net::Predictor::set_max_queueing_delay(
        max_queueing_delay_ms);
  }
}

void ChromeBrowserMainParts::DefaultAppsFieldTrial() {
  std::string brand;
  google_util::GetBrand(&brand);

  // Create a 100% field trial based on the brand code.
  if (LowerCaseEqualsASCII(brand, "ecdb")) {
    base::FieldTrialList::CreateFieldTrial(kDefaultAppsTrial_Name,
                                           kDefaultAppsTrial_NoAppsGroup);
  } else if (LowerCaseEqualsASCII(brand, "ecda")) {
    base::FieldTrialList::CreateFieldTrial(kDefaultAppsTrial_Name,
                                           kDefaultAppsTrial_WithAppsGroup);
  }
}

// ChromeBrowserMainParts: |SetupMetricsAndFieldTrials()| related --------------

// Initializes the metrics service with the configuration for this process,
// returning the created service (guaranteed non-NULL).
MetricsService* ChromeBrowserMainParts::InitializeMetrics(
    const CommandLine& parsed_command_line,
    const PrefService* local_state) {
#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kChromeFrame))
    MetricsLog::set_version_extension("-F");
#elif defined(ARCH_CPU_64_BITS)
  MetricsLog::set_version_extension("-64");
#endif  // defined(OS_WIN)

  MetricsService* metrics = g_browser_process->metrics_service();

  if (parsed_command_line.HasSwitch(switches::kMetricsRecordingOnly) ||
      parsed_command_line.HasSwitch(switches::kEnableBenchmarking)) {
    // If we're testing then we don't care what the user preference is, we turn
    // on recording, but not reporting, otherwise tests fail.
    metrics->StartRecordingOnly();
    return metrics;
  }

  // If the user permits metrics reporting with the checkbox in the
  // prefs, we turn on recording.  We disable metrics completely for
  // non-official builds.
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  bool enabled = chromeos::UserCrosSettingsProvider::cached_reporting_enabled();
#else
  bool enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
  if (enabled) {
    metrics->Start();
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  return metrics;
}

void ChromeBrowserMainParts::SetupFieldTrials(bool metrics_recording_enabled,
                                              bool proxy_policy_is_set) {
  // Note: make sure to call ConnectionFieldTrial() before
  // ProxyConnectionsFieldTrial().
  ConnectionFieldTrial();
  SocketTimeoutFieldTrial();
  // If a policy is defining the number of active connections this field test
  // shoud not be performed.
  if (!proxy_policy_is_set)
    ProxyConnectionsFieldTrial();
  prerender::ConfigurePrefetchAndPrerender(parsed_command_line());
  InstantFieldTrial::Activate();
  SpdyFieldTrial();
  ConnectBackupJobsFieldTrial();
  WarmConnectionFieldTrial();
  PredictorFieldTrial();
  DefaultAppsFieldTrial();
}

// -----------------------------------------------------------------------------
// TODO(viettrungluu): move more/rest of BrowserMain() into BrowserMainParts.

#if defined(OS_CHROMEOS)
// Allows authenticator to be invoked without adding refcounting. The instances
// will delete themselves upon completion.
DISABLE_RUNNABLE_METHOD_REFCOUNT(StubLogin);
#endif

#if defined(OS_WIN)
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}

DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  // Need an instance of AtExitManager to handle singleton creations and
  // deletions.  We need this new instance because, the old instance created
  // in ChromeMain() got destructed when the function returned.
  base::AtExitManager exit_manager;
  upgrade_util::RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}
#endif

void ChromeBrowserMainParts::PreEarlyInitialization() {
}

void ChromeBrowserMainParts::PostEarlyInitialization() {
}

void ChromeBrowserMainParts::ToolkitInitialized() {
}

void ChromeBrowserMainParts::PreMainMessageLoopStart() {
}

void ChromeBrowserMainParts::PostMainMessageLoopStart() {
}

void ChromeBrowserMainParts::PreMainMessageLoopRun() {
  result_code_ = PreMainMessageLoopRunImpl();
}

int ChromeBrowserMainParts::PreMainMessageLoopRunImpl() {
  run_message_loop_ = false;
  FilePath user_data_dir;
#if defined(OS_WIN)
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
#else
  // Getting the user data dir can fail if the directory isn't
  // creatable, for example; on Windows in code below we bring up a
  // dialog prompting the user to pick a different directory.
  // However, ProcessSingleton needs a real user_data_dir on Mac/Linux,
  // so it's better to fail here than fail mysteriously elsewhere.
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      << "Must be able to get user data directory!";
#endif

  process_singleton_.reset(new ProcessSingleton(user_data_dir));

  bool is_first_run = FirstRun::IsChromeFirstRun() ||
      parsed_command_line().HasSwitch(switches::kFirstRun);

  if (parsed_command_line().HasSwitch(switches::kImport) ||
      parsed_command_line().HasSwitch(switches::kImportFromFile)) {
    // We use different BrowserProcess when importing so no GoogleURLTracker is
    // instantiated (as it makes a net::URLRequest and we don't have an IO
    // thread, see bug #1292702).
    browser_process_.reset(new FirstRunBrowserProcess(parsed_command_line()));
    is_first_run = false;
  } else {
    browser_process_.reset(new BrowserProcessImpl(parsed_command_line()));
  }

  if (parsed_command_line().HasSwitch(switches::kEnableProfiling)) {
    // User wants to override default tracking status.
    std::string flag =
      parsed_command_line().GetSwitchValueASCII(switches::kEnableProfiling);
    bool enabled = flag.compare("0") != 0;
    tracked_objects::ThreadData::InitializeAndSetTrackingStatus(enabled);
  }

  // This forces the TabCloseableStateWatcher to be created and, on chromeos,
  // register for the notifications it needs to track the closeable state of
  // tabs.
  g_browser_process->tab_closeable_state_watcher();

  PrefService* local_state = InitializeLocalState(parsed_command_line(),
                                                  is_first_run);

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new GetLinuxDistroTask());

  if (IsCrashReportingEnabled(local_state))
    InitCrashReporter();
#endif

  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (parameters().ui_task) {
    g_browser_process->SetApplicationLocale("en-US");
  } else {
    // Mac starts it earlier in |PreMainMessageLoopStart()| (because it is
    // needed when loading the MainMenu.nib and the language doesn't depend on
    // anything since it comes from Cocoa.
#if defined(OS_MACOSX)
    g_browser_process->SetApplicationLocale(l10n_util::GetLocaleOverride());
#else
    const std::string locale =
        local_state->GetString(prefs::kApplicationLocale);
    // On a POSIX OS other than ChromeOS, the parameter that is passed to the
    // method InitSharedInstance is ignored.
    const std::string loaded_locale =
        ResourceBundle::InitSharedInstance(locale);
    if (loaded_locale.empty() &&
        !parsed_command_line().HasSwitch(switches::kNoErrorDialogs)) {
      ShowMissingLocaleMessageBox();
      return chrome::RESULT_CODE_MISSING_DATA;
    }
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;
    g_browser_process->SetApplicationLocale(loaded_locale);

    FilePath resources_pack_path;
    PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
    ResourceBundle::AddDataPackToSharedInstance(resources_pack_path);
#endif  // defined(OS_MACOSX)
  }

#if defined(TOOLKIT_GTK)
  g_set_application_name(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
#endif

  std::string try_chrome =
      parsed_command_line().GetSwitchValueASCII(switches::kTryChromeAgain);
  if (!try_chrome.empty()) {
#if defined(OS_WIN) && !defined(USE_AURA)
    // Setup.exe has determined that we need to run a retention experiment
    // and has lauched chrome to show the experiment UI.
    if (process_singleton_->FoundOtherProcessWindow()) {
      // It seems that we don't need to run the experiment since chrome
      // in the same profile is already running.
      VLOG(1) << "Retention experiment not required";
      return TryChromeDialogView::NOT_NOW;
    }
    int try_chrome_int;
    base::StringToInt(try_chrome, &try_chrome_int);
    TryChromeDialogView::Result answer =
        TryChromeDialogView::Show(try_chrome_int, process_singleton_.get());
    if (answer == TryChromeDialogView::NOT_NOW)
      return chrome::RESULT_CODE_NORMAL_EXIT_CANCEL;
    if (answer == TryChromeDialogView::UNINSTALL_CHROME)
      return chrome::RESULT_CODE_NORMAL_EXIT_EXP2;
#else
    // We don't support retention experiments on Mac or Linux.
    return content::RESULT_CODE_NORMAL_EXIT;
#endif  // defined(OS_WIN)
  }

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  // This needs to be called after the locale has been set.
  RegisterTranslateableItems();
#endif

  browser_init_.reset(new BrowserInit);

  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  master_prefs_.reset(new FirstRun::MasterPrefs);
  bool first_run_ui_bypass = false;  // True to skip first run UI.
  if (is_first_run) {
    first_run_ui_bypass =
        !FirstRun::ProcessMasterPreferences(user_data_dir, master_prefs_.get());
    AddFirstRunNewTabs(browser_init_.get(), master_prefs_->new_tabs);

    // If we are running in App mode, we do not want to show the importer
    // (first run) UI.
    if (!first_run_ui_bypass &&
        (parsed_command_line().HasSwitch(switches::kApp) ||
         parsed_command_line().HasSwitch(switches::kAppId) ||
         parsed_command_line().HasSwitch(switches::kNoFirstRun)))
      first_run_ui_bypass = true;
  }

  // TODO(viettrungluu): why don't we run this earlier?
  if (!parsed_command_line().HasSwitch(switches::kNoErrorDialogs))
    WarnAboutMinimumSystemRequirements();

  // Enable print preview once for supported platforms.
#if defined(GOOGLE_CHROME_BUILD)
  local_state->RegisterBooleanPref(prefs::kPrintingPrintPreviewEnabledOnce,
                                   false,
                                   PrefService::UNSYNCABLE_PREF);
  if (!local_state->GetBoolean(prefs::kPrintingPrintPreviewEnabledOnce)) {
    local_state->SetBoolean(prefs::kPrintingPrintPreviewEnabledOnce, true);
    about_flags::SetExperimentEnabled(local_state, "print-preview", true);
  }
#endif

#if defined(TOUCH_UI)
  // Always add the --views-desktop flag, if not already set.
  // If the user disabled the flag from about:flags, it will get unset by the
  // call to ConvertFlagsToSwitches in the following line.
  if (!parsed_command_line().HasSwitch(switches::kViewsDesktop))
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kViewsDesktop,
                                                        "other");
#endif

  // Convert active labs into switches. Modifies the current command line.
  about_flags::ConvertFlagsToSwitches(local_state,
                                      CommandLine::ForCurrentProcess());

  // Reset the command line in the crash report details, since we may have
  // just changed it to include experiments.
  child_process_logging::SetCommandLine(CommandLine::ForCurrentProcess());

  InitializeNetworkOptions(parsed_command_line());
  InitializeURLRequestThrottlerManager(browser_process_->net_log());

  // Initialize histogram synchronizer system. This is a singleton and is used
  // for posting tasks via NewRunnableMethod. Its deleted when it goes out of
  // scope. Even though NewRunnableMethod does AddRef and Release, the object
  // will not be deleted after the Task is executed.
  histogram_synchronizer_ = new HistogramSynchronizer();
  tracking_synchronizer_ = new chrome_browser_metrics::TrackingSynchronizer();

  // Now the command line has been mutated based on about:flags, we can
  // set up metrics and initialize field trials.
  MetricsService* metrics = SetupMetricsAndFieldTrials(local_state);

#if defined(USE_WEBKIT_COMPOSITOR)
  // We need to ensure WebKit has been initialized before we start the WebKit
  // compositor. This is done by the ResourceDispatcherHost on creation.
  g_browser_process->resource_dispatcher_host();
#endif

  // Now that all preferences have been registered, set the install date
  // for the uninstall metrics if this is our first run. This only actually
  // gets used if the user has metrics reporting enabled at uninstall time.
  int64 install_date =
      local_state->GetInt64(prefs::kUninstallMetricsInstallDate);
  if (install_date == 0) {
    local_state->SetInt64(prefs::kUninstallMetricsInstallDate,
                          base::Time::Now().ToTimeT());
  }

#if defined(OS_MACOSX)
  // Get the Keychain API to register for distributed notifications on the main
  // thread, which has a proper CFRunloop, instead of later on the I/O thread,
  // which doesn't. This ensures those notifications will get delivered
  // properly. See issue 37766.
  // (Note that the callback mask here is empty. I don't want to register for
  // any callbacks, I just want to initialize the mechanism.)
  SecKeychainAddCallback(&KeychainCallback, 0, NULL);
#endif

  CreateChildThreads(browser_process_.get());

#if defined(OS_CHROMEOS)
  // Now that the file thread exists we can record our stats.
  chromeos::BootTimesLoader::Get()->RecordChromeMainStats();

#if defined(TOOLKIT_USES_GTK)
  // Read locale-specific GTK resource information.
  std::string gtkrc = l10n_util::GetStringUTF8(IDS_LOCALE_GTKRC);
  if (!gtkrc.empty())
    gtk_rc_parse_string(gtkrc.c_str());
#else
  // TODO(saintlou): Need to provide an Aura equivalent.
  NOTIMPLEMENTED();
#endif

  // Trigger prefetching of ownership status.
  chromeos::OwnershipService::GetSharedInstance()->Prewarm();
#endif

  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

#if defined(OS_WIN)
  // On Windows, we use our startup as an opportunity to do upgrade/uninstall
  // tasks.  Those care whether the browser is already running.  On Linux/Mac,
  // upgrade/uninstall happen separately.
  bool already_running = browser_util::IsBrowserAlreadyRunning();

  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (parsed_command_line().HasSwitch(switches::kUninstall)) {
    return DoUninstallTasks(already_running);
  }

  if (parsed_command_line().HasSwitch(switches::kHideIcons) ||
      parsed_command_line().HasSwitch(switches::kShowIcons)) {
    return ChromeBrowserMainPartsWin::HandleIconsCommands(
        parsed_command_line_);
  }
#endif

  if (parsed_command_line().HasSwitch(switches::kMakeDefaultBrowser)) {
    return ShellIntegration::SetAsDefaultBrowser() ?
        static_cast<int>(content::RESULT_CODE_NORMAL_EXIT) :
        static_cast<int>(chrome::RESULT_CODE_SHELL_INTEGRATION_FAILED);
  }

  // If the command line specifies --pack-extension, attempt the pack extension
  // startup action and exit.
  if (parsed_command_line().HasSwitch(switches::kPackExtension)) {
    ExtensionsStartupUtil extension_startup_util;
    if (extension_startup_util.PackExtension(parsed_command_line()))
      return content::RESULT_CODE_NORMAL_EXIT;
    return chrome::RESULT_CODE_PACK_EXTENSION_ERROR;
  }

#if !defined(OS_MACOSX)
  // In environments other than Mac OS X we support import of settings
  // from other browsers. In case this process is a short-lived "import"
  // process that another browser runs just to import the settings, we
  // don't want to be checking for another browser process, by design.
  if (!(parsed_command_line().HasSwitch(switches::kImport) ||
        parsed_command_line().HasSwitch(switches::kImportFromFile))) {
#endif
    // When another process is running, use that process instead of starting a
    // new one. NotifyOtherProcess will currently give the other process up to
    // 20 seconds to respond. Note that this needs to be done before we attempt
    // to read the profile.
    notify_result_ = process_singleton_->NotifyOtherProcessOrCreate();
    switch (notify_result_) {
      case ProcessSingleton::PROCESS_NONE:
        // No process already running, fall through to starting a new one.
        break;

      case ProcessSingleton::PROCESS_NOTIFIED:
#if defined(OS_POSIX) && !defined(OS_MACOSX)
        printf("%s\n", base::SysWideToNativeMB(UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_USED_EXISTING_BROWSER))).c_str());
#endif
        return content::RESULT_CODE_NORMAL_EXIT;

      case ProcessSingleton::PROFILE_IN_USE:
        return chrome::RESULT_CODE_PROFILE_IN_USE;

      case ProcessSingleton::LOCK_ERROR:
        LOG(ERROR) << "Failed to create a ProcessSingleton for your profile "
                      "directory. This means that running multiple instances "
                      "would start multiple browser processes rather than "
                      "opening a new window in the existing process. Aborting "
                      "now to avoid profile corruption.";
        return chrome::RESULT_CODE_PROFILE_IN_USE;

      default:
        NOTREACHED();
    }
#if !defined(OS_MACOSX)  // closing brace for if
  }
#endif

#if defined(USE_X11)
  SetBrowserX11ErrorHandlers();
#endif

  // Profile creation ----------------------------------------------------------

#if defined(OS_CHROMEOS)
  // Initialize the screen locker now so that it can receive
  // LOGIN_USER_CHANGED notification from UserManager.
  chromeos::ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager();

  // TODO(abarth): Should this move to InitializeNetworkOptions()?
  // Allow access to file:// on ChromeOS for tests.
  if (parsed_command_line().HasSwitch(switches::kAllowFileAccess))
    net::URLRequest::AllowFileAccess();

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
    chromeos::UserManager::Get()->UserLoggedIn(username);

    // Redirects Chrome logging to the user data dir.
    logging::RedirectChromeLogging(parsed_command_line());

    // Initialize user policy before creating the profile so the profile
    // initialization code sees policy settings.
    g_browser_process->browser_policy_connector()->InitializeUserPolicy(
        username, false  /* wait_for_policy_fetch */);
  }
#endif

  if (is_first_run) {
    // Warn the ProfileManager that an import process will run, possibly
    // locking the WebDataService directory of the next Profile created.
    g_browser_process->profile_manager()->SetWillImport();
  }

  profile_ = CreateProfile(parameters(), user_data_dir, parsed_command_line());
  if (!profile_)
    return content::RESULT_CODE_NORMAL_EXIT;

  // Autoload any profiles which are running background apps.
  // TODO(rlp): Do this on a separate thread. See http://crbug.com/99075.
  if (!BackgroundModeManager::IsBackgroundModePermanentlyDisabled(
      &parsed_command_line())) {
    g_browser_process->profile_manager()->AutoloadProfiles();
  }
  // Post-profile init ---------------------------------------------------------

#if defined(OS_CHROMEOS)
  // Pass the TokenService pointer to the policy connector so user policy can
  // grab a token and register with the policy server.
  // TODO(mnissler): Remove once OAuth is the only authentication mechanism.
  if (parsed_command_line().HasSwitch(switches::kLoginUser) &&
      !parsed_command_line().HasSwitch(switches::kLoginPassword)) {
    g_browser_process->browser_policy_connector()->SetUserPolicyTokenService(
        profile_->GetTokenService());
  }
#endif

#if !defined(OS_MACOSX)
  // Importing other browser settings is done in a browser-like process
  // that exits when this task has finished.
  // TODO(port): Port the Mac's IPC-based implementation to other platforms to
  //             replace this implementation. http://crbug.com/22142
  if (parsed_command_line().HasSwitch(switches::kImport) ||
      parsed_command_line().HasSwitch(switches::kImportFromFile)) {
    return FirstRun::ImportNow(profile_, parsed_command_line());
  }
#endif

#if defined(OS_WIN)
  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && upgrade_util::DoUpgradeTasks(parsed_command_line()))
    return content::RESULT_CODE_NORMAL_EXIT;

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check should only happen here, after all the checks above
  // (uninstall, resource bundle initialization, other chrome browser
  // processes etc).
  // Do not allow this to occur for Chrome Frame user-to-system handoffs.
  if (!parsed_command_line().HasSwitch(switches::kChromeFrame) &&
      ChromeBrowserMainPartsWin::CheckMachineLevelInstall()) {
    return chrome::RESULT_CODE_MACHINE_LEVEL_INSTALL_EXISTS;
  }
#endif

  // Create the TranslateManager singleton.
  translate_manager_ = TranslateManager::GetInstance();
  DCHECK(translate_manager_ != NULL);

#if defined(OS_MACOSX)
  if (!parsed_command_line().HasSwitch(switches::kNoFirstRun)) {
    // Disk image installation is sort of a first-run task, so it shares the
    // kNoFirstRun switch.
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      return content::RESULT_CODE_NORMAL_EXIT;
    }
  }
#endif

  // Show the First Run UI if this is the first time Chrome has been run on
  // this computer, or we're being compelled to do so by a command line flag.
  // Note that this be done _after_ the PrefService is initialized and all
  // preferences are registered, since some of the code that the importer
  // touches reads preferences.
  if (is_first_run) {
    if (!first_run_ui_bypass) {
      FirstRun::AutoImport(profile_,
                           master_prefs_->homepage_defined,
                           master_prefs_->do_import_items,
                           master_prefs_->dont_import_items,
                           master_prefs_->run_search_engine_experiment,
                           master_prefs_->randomize_search_engine_experiment,
                           master_prefs_->make_chrome_default,
                           process_singleton_.get());
#if defined(OS_POSIX)
      // On Windows, the download is tagged with enable/disable stats so there
      // is no need for this code.

      // If stats reporting was turned on by the first run dialog then toggle
      // the pref.
      if (GoogleUpdateSettings::GetCollectStatsConsent())
        local_state->SetBoolean(prefs::kMetricsReportingEnabled, true);
#endif  // OS_POSIX
    }  // if (!first_run_ui_bypass)

    Browser::SetNewHomePagePrefs(profile_->GetPrefs());
    g_browser_process->profile_manager()->OnImportFinished(profile_);
  }  // if (is_first_run)

#if defined(OS_WIN)
  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
      parsed_command_line());

  // Registers Chrome with the Windows Restart Manager, which will restore the
  // Chrome session when the computer is restarted after a system update.
  // This could be run as late as WM_QUERYENDSESSION for system update reboots,
  // but should run on startup if extended to handle crashes/hangs/patches.
  // Also, better to run once here than once for each HWND's WM_QUERYENDSESSION.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    ChromeBrowserMainPartsWin::RegisterApplicationRestart(
        parsed_command_line());
  }

#if defined(GOOGLE_CHROME_BUILD)
  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  bool google_search_default = false;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service) {
    const TemplateURL* url_template =
        template_url_service->GetDefaultSearchProvider();
    if (url_template) {
      const TemplateURLRef* urlref = url_template->url();
      if (urlref) {
        google_search_default = urlref->HasGoogleBaseURLs();
      }
    }
  }

  bool google_search_homepage = false;
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service) {
    std::string homepage = pref_service->GetString(prefs::kHomePage);
    google_search_homepage =
        homepage == GoogleURLTracker::kDefaultGoogleHomepage;
  }

  RLZTracker::InitRlzDelayed(is_first_run, master_prefs_->ping_delay,
                             google_search_default, google_search_homepage);

  // Prime the RLZ cache for the home page access point so that its avaiable
  // for the startup page if needed (i.e., when the startup page is set to
  // the home page).
  RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, NULL);
#endif  // GOOGLE_CHROME_BUILD
#endif  // OS_WIN

  // Start watching for hangs during startup. We disarm this hang detector when
  // ThreadWatcher takes over or when browser is shutdown.
  StartupTimeBomb::Arm(base::TimeDelta::FromSeconds(300));

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

  // In unittest mode, this will do nothing.  In normal mode, this will create
  // the global GoogleURLTracker and IntranetRedirectDetector instances, which
  // will promptly go to sleep for five and seven seconds, respectively (to
  // avoid slowing startup), and wake up afterwards to see if they should do
  // anything else.
  //
  // A simpler way of doing all this would be to have some function which could
  // give the time elapsed since startup, and simply have these objects check
  // that when asked to initialize themselves, but this doesn't seem to exist.
  //
  // These can't be created in the BrowserProcessImpl constructor because they
  // need to read prefs that get set after that runs.
  browser_process_->google_url_tracker();
  browser_process_->intranet_redirect_detector();

  // Disable SDCH filtering if switches::kEnableSdch is 0.
  int sdch_enabled = 1;
  if (parsed_command_line().HasSwitch(switches::kEnableSdch)) {
    base::StringToInt(parsed_command_line().GetSwitchValueASCII(
        switches::kEnableSdch), &sdch_enabled);
    if (!sdch_enabled)
      net::SdchManager::EnableSdchSupport(false);
  }
  if (sdch_enabled) {
    // Perform A/B test to measure global impact of SDCH support.
    // Set up a field trial to see what disabling SDCH does to latency of page
    // layout globally.
    base::FieldTrial::Probability kSDCH_DIVISOR = 1000;
    base::FieldTrial::Probability kSDCH_DISABLE_PROBABILITY = 1;  // 0.1% prob.
    // After March 31, 2012 builds, it will always be in default group.
    scoped_refptr<base::FieldTrial> sdch_trial(
        new base::FieldTrial("GlobalSdch", kSDCH_DIVISOR, "global_enable_sdch",
            2012, 3, 31));
    int sdch_enabled_group = sdch_trial->kDefaultGroupNumber;

    sdch_trial->AppendGroup("global_disable_sdch",
                            kSDCH_DISABLE_PROBABILITY);
    if (sdch_enabled_group != sdch_trial->group())
      net::SdchManager::EnableSdchSupport(false);
  }

  if (parsed_command_line().HasSwitch(switches::kEnableWatchdog))
    InstallJankometer(parsed_command_line());

#if defined(OS_WIN) && !defined(GOOGLE_CHROME_BUILD)
  if (parsed_command_line().HasSwitch(switches::kDebugPrint)) {
    FilePath path =
        parsed_command_line().GetSwitchValuePath(switches::kDebugPrint);
    printing::PrintedDocument::set_debug_dump_path(path);
  }
#endif

  HandleTestParameters(parsed_command_line());
  RecordBreakpadStatusUMA(metrics);
  about_flags::RecordUMAStatistics(local_state);
  LanguageUsageMetrics::RecordAcceptLanguages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  LanguageUsageMetrics::RecordApplicationLanguage(
      g_browser_process->GetApplicationLocale());

#if defined(OS_WIN)
  fragmentation_checker::RecordFragmentationMetricForCurrentModule();
#endif

#if defined(OS_CHROMEOS)
  metrics->StartExternalMetrics();

  // Initialize the audio handler on ChromeOS.
  chromeos::AudioHandler::Initialize();

  // Listen for system key events so that the user will be able to adjust the
  // volume on the login screen, if Chrome is running on Chrome OS
  // (i.e. not Linux desktop), and in non-test mode.
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS() &&
      !parameters().ui_task) {  // ui_task is non-NULL when running tests.
    chromeos::SystemKeyEventListener::Initialize();
  }

  // Listen for XI_HierarchyChanged events.
  chromeos::XInputHierarchyChangedEventListener::GetInstance();
#endif

  // The extension service may be available at this point. If the command line
  // specifies --uninstall-extension, attempt the uninstall extension startup
  // action.
  if (parsed_command_line().HasSwitch(switches::kUninstallExtension)) {
    ExtensionsStartupUtil ext_startup_util;
    if (ext_startup_util.UninstallExtension(parsed_command_line(), profile_))
      return content::RESULT_CODE_NORMAL_EXIT;
    return chrome::RESULT_CODE_UNINSTALL_EXTENSION_ERROR;
  }

  // Start watching for a hang.
  MetricsService::LogNeedForCleanShutdown();

#if defined(OS_WIN)
  // We check this here because if the profile is OTR (chromeos possibility)
  // it won't still be accessible after browser is destroyed.
  record_search_engine_ = is_first_run && !profile_->IsOffTheRecord();
#endif

  // ChildProcess:: is a misnomer unless you consider context.  Use
  // of --wait-for-debugger only makes sense when Chrome itself is a
  // child process (e.g. when launched by PyAuto).
  if (parsed_command_line().HasSwitch(switches::kWaitForDebugger)) {
    ChildProcess::WaitForDebugger("Browser");
  }

#if defined(OS_CHROMEOS)
  // Wait until here to start the out-of-memory priority manager so that
  // we give the most amount of time for the other services to start up
  // before we start adjusting the oom priority.
  g_browser_process->oom_priority_manager()->Start();
#endif

  // Create the instance of the cloud print proxy service so that it can launch
  // the service process if needed. This is needed because the service process
  // might have shutdown because an update was available.
  // TODO(torne): this should maybe be done with
  // ProfileKeyedServiceFactory::ServiceIsCreatedWithProfile() instead?
  CloudPrintProxyServiceFactory::GetForProfile(profile_);

  // Initialize GpuDataManager and collect preliminary gpu info on FILE thread.
  // Upon completion, it posts GpuBlacklist auto update task on UI thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GpuBlacklistUpdater::SetupOnFileThread));

  // Start watching all browser threads for responsiveness.
  ThreadWatcherList::StartWatchingAll(parsed_command_line());

#if !defined(DISABLE_NACL)
  NaClProcessHost::EarlyStartup();
#endif

  run_message_loop_ = true;
  return content::RESULT_CODE_NORMAL_EXIT;
}

// Called from MainMessageLoopRun().
void ChromeBrowserMainParts::StartBrowserOrUITask() {
  // Still initializing, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Set the notification UI manager after any desktop initialization in
  // PreMainMessageLoopRun() is complete, and before starting the browser.
  DesktopNotificationServiceFactory::GetForProfile(profile_)->SetUIManager(
      g_browser_process->notification_ui_manager());

  // Tests should be able to tune login manager before showing it.
  // Thus only show login manager in normal (non-testing) mode.
  if (!parameters().ui_task)
    OptionallyRunChromeOSLoginManager(parsed_command_line(), profile_);

  if (parameters().ui_task) {
    // We are in test mode. Run one task and enter the main message loop.
#if defined(OS_MACOSX)
    if (parameters().autorelease_pool)
      parameters().autorelease_pool->Recycle();
#endif
    parameters().ui_task->Run();
    delete parameters().ui_task;
    run_message_loop_ = false;
  } else {
    // Most general initialization is behind us, but opening a
    // tab and/or session restore and such is still to be done.
    base::TimeTicks browser_open_start = base::TimeTicks::Now();

    // We are in regular browser boot sequence. Open initial tabs and enter the
    // main message loop.
    if (browser_init_->Start(parsed_command_line(), FilePath(), profile_,
                             &result_code_)) {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
      // Initialize autoupdate timer. Timer callback costs basically nothing
      // when browser is not in persistent mode, so it's OK to let it ride on
      // the main thread. This needs to be done here because we don't want
      // to start the timer when Chrome is run inside a test harness.
      g_browser_process->StartAutoupdateTimer();
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
      // On Linux, the running exe will be updated if an upgrade becomes
      // available while the browser is running.  We need to save the last
      // modified time of the exe, so we can compare to determine if there is
      // an upgrade while the browser is kept alive by a persistent extension.
      upgrade_util::SaveLastModifiedTimeOfExe();
#endif

      // Record now as the last successful chrome start.
      GoogleUpdateSettings::SetLastRunTime();

#if defined(OS_MACOSX)
      // Call Recycle() here as late as possible, before going into the loop
      // because Start() will add things to it while creating the main window.
      if (parameters().autorelease_pool)
        parameters().autorelease_pool->Recycle();
#endif

      RecordPreReadExperimentTime("Startup.BrowserOpenTabs",
                                  base::TimeTicks::Now() - browser_open_start);

      // TODO(mad): Move this call in a proper place on CrOS.
      // http://crosbug.com/17687
#if !defined(OS_CHROMEOS)
      // If we're running tests (ui_task is non-null), then we don't want to
      // call FetchLanguageListFromTranslateServer
      if (parameters().ui_task == NULL && translate_manager_ != NULL) {
        // TODO(willchan): Get rid of this after TranslateManager doesn't use
        // the default request context. http://crbug.com/89396.
        // This is necessary to force |default_request_context_| to be
        // initialized.
        profile_->GetRequestContext();
        translate_manager_->FetchLanguageListFromTranslateServer(
            profile_->GetPrefs());
      }
#endif

      run_message_loop_ = true;
    } else {
      run_message_loop_ = false;
    }
  }
  browser_init_.reset();
}

bool ChromeBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Set the result code set in PreMainMessageLoopRun or set above.
  *result_code = result_code_;

  // TODO(stevenjb): Move this to another phase, and/or clean up
  // PreMainMessageLoopRun() so that this can happen after desktop
  // initilaization and before running the main loop.
  if (run_message_loop_)
    StartBrowserOrUITask();

  if (!run_message_loop_)
    return true;  // Don't run the default message loop.

  // This should be invoked as close to the start of the browser's
  // UI thread message loop as possible to get a stable measurement
  // across versions.
  RecordBrowserStartupTime();

#if defined(USE_AURA)
  aura::Desktop::GetInstance()->Run();
#elif defined(TOOLKIT_VIEWS)
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);
#elif defined(USE_X11)
  MessageLoopForUI::current()->RunWithDispatcher(NULL);
#elif defined(OS_POSIX)
  MessageLoopForUI::current()->Run();
#endif
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("UIMessageLoopEnded",
                                                        true);
#endif

  return true;
}

void ChromeBrowserMainParts::PostMainMessageLoopRun() {
  // Start watching for jank during shutdown. It gets disarmed when
  // |shutdown_watcher_| object is destructed.
  shutdown_watcher_->Arm(base::TimeDelta::FromSeconds(90));

  // Disarm the startup hang detector time bomb if it is still Arm'ed.
  StartupTimeBomb::Disarm();

#if defined(OS_WIN)
  // If it's the first run, log the search engine chosen.  We wait until
  // shutdown because otherwise we can't be sure the user has finished
  // selecting a search engine through the dialog reached from the first run
  // bubble link.
  if (record_search_engine_) {
    TemplateURLService* url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    const TemplateURL* default_search_engine =
        url_service->GetDefaultSearchProvider();
    // The default engine can be NULL if the administrator has disabled
    // default search.
    SearchEngineType search_engine_type =
        default_search_engine ? default_search_engine->search_engine_type() :
                                SEARCH_ENGINE_OTHER;
    // Record the search engine chosen.
    if (master_prefs_->run_search_engine_experiment) {
      UMA_HISTOGRAM_ENUMERATION(
          "Chrome.SearchSelectExperiment",
          search_engine_type,
          SEARCH_ENGINE_MAX);
      // If the selection has been randomized, also record the winner by slot.
      if (master_prefs_->randomize_search_engine_experiment) {
        size_t engine_pos = url_service->GetSearchEngineDialogSlot();
        if (engine_pos < 4) {
          std::string experiment_type = "Chrome.SearchSelectExperimentSlot";
          // Nicer in UMA if slots are 1-based.
          experiment_type.push_back('1' + engine_pos);
          UMA_HISTOGRAM_ENUMERATION(
              experiment_type,
              search_engine_type,
              SEARCH_ENGINE_MAX);
        } else {
          NOTREACHED() << "Invalid search engine selection slot.";
        }
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Chrome.SearchSelectExempt",
          search_engine_type,
          SEARCH_ENGINE_MAX);
    }
  }
#endif

#if defined(OS_CHROMEOS)
  g_browser_process->oom_priority_manager()->Stop();
#endif

  // Some tests don't set parameters.ui_task, so they started translate
  // language fetch that was never completed so we need to cleanup here
  // otherwise it will be done by the destructor in a wrong thread.
  if (parameters().ui_task == NULL && translate_manager_ != NULL)
    translate_manager_->CleanupPendingUlrFetcher();

  if (notify_result_ == ProcessSingleton::PROCESS_NONE)
    process_singleton_->Cleanup();

  // Stop all tasks that might run on WatchDogThread.
  ThreadWatcherList::StopWatchingAll();

  g_browser_process->metrics_service()->Stop();

#if defined(OS_CHROMEOS)
  // The XInput2 event listener needs to be shut down earlier than when
  // Singletons are finally destroyed in AtExitManager.
  chromeos::XInputHierarchyChangedEventListener::GetInstance()->Stop();

  // chromeos::SystemKeyEventListener::Shutdown() is always safe to call,
  // even if Initialize() wasn't called.
  chromeos::SystemKeyEventListener::Shutdown();
  chromeos::AudioHandler::Shutdown();
#endif

  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  ignore_result(browser_process_.release());
  browser_shutdown::Shutdown();
  master_prefs_.reset();
  process_singleton_.reset();

  // We need to do this check as late as possible, but due to modularity, this
  // may be the last point in Chrome.  This would be more effective if done at
  // a higher level on the stack, so that it is impossible for an early return
  // to bypass this code.  Perhaps we need a *final* hook that is called on all
  // paths from content/browser/browser_main.
  CHECK(MetricsService::UmaMetricsProperlyShutdown());
}

// This code is specific to the Windows-only PreReadExperiment field-trial.
void RecordPreReadExperimentTime(const char* name, base::TimeDelta time) {
  DCHECK(name != NULL);

  // This gets called with different histogram names, so we don't want to use
  // the UMA_HISTOGRAM_CUSTOM_TIMES macro--it uses a static variable, and the
  // first call wins.
  AddPreReadHistogramTime(name, time);

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
  // The pre-read experiment is Windows and Google Chrome specific.
  scoped_ptr<base::Environment> env(base::Environment::Create());

  // Only record the sub-histogram result if the experiment is running
  // (environment variable is set, and valid).
  std::string pre_read;
  if (env->GetVar(chrome::kPreReadEnvironmentVariable, &pre_read) &&
      (pre_read == "0" || pre_read == "1")) {
    std::string uma_name(name);
    uma_name += "_PreRead";
    uma_name += pre_read == "1" ? "Enabled" : "Disabled";
    AddPreReadHistogramTime(uma_name.c_str(), time);
  }
#endif
#endif
}
