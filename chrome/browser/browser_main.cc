// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ui/base/l10n/l10n_util.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
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
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_startup.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/blob_url_request_job_factory.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker.h"
#include "chrome/browser/net/chrome_dns_cert_provenance_checker_factory.h"
#include "chrome/browser/net/metadata_url_request.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/gfx_resource_provider.h"
#include "chrome/common/hi_res_timer_manager.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "gfx/gfx_module.h"
#include "grit/app_locale_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/system_monitor/system_monitor.h"

#if defined(USE_LINUX_BREAKPAD)
#include "base/linux_util.h"
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <dbus/dbus-glib.h>
#include "chrome/browser/browser_main_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/oom_priority_manager.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include <commctrl.h>
#include <shellapi.h>
#include <windows.h>

#include "app/win/scoped_com_initializer.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/ui/views/user_data_dir_dialog.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "gfx/platform_font_win.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "printing/printed_document.h"
#include "sandbox/src/sandbox.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include <Security/Security.h>
#include "chrome/browser/ui/cocoa/install_from_dmg.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "views/focus/accelerator_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/metrics_cros_settings_provider.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include "gfx/gtk_util.h"
#endif

// BrowserMainParts ------------------------------------------------------------

BrowserMainParts::BrowserMainParts(const MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line_) {
}

BrowserMainParts::~BrowserMainParts() {
}

// BrowserMainParts: |EarlyInitialization()| and related -----------------------

void BrowserMainParts::EarlyInitialization() {
  PreEarlyInitialization();

  if (parsed_command_line().HasSwitch(switches::kEnableBenchmarking))
    base::FieldTrial::EnableBenchmarking();

  // Note: make sure to call ConnectionFieldTrial() before
  // ProxyConnectionsFieldTrial().
  ConnectionFieldTrial();
  SocketTimeoutFieldTrial();
  ProxyConnectionsFieldTrial();
  SpdyFieldTrial();
  PrefetchFieldTrial();
  ConnectBackupJobsFieldTrial();
  InitializeSSL();

  if (parsed_command_line().HasSwitch(switches::kEnableDNSSECCerts))
    net::SSLConfigService::EnableDNSSEC();
  if (parsed_command_line().HasSwitch(switches::kDisableSSLFalseStart))
    net::SSLConfigService::DisableFalseStart();
  if (parsed_command_line().HasSwitch(switches::kAllowSSLMITMProxies))
    net::SSLConfigService::AllowMITMProxies();
  // Disabled to stop people playing with it.
  // if (parsed_command_line().HasSwitch(switches::kEnableSnapStart))
  //   net::SSLConfigService::EnableSnapStart();
  if (parsed_command_line().HasSwitch(
          switches::kEnableDNSCertProvenanceChecking)) {
    net::SSLConfigService::EnableDNSCertProvenanceChecking();
  }

  if (parsed_command_line().HasSwitch(switches::kEnableTcpFastOpen))
    net::set_tcp_fastopen_enabled(true);

  PostEarlyInitialization();
}

// This is an A/B test for the maximum number of persistent connections per
// host. Currently Chrome, Firefox, and IE8 have this value set at 6. Safari
// uses 4, and Fasterfox (a plugin for Firefox that supposedly configures it to
// run faster) uses 8. We would like to see how much of an effect this value has
// on browsing. Too large a value might cause us to run into SYN flood detection
// mechanisms.
void BrowserMainParts::ConnectionFieldTrial() {
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
// result in more ERR_CONNECT_RESETs, requiring one RTT to receive the RST
// packet and possibly another RTT to re-establish the connection.
void BrowserMainParts::SocketTimeoutFieldTrial() {
  const base::FieldTrial::Probability kIdleSocketTimeoutDivisor = 100;
  // 1% probability for all experimental settings.
  const base::FieldTrial::Probability kSocketTimeoutProbability = 1;

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> socket_timeout_trial(
      new base::FieldTrial("IdleSktToImpact", kIdleSocketTimeoutDivisor,
          "idle_timeout_60", 2011, 6, 30));
  const int socket_timeout_60 = socket_timeout_trial->kDefaultGroupNumber;

  const int socket_timeout_5 =
      socket_timeout_trial->AppendGroup("idle_timeout_5",
                                        kSocketTimeoutProbability);
  const int socket_timeout_10 =
      socket_timeout_trial->AppendGroup("idle_timeout_10",
                                        kSocketTimeoutProbability);
  const int socket_timeout_20 =
      socket_timeout_trial->AppendGroup("idle_timeout_20",
                                        kSocketTimeoutProbability);

  const int idle_to_trial_group = socket_timeout_trial->group();

  if (idle_to_trial_group == socket_timeout_5) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(5);
  } else if (idle_to_trial_group == socket_timeout_10) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(10);
  } else if (idle_to_trial_group == socket_timeout_20) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(20);
  } else if (idle_to_trial_group == socket_timeout_60) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(60);
  } else {
    NOTREACHED();
  }
}

void BrowserMainParts::ProxyConnectionsFieldTrial() {
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
void BrowserMainParts::SpdyFieldTrial() {
  bool is_spdy_trial = false;
  if (parsed_command_line().HasSwitch(switches::kUseSpdy)) {
    std::string spdy_mode =
        parsed_command_line().GetSwitchValueASCII(switches::kUseSpdy);
    net::HttpNetworkLayer::EnableSpdy(spdy_mode);
  } else {
    const base::FieldTrial::Probability kSpdyDivisor = 100;
    // 10% to preclude SPDY.
    base::FieldTrial::Probability npnhttp_probability = 10;

    // After June 30, 2011 builds, it will always be in default group.
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial(
            "SpdyImpact", kSpdyDivisor, "npn_with_spdy", 2011, 6, 30));

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
  }

  // Setup SPDY CWND Field trial.
  const base::FieldTrial::Probability kSpdyCwndDivisor = 100;
  const base::FieldTrial::Probability kSpdyCwnd32 = 20;     // fixed at 32
  const base::FieldTrial::Probability kSpdyCwnd16 = 20;     // fixed at 16
  const base::FieldTrial::Probability kSpdyCwndMin16 = 20;  // no less than 16
  const base::FieldTrial::Probability kSpdyCwndMin10 = 20;  // no less than 10

  // After June 30, 2011 builds, it will always be in default group
  // (cwndDynamic).
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial(
          "SpdyCwnd", kSpdyCwndDivisor, "cwndDynamic", 2011, 6, 30));

  trial->AppendGroup("cwnd32", kSpdyCwnd32);
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

// If any of --enable-prerender, --enable-content-prefetch or
// --disable-content-prefetch are set, use those to determine if
// prefetch is enabled. Otherwise, randomly assign users to an A/B test for
// prefetching.
void BrowserMainParts::PrefetchFieldTrial() {
  if (parsed_command_line().HasSwitch(switches::kEnableContentPrefetch) ||
      parsed_command_line().HasSwitch(switches::kEnablePagePrerender))
    ResourceDispatcherHost::set_is_prefetch_enabled(true);
  else if (parsed_command_line().HasSwitch(switches::kDisableContentPrefetch)) {
    ResourceDispatcherHost::set_is_prefetch_enabled(false);
  } else {
    const base::FieldTrial::Probability kPrefetchDivisor = 1000;
    const base::FieldTrial::Probability no_prefetch_probability = 500;
    // After June 30, 2011 builds, it will always be in default group.
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial("Prefetch", kPrefetchDivisor,
            "ContentPrefetchEnabled", 2011, 6, 30));
    const int yes_prefetch_grp = trial->kDefaultGroupNumber;
    trial->AppendGroup("ContentPrefetchDisabled", no_prefetch_probability);
    const int trial_grp = trial->group();
    ResourceDispatcherHost::set_is_prefetch_enabled(
        trial_grp == yes_prefetch_grp);
  }
}

// If neither --enable-connect-backup-jobs or --disable-connect-backup-jobs is
// specified, run an A/B test for automatically establishing backup TCP
// connections when a certain timeout value is exceeded.
void BrowserMainParts::ConnectBackupJobsFieldTrial() {
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
    // After June 30, 2011 builds, it will always be in defaut group.
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

// BrowserMainParts: |MainMessageLoopStart()| and related ----------------------

void BrowserMainParts::MainMessageLoopStart() {
  PreMainMessageLoopStart();

  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

  // TODO(viettrungluu): should these really go before setting the thread name?
  system_monitor_.reset(new ui::SystemMonitor);
  hi_res_timer_manager_.reset(new HighResolutionTimerManager);
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());

  InitializeMainThread();

  PostMainMessageLoopStart();
}

void BrowserMainParts::InitializeMainThread() {
  const char* kThreadName = "CrBrowserMain";
  base::PlatformThread::SetName(kThreadName);
  main_message_loop().set_thread_name(kThreadName);

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(new BrowserThread(BrowserThread::UI,
                                       MessageLoop::current()));
}

// -----------------------------------------------------------------------------
// TODO(viettrungluu): move more/rest of BrowserMain() into above structure

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
    exit(!chromeos::CrosLibrary::Get()->EnsureLoaded());
#endif
}

void RunUIMessageLoop(BrowserProcess* browser_process) {
  TRACE_EVENT_BEGIN("BrowserMain:MESSAGE_LOOP", 0, "");

#if !defined(OS_CHROMEOS)
  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  // TODO(evanm): turn this on for all platforms.
  //   http://code.google.com/p/chromium/issues/detail?id=60211
  base::ThreadRestrictions::SetIOAllowed(false);
#endif

#if defined(TOOLKIT_VIEWS)
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
#elif defined(USE_X11)
  MessageLoopForUI::current()->Run(NULL);
#elif defined(OS_POSIX)
  MessageLoopForUI::current()->Run();
#endif
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("UIMessageLoopEnded",
                                                        true);
#endif

  TRACE_EVENT_END("BrowserMain:MESSAGE_LOOP", 0, "");
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

  if (parsed_command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    net::HttpStreamFactory::set_ignore_certificate_errors(true);

  if (parsed_command_line.HasSwitch(switches::kHostRules))
    net::HttpStreamFactory::SetHostMappingRules(
        parsed_command_line.GetSwitchValueASCII(switches::kHostRules));

  if (parsed_command_line.HasSwitch(switches::kMaxSpdySessionsPerDomain)) {
    int value;
    base::StringToInt(parsed_command_line.GetSwitchValueASCII(
            switches::kMaxSpdySessionsPerDomain),
        &value);
    net::SpdySessionPool::set_max_sessions_per_domain(value);
  }

  if (parsed_command_line.HasSwitch(switches::kDisableEnforcedThrottling)) {
    net::URLRequestThrottlerManager::GetInstance()->
        set_enforce_throttling(false);
  }

  SetDnsCertProvenanceCheckerFactory(CreateChromeDnsCertProvenanceChecker);
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

  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  local_state->RegisterStringPref(prefs::kApplicationLocale,
                                  std::string());
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
        PrefService::CreatePrefService(parent_profile, NULL, NULL));
    parent_local_state->RegisterStringPref(prefs::kApplicationLocale,
                                           std::string());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state->GetString(prefs::kApplicationLocale));
  }

  return local_state;
}

// Windows-specific initialization code for the sandbox broker services. This
// is just a NOP on non-Windows platforms to reduce ifdefs later on.
void InitializeBrokerServices(const MainFunctionParams& parameters,
                              const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services) {
    sandbox::InitBrokerServices(broker_services);
    if (!parsed_command_line.HasSwitch(switches::kNoSandbox)) {
      bool use_winsta = !parsed_command_line.HasSwitch(
                            switches::kDisableAltWinstation);
      // Precreate the desktop and window station used by the renderers.
      sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(use_winsta);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
      policy->Release();
    }
  }
#endif
}

// Initializes the metrics service with the configuration for this process,
// returning the created service (guaranteed non-NULL).
MetricsService* InitializeMetrics(const CommandLine& parsed_command_line,
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
  } else {
    // If the user permits metrics reporting with the checkbox in the
    // prefs, we turn on recording.  We disable metrics completely for
    // non-official builds.
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
    bool enabled = chromeos::MetricsCrosSettingsProvider::GetMetricsStatus();
#else
    bool enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
    metrics->SetUserPermitsUpload(enabled);
    if (enabled) {
      metrics->Start();
      chrome_browser_net_websocket_experiment::
          WebSocketExperimentRunner::Start();
    }
#endif
  }

  return metrics;
}

// Initializes the profile, possibly doing some user prompting to pick a
// fallback profile. Returns the newly created profile, or NULL if startup
// should not continue.
Profile* CreateProfile(const MainFunctionParams& parameters,
                       const FilePath& user_data_dir) {
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile(
      user_data_dir);
  if (profile)
    return profile;

#if defined(OS_WIN)
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
    CommandLine new_command_line = parameters.command_line_;
    new_command_line.AppendSwitchPath(switches::kUserDataDir,
                                      new_user_data_dir);
    base::LaunchApp(new_command_line, false, false, NULL);
  }
#else
  // TODO(port): fix this.  See comments near the definition of
  // user_data_dir.  It is better to CHECK-fail here than it is to
  // silently exit because of missing code in the above test.
  CHECK(profile) << "Cannot get default profile.";
#endif

  return NULL;
}

#if defined(OS_WIN)

// gfx::Font callbacks
void AdjustUIFont(LOGFONT* logfont) {
  l10n_util::AdjustUIFont(logfont);
}

int GetMinimumFontSize() {
  int min_font_size;
  base::StringToInt(l10n_util::GetStringUTF16(IDS_MINIMUM_UI_FONT_SIZE),
                    &min_font_size);
  return min_font_size;
}

#elif defined(OS_CHROMEOS)
// Changes the UI font if non-default font name is specified in
// IDS_UI_FONT_FAMILY_CROS. This is necessary as the default font
// specified in /etc/gtk-2.0/gtrkc may not work well for some languages
// For instance, ChromeDroidSans does not work well for Japanese users,
// since Chinese glyphs are used for Kanji characters.
void MaybeChangeUIFont() {
  const std::string font_name =
      l10n_util::GetStringUTF8(IDS_UI_FONT_FAMILY_CROS);
  // The font name should not be empty here, but just in case.
  if (font_name == "default" || font_name.empty()) {
    return;
  }
  gtk_util::SetGtkFont(font_name);
}
#endif

#if defined(TOOLKIT_USES_GTK)
static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer userdata) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  if (strstr(message, "Loading IM context type") ||
      strstr(message, "wrong ELF class: ELFCLASS64")) {
    // http://crbug.com/9643
    // Until we have a real 64-bit build or all of these 32-bit package issues
    // are sorted out, don't fatal on ELF 32/64-bit mismatch warnings and don't
    // spam the user with more than one of them.
    static bool alerted = false;
    if (!alerted) {
      LOG(ERROR) << "Bug 9643: " << log_domain << ": " << message;
      alerted = true;
    }
  } else if (strstr(message, "gtk_widget_size_allocate(): attempt to "
                             "allocate widget with width") &&
             !GTK_CHECK_VERSION(2, 16, 1)) {
    // This warning only occurs in obsolete versions of GTK and is harmless.
    // http://crbug.com/11133
  } else if (strstr(message, "Theme file for default has no") ||
             strstr(message, "Theme directory") ||
             strstr(message, "theme pixmap")) {
    LOG(ERROR) << "GTK theme error: " << message;
  } else if (strstr(message, "gtk_drag_dest_leave: assertion")) {
    LOG(ERROR) << "Drag destination deleted: http://crbug.com/18557";
  } else {
#ifdef NDEBUG
    LOG(ERROR) << log_domain << ": " << message;
#else
    LOG(FATAL) << log_domain << ": " << message;
#endif
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* kLogDomains[] = { NULL, "Gtk", "Gdk", "GLib", "GLib-GObject" };
  for (size_t i = 0; i < arraysize(kLogDomains); i++) {
    g_log_set_handler(kLogDomains[i],
                      static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION |
                                                  G_LOG_FLAG_FATAL |
                                                  G_LOG_LEVEL_ERROR |
                                                  G_LOG_LEVEL_CRITICAL |
                                                  G_LOG_LEVEL_WARNING),
                      GLibLogHandler,
                      NULL);
  }
}
#endif

void InitializeToolkit(const MainFunctionParams& parameters) {
  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.

#if defined(TOOLKIT_USES_GTK)
  // We want to call g_thread_init(), but in some codepaths (tests) it
  // is possible it has already been called.  In older versions of
  // GTK, it is an error to call g_thread_init twice; unfortunately,
  // the API to tell whether it has been called already was also only
  // added in a newer version of GTK!  Thankfully, this non-intuitive
  // check is actually equivalent and sufficient to work around the
  // error.
  if (!g_thread_supported())
    g_thread_init(NULL);
  // Glib type system initialization. Needed at least for gconf,
  // used in net/proxy/proxy_config_service_linux.cc. Most likely
  // this is superfluous as gtk_init() ought to do this. It's
  // definitely harmless, so retained as a reminder of this
  // requirement for gconf.
  g_type_init();
  // We use glib-dbus for geolocation and it's possible other libraries
  // (e.g. gnome-keyring) will use it, so initialize its threading here
  // as well.
  dbus_g_thread_init();
  gfx::GtkInitFromCommandLine(parameters.command_line_);
  SetUpGLibLogHandler();
#endif

#if defined(TOOLKIT_GTK)
  // It is important for this to happen before the first run dialog, as it
  // styles the dialog as well.
  gtk_util::InitRCStyles();
#endif

#if defined(TOOLKIT_VIEWS)
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;
#endif

#if defined(OS_WIN)
  gfx::PlatformFontWin::adjust_font_callback = &AdjustUIFont;
  gfx::PlatformFontWin::get_minimum_font_size_callback = &GetMinimumFontSize;

  // Init common control sex.
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&config);
#endif
}

#if defined(OS_CHROMEOS)

// Class is used to login using passed username and password.
// The instance will be deleted upon success or failure.
class StubLogin : public chromeos::LoginStatusConsumer {
 public:
  explicit StubLogin(std::string username, std::string password) {
    authenticator_ = chromeos::LoginUtils::Get()->CreateAuthenticator(this);
    authenticator_.get()->AuthenticateToLogin(
        g_browser_process->profile_manager()->GetDefaultProfile(),
        username,
        password,
        std::string(),
        std::string());
  }

  void OnLoginFailure(const chromeos::LoginFailure& error) {
    LOG(ERROR) << "Login Failure: " << error.GetErrorString();
    delete this;
  }

  void OnLoginSuccess(const std::string& username,
                      const std::string& password,
                      const GaiaAuthConsumer::ClientLoginResult& credentials,
                      bool pending_requests) {
    chromeos::LoginUtils::Get()->CompleteLogin(username,
                                               password,
                                               credentials,
                                               pending_requests);
    delete this;
  }

  scoped_refptr<chromeos::Authenticator> authenticator_;
};

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line) {
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
  }
}

#else

void OptionallyRunChromeOSLoginManager(const CommandLine& parsed_command_line) {
  // Dummy empty function for non-ChromeOS builds to avoid extra ifdefs below.
}

#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                          SecKeychainCallbackInfo *info, void *context) {
  return noErr;
}
#endif

}  // namespace

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
  Upgrade::RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}
#endif

#if defined(USE_LINUX_BREAKPAD)
bool IsMetricsReportingEnabled(const PrefService* local_state) {
  // Check whether we should initialize the crash reporter. It may be disabled
  // through configuration policy or user preference.
  // The kHeadless environment variable overrides the decision, but only if the
  // crash service is under control of the user. It is used by QA testing
  // infrastructure to switch on generation of crash reports.
#if defined(OS_CHROMEOS)
  bool breakpad_enabled =
      chromeos::MetricsCrosSettingsProvider::GetMetricsStatus();
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

// Main routine for running as the Browser process.
int BrowserMain(const MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN("BrowserMain", 0, "");
  scoped_ptr<BrowserMainParts>
      parts(BrowserMainParts::CreateBrowserMainParts(parameters));

  parts->EarlyInitialization();

  // Must happen before we try to use a message loop or display any UI.
  InitializeToolkit(parameters);

  parts->MainMessageLoopStart();

  // WARNING: If we get a WM_ENDSESSION, objects created on the stack here
  // are NOT deleted. If you need something to run during WM_ENDSESSION add it
  // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

  // !!!!!!!!!! READ ME !!!!!!!!!!
  // I (viettrungluu) am in the process of refactoring |BrowserMain()|. If you
  // need to add something above this comment, read the documentation in
  // browser_main.h. If you need to add something below, please do the
  // following:
  //  - Figure out where you should add your code. Do NOT just pick a random
  //    location "which works".
  //  - Document the dependencies apart from compile-time-checkable ones. What
  //    must happen before your new code is executed? Does your new code need to
  //    run before something else? Are there performance reasons for executing
  //    your code at that point?
  //  - If you need to create a (persistent) object, heap allocate it and keep a
  //    |scoped_ptr| to it rather than allocating it on the stack. Otherwise
  //    I'll have to convert your code when I refactor.
  //  - Unless your new code is just a couple of lines, factor it out into a
  //    function with a well-defined purpose. Do NOT just add it inline in
  //    |BrowserMain()|.
  // Thanks!

  // TODO(viettrungluu): put the remainder into BrowserMainParts
  const CommandLine& parsed_command_line = parameters.command_line_;
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

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

  ProcessSingleton process_singleton(user_data_dir);

  bool is_first_run = FirstRun::IsChromeFirstRun() ||
      parsed_command_line.HasSwitch(switches::kFirstRun);

  scoped_ptr<BrowserProcessImpl> browser_process;
  if (parsed_command_line.HasSwitch(switches::kImport) ||
      parsed_command_line.HasSwitch(switches::kImportFromFile)) {
    // We use different BrowserProcess when importing so no GoogleURLTracker is
    // instantiated (as it makes a net::URLRequest and we don't have an IO
    // thread, see bug #1292702).
    browser_process.reset(new FirstRunBrowserProcess(parsed_command_line));
    is_first_run = false;
  } else {
    browser_process.reset(new BrowserProcessImpl(parsed_command_line));
  }

  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);

  // This forces the TabCloseableStateWatcher to be created and, on chromeos,
  // register for the notifications it needs to track the closeable state of
  // tabs.
  g_browser_process->tab_closeable_state_watcher();

  // The broker service initialization needs to run early because it will
  // initialize the sandbox broker, which requires the process to swap its
  // window station. During this time all the UI will be broken. This has to
  // run before threads and windows are created.
  InitializeBrokerServices(parameters, parsed_command_line);

  // Initialize histogram statistics gathering system.
  base::StatisticsRecorder statistics;

  PrefService* local_state = InitializeLocalState(parsed_command_line,
                                                  is_first_run);

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new GetLinuxDistroTask());

  if (IsMetricsReportingEnabled(local_state))
    InitCrashReporter();
#endif

  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (parameters.ui_task) {
    g_browser_process->SetApplicationLocale("en-US");
  } else {
    // Mac starts it earlier in |PreMainMessageLoopStart()| (because it is
    // needed when loading the MainMenu.nib and the language doesn't depend on
    // anything since it comes from Cocoa.
#if defined(OS_MACOSX)
    g_browser_process->SetApplicationLocale(l10n_util::GetLocaleOverride());
#else
    // On a POSIX OS other than ChromeOS, the parameter that is passed to the
    // method InitSharedInstance is ignored.
    const std::string locale =
        local_state->GetString(prefs::kApplicationLocale);
    const std::string loaded_locale =
        ResourceBundle::InitSharedInstance(locale);
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;
    g_browser_process->SetApplicationLocale(loaded_locale);

    FilePath resources_pack_path;
    PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
    ResourceBundle::AddDataPackToSharedInstance(resources_pack_path);
#endif  // !defined(OS_MACOSX)
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  // gdk_pixbuf_loader_write always fails on chromeos device.
  // Disabling for chromeos as well because chromeos is not using them.
  // http://crosbug.com/4996 .
  gtk_util::SetDefaultWindowIcon();
#endif

  std::string try_chrome =
      parsed_command_line.GetSwitchValueASCII(switches::kTryChromeAgain);
  if (!try_chrome.empty()) {
#if defined(OS_WIN)
    int try_chrome_int;
    base::StringToInt(try_chrome, &try_chrome_int);
    Upgrade::TryResult answer = Upgrade::ShowTryChromeDialog(try_chrome_int);
    if (answer == Upgrade::TD_NOT_NOW)
      return ResultCodes::NORMAL_EXIT_CANCEL;
    if (answer == Upgrade::TD_UNINSTALL_CHROME)
      return ResultCodes::NORMAL_EXIT_EXP2;
#else
    // We don't support retention experiments on Mac or Linux.
    return ResultCodes::NORMAL_EXIT;
#endif  // defined(OS_WIN)
  }

  BrowserInit browser_init;

  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  FirstRun::MasterPrefs master_prefs;
  bool first_run_ui_bypass = false;  // True to skip first run UI.
  if (is_first_run) {
    first_run_ui_bypass =
        !FirstRun::ProcessMasterPreferences(user_data_dir, &master_prefs);
    AddFirstRunNewTabs(&browser_init, master_prefs.new_tabs);

    // If we are running in App mode, we do not want to show the importer
    // (first run) UI.
    if (!first_run_ui_bypass &&
        (parsed_command_line.HasSwitch(switches::kApp) ||
         parsed_command_line.HasSwitch(switches::kAppId) ||
         parsed_command_line.HasSwitch(switches::kNoFirstRun)))
      first_run_ui_bypass = true;
  }

  // TODO(viettrungluu): why don't we run this earlier?
  if (!parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    WarnAboutMinimumSystemRequirements();

  InitializeNetworkOptions(parsed_command_line);

  // Initialize histogram synchronizer system. This is a singleton and is used
  // for posting tasks via NewRunnableMethod. Its deleted when it goes out of
  // scope. Even though NewRunnableMethod does AddRef and Release, the object
  // will not be deleted after the Task is executed.
  scoped_refptr<HistogramSynchronizer> histogram_synchronizer(
      new HistogramSynchronizer());

  // Initialize the prefs of the local state.
  browser::RegisterLocalState(local_state);

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

  CreateChildThreads(browser_process.get());

#if defined(OS_CHROMEOS)
  // Now that the file thread exists we can record our stats.
  chromeos::BootTimesLoader::Get()->RecordChromeMainStats();
  // Change the UI font if necessary. This has to be done after
  // InitSharedInstance() is called, as it depends on resource data.
  MaybeChangeUIFont();
#endif

  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

#if defined(OS_WIN)
  // On Windows, we use our startup as an opportunity to do upgrade/uninstall
  // tasks.  Those care whether the browser is already running.  On Linux/Mac,
  // upgrade/uninstall happen separately.
  bool already_running = Upgrade::IsBrowserAlreadyRunning();

  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (parsed_command_line.HasSwitch(switches::kUninstall))
    return DoUninstallTasks(already_running);
#endif

  if (parsed_command_line.HasSwitch(switches::kHideIcons) ||
      parsed_command_line.HasSwitch(switches::kShowIcons))
    return HandleIconsCommands(parsed_command_line);
  if (parsed_command_line.HasSwitch(switches::kMakeDefaultBrowser)) {
    return ShellIntegration::SetAsDefaultBrowser() ?
        ResultCodes::NORMAL_EXIT : ResultCodes::SHELL_INTEGRATION_FAILED;
  }

  // If the command line specifies --pack-extension, attempt the pack extension
  // startup action and exit.
  if (parsed_command_line.HasSwitch(switches::kPackExtension)) {
    ExtensionsStartupUtil extension_startup_util;
    if (extension_startup_util.PackExtension(parsed_command_line)) {
      return ResultCodes::NORMAL_EXIT;
    } else {
      return ResultCodes::PACK_EXTENSION_ERROR;
    }
  }

#if !defined(OS_MACOSX)
  // In environments other than Mac OS X we support import of settings
  // from other browsers. In case this process is a short-lived "import"
  // process that another browser runs just to import the settings, we
  // don't want to be checking for another browser process, by design.
  if (!(parsed_command_line.HasSwitch(switches::kImport) ||
        parsed_command_line.HasSwitch(switches::kImportFromFile))) {
#endif
    // When another process is running, use that process instead of starting a
    // new one. NotifyOtherProcess will currently give the other process up to
    // 20 seconds to respond. Note that this needs to be done before we attempt
    // to read the profile.
    switch (process_singleton.NotifyOtherProcessOrCreate()) {
      case ProcessSingleton::PROCESS_NONE:
        // No process already running, fall through to starting a new one.
        break;

      case ProcessSingleton::PROCESS_NOTIFIED:
#if defined(OS_POSIX) && !defined(OS_MACOSX)
        printf("%s\n", base::SysWideToNativeMB(UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_USED_EXISTING_BROWSER))).c_str());
#endif
        return ResultCodes::NORMAL_EXIT;

      case ProcessSingleton::PROFILE_IN_USE:
        return ResultCodes::PROFILE_IN_USE;

      case ProcessSingleton::LOCK_ERROR:
        LOG(ERROR) << "Failed to create a ProcessSingleton for your profile "
                      "directory. This means that running multiple instances "
                      "would start multiple browser processes rather than "
                      "opening a new window in the existing process. Aborting "
                      "now to avoid profile corruption.";
        return ResultCodes::PROFILE_IN_USE;

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
  // Stub out chromeos implementations.
  if (parsed_command_line.HasSwitch(switches::kStubCros))
    chromeos::CrosLibrary::Get()->GetTestApi()->SetUseStubImpl();

  // Initialize the screen locker now so that it can receive
  // LOGIN_USER_CHANGED notification from UserManager.
  chromeos::ScreenLocker::InitClass();

  // This forces the ProfileManager to be created and register for the
  // notification it needs to track the logged in user.
  g_browser_process->profile_manager()->GetDefaultProfile();

  // Allow access to file:// on ChromeOS for tests.
  if (parsed_command_line.HasSwitch(switches::kAllowFileAccess)) {
    net::URLRequest::AllowFileAccess();
  }

  // There are two use cases for kLoginUser:
  //   1) if passed in tandem with kLoginPassword, to drive a "StubLogin"
  //   2) if passed alone, to signal that the indicated user has already
  //      logged in and we should behave accordingly.
  // This handles case 2.
  if (parsed_command_line.HasSwitch(switches::kLoginUser) &&
      !parsed_command_line.HasSwitch(switches::kLoginPassword)) {
    std::string username =
        parsed_command_line.GetSwitchValueASCII(switches::kLoginUser);
    VLOG(1) << "Relaunching browser for user: " << username;
    chromeos::UserManager::Get()->UserLoggedIn(username);

    // Redirects Chrome logging to the user data dir.
    logging::RedirectChromeLogging(parsed_command_line);
  }
#endif

  // Modifies the current command line based on active experiments on
  // about:flags.
  Profile* profile = CreateProfile(parameters, user_data_dir);
  if (!profile)
    return ResultCodes::NORMAL_EXIT;

  // Post-profile init ---------------------------------------------------------

  PrefService* user_prefs = profile->GetPrefs();
  DCHECK(user_prefs);

  // Tests should be able to tune login manager before showing it.
  // Thus only show login manager in normal (non-testing) mode.
  if (!parameters.ui_task) {
    OptionallyRunChromeOSLoginManager(parsed_command_line);
  }

#if !defined(OS_MACOSX)
  // Importing other browser settings is done in a browser-like process
  // that exits when this task has finished.
  // TODO(port): Port the Mac's IPC-based implementation to other platforms to
  //             replace this implementation. http://crbug.com/22142
  if (parsed_command_line.HasSwitch(switches::kImport) ||
      parsed_command_line.HasSwitch(switches::kImportFromFile)) {
    return FirstRun::ImportNow(profile, parsed_command_line);
  }
#endif

#if defined(OS_WIN)
  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && Upgrade::DoUpgradeTasks(parsed_command_line))
    return ResultCodes::NORMAL_EXIT;
#endif

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check should only happen here, after all the checks above
  // (uninstall, resource bundle initialization, other chrome browser
  // processes etc).
  if (CheckMachineLevelInstall())
    return ResultCodes::MACHINE_LEVEL_INSTALL_EXISTS;

  // Create the TranslateManager singleton.
  TranslateManager::GetInstance();

#if defined(OS_MACOSX)
  if (!parsed_command_line.HasSwitch(switches::kNoFirstRun)) {
    // Disk image installation is sort of a first-run task, so it shares the
    // kNoFirstRun switch.
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      return ResultCodes::NORMAL_EXIT;
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
      FirstRun::AutoImport(profile,
                           master_prefs.homepage_defined,
                           master_prefs.do_import_items,
                           master_prefs.dont_import_items,
                           master_prefs.run_search_engine_experiment,
                           master_prefs.randomize_search_engine_experiment,
                           master_prefs.make_chrome_default,
                           &process_singleton);
#if defined(OS_POSIX)
      // On Windows, the download is tagged with enable/disable stats so there
      // is no need for this code.

      // If stats reporting was turned on by the first run dialog then toggle
      // the pref.
      if (GoogleUpdateSettings::GetCollectStatsConsent())
        local_state->SetBoolean(prefs::kMetricsReportingEnabled, true);
#endif  // OS_POSIX
    }  // if (!first_run_ui_bypass)

    Browser::SetNewHomePagePrefs(user_prefs);
  }

  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  PrepareRestartOnCrashEnviroment(parsed_command_line);

  // Initialize and maintain network predictor module, which handles DNS
  // pre-resolution, as well as TCP/IP connection pre-warming.
  // This also registers an observer to discard data when closing incognito
  // mode.
  bool preconnect_enabled = true;  // Default status (easy to change!).
  if (parsed_command_line.HasSwitch(switches::kDisablePreconnect))
    preconnect_enabled = false;
  else if (parsed_command_line.HasSwitch(switches::kEnablePreconnect))
    preconnect_enabled = true;
  chrome_browser_net::PredictorInit dns_prefetch(
      user_prefs,
      local_state,
      preconnect_enabled);

#if defined(OS_WIN)
  app::win::ScopedCOMInitializer com_initializer;

#if defined(GOOGLE_CHROME_BUILD)
  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  RLZTracker::InitRlzDelayed(is_first_run, master_prefs.ping_delay);
#endif
#endif

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);
  gfx::GfxModule::SetResourceProvider(chrome::GfxResourceProvider);

  // Register our global network handler for chrome:// and
  // chrome-extension:// URLs.
  RegisterURLRequestChromeJob();
  RegisterExtensionProtocols();
  RegisterMetadataURLRequestHandler();
  RegisterBlobURLRequestJobFactory();

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
  browser_process->google_url_tracker();
  browser_process->intranet_redirect_detector();

  // Do initialize the plug-in service (and related preferences).
  PluginService::InitGlobalInstance(profile);

  // Prepare for memory caching of SDCH dictionaries.
  // Perform A/B test to measure global impact of SDCH support.
  // Set up a field trial to see what disabling SDCH does to latency of page
  // layout globally.
  base::FieldTrial::Probability kSDCH_DIVISOR = 1000;
  base::FieldTrial::Probability kSDCH_DISABLE_PROBABILITY = 1;  // 0.1% prob.
  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> sdch_trial(
      new base::FieldTrial("GlobalSdch", kSDCH_DIVISOR, "global_enable_sdch",
          2011, 6, 30));
  int sdch_enabled = sdch_trial->kDefaultGroupNumber;

  // Use default of "" so that all domains are supported.
  std::string sdch_supported_domain("");
  if (parsed_command_line.HasSwitch(switches::kSdchFilter)) {
    sdch_supported_domain =
        parsed_command_line.GetSwitchValueASCII(switches::kSdchFilter);
  } else {
    sdch_trial->AppendGroup("global_disable_sdch",
                            kSDCH_DISABLE_PROBABILITY);
    if (sdch_enabled != sdch_trial->group())
      sdch_supported_domain = "never_enabled_sdch_for_any_domain";
  }

  SdchManager sdch_manager;  // Singleton database.
  sdch_manager.set_sdch_fetcher(new SdchDictionaryFetcher);
  sdch_manager.EnableSdchSupport(sdch_supported_domain);

  MetricsService* metrics = InitializeMetrics(parsed_command_line, local_state);
  InstallJankometer(parsed_command_line);

#if defined(OS_WIN) && !defined(GOOGLE_CHROME_BUILD)
  if (parsed_command_line.HasSwitch(switches::kDebugPrint)) {
    FilePath path =
        parsed_command_line.GetSwitchValuePath(switches::kDebugPrint);
    printing::PrintedDocument::set_debug_dump_path(path);
  }
#endif

  HandleTestParameters(parsed_command_line);
  RecordBreakpadStatusUMA(metrics);
  about_flags::RecordUMAStatistics(user_prefs);

  // Stat the directory with the inspector's files so that we can know if we
  // should display the entry in the context menu or not.
  browser_process->CheckForInspectorFiles();

#if defined(OS_CHROMEOS)
  metrics->StartExternalMetrics();
#endif

  // Initialize extension event routers. Note that on Chrome OS, this will
  // not succeed if the user has not logged in yet, in which case the
  // event routers are initialized in LoginUtilsImpl::CompleteLogin instead.
  if (profile->GetExtensionService()) {
    // This will initialize bookmarks. Call it after bookmark import is done.
    // See issue 40144.
    profile->GetExtensionService()->InitEventRouters();
  }

  // The extension service may be available at this point. If the command line
  // specifies --uninstall-extension, attempt the uninstall extension startup
  // action.
  if (parsed_command_line.HasSwitch(switches::kUninstallExtension)) {
    ExtensionsStartupUtil ext_startup_util;
    if (ext_startup_util.UninstallExtension(parsed_command_line, profile)) {
      return ResultCodes::NORMAL_EXIT;
    } else {
      return ResultCodes::UNINSTALL_EXTENSION_ERROR;
    }
  }

#if defined(OS_WIN)
  // We check this here because if the profile is OTR (chromeos possibility)
  // it won't still be accessible after browser is destroyed.
  bool record_search_engine = is_first_run && !profile->IsOffTheRecord();
#endif

  // ChildProcess:: is a misnomer unless you consider context.  Use
  // of --wait-for-debugger only makes sense when Chrome itself is a
  // child process (e.g. when launched by PyAuto).
  if (parsed_command_line.HasSwitch(switches::kWaitForDebugger)) {
    ChildProcess::WaitForDebugger(L"Browser");
  }

  // If remoting or cloud print proxy is enabled and setup has been completed
  // we start the service process here.
  // The prerequisite for running the service process is that we have IO, UI
  // and PROCESS_LAUNCHER threads up and running.
  // TODO(hclam): Need to check for cloud print proxy too.
  if (parsed_command_line.HasSwitch(switches::kEnableRemoting)) {
    if (user_prefs->GetBoolean(prefs::kRemotingHasSetupCompleted)) {
      ServiceProcessControl* control =
          ServiceProcessControlManager::GetInstance()->GetProcessControl(
              profile);
       control->Launch(NULL, NULL);
    }
  }

#if defined(OS_CHROMEOS)
  // Run the Out of Memory priority manager while in this scope.  Wait
  // until here to start so that we give the most amount of time for
  // the other services to start up before we start adjusting the oom
  // priority.  In reality, it doesn't matter much where in this scope
  // this is started, but it must be started in this scope so it will
  // also be terminated when this scope exits.
  scoped_ptr<browser::OomPriorityManager> oom_priority_manager(
      new browser::OomPriorityManager);
#endif

  // Create the instance of the cloud print proxy service so that it can launch
  // the service process if needed. This is needed because the service process
  // might have shutdown because an update was available.
  profile->GetCloudPrintProxyService();

  int result_code = ResultCodes::NORMAL_EXIT;
  if (parameters.ui_task) {
    // We are in test mode. Run one task and enter the main message loop.
    if (pool)
      pool->Recycle();
    parameters.ui_task->Run();
    delete parameters.ui_task;
  } else {
    // We are in regular browser boot sequence. Open initial stabs and enter
    // the main message loop.
    if (browser_init.Start(parsed_command_line, FilePath(), profile,
                           &result_code)) {
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
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
      Upgrade::SaveLastModifiedTimeOfExe();
#endif

      // Record now as the last successful chrome start.
      GoogleUpdateSettings::SetLastRunTime();
      // Call Recycle() here as late as possible, before going into the loop
      // because Start() will add things to it while creating the main window.
      if (pool)
        pool->Recycle();
      RunUIMessageLoop(browser_process.get());
    }
  }

#if defined(OS_WIN)
  // If it's the first run, log the search engine chosen.  We wait until
  // shutdown because otherwise we can't be sure the user has finished
  // selecting a search engine through the dialog reached from the first run
  // bubble link.
  if (FirstRun::InSearchExperimentLocale() && record_search_engine) {
    const TemplateURL* default_search_engine =
        profile->GetTemplateURLModel()->GetDefaultSearchProvider();
    // The default engine can be NULL if the administrator has disabled
    // default search.
    SearchEngineType search_engine_type =
        default_search_engine ? default_search_engine->search_engine_type() :
                                SEARCH_ENGINE_OTHER;
    // Record the search engine chosen.
    if (master_prefs.run_search_engine_experiment) {
      UMA_HISTOGRAM_ENUMERATION(
          "Chrome.SearchSelectExperiment",
          search_engine_type,
          SEARCH_ENGINE_MAX);
      // If the selection has been randomized, also record the winner by slot.
      if (master_prefs.randomize_search_engine_experiment) {
        size_t engine_pos = profile->GetTemplateURLModel()->
            GetSearchEngineDialogSlot();
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

  chrome_browser_net_websocket_experiment::WebSocketExperimentRunner::Stop();

  process_singleton.Cleanup();

  metrics->Stop();

  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  ignore_result(browser_process.release());
  browser_shutdown::Shutdown();

#if defined(OS_CHROMEOS)
  // To be precise, logout (browser shutdown) is not yet done, but the
  // remaining work is negligible, hence we say LogoutDone here.
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutDone",
                                                        false);
  chromeos::BootTimesLoader::Get()->WriteLogoutTimes();
#endif
  TRACE_EVENT_END("BrowserMain", 0, 0);
  return result_code;
}
