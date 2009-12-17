// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#include <algorithm>

#include "app/hi_res_timer_manager.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/system_monitor.h"
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/lazy_instance.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extension_protocols.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/metadata_url_request.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/histogram_synchronizer.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_base.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "base/linux_util.h"
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_LINUX)
#include "chrome/common/gtk_util.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "app/win_util.h"
#include "base/registry.h"
#include "base/win_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/views/user_data_dir_dialog.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/version.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/winsock_init.h"
#include "net/http/http_network_layer.h"
#include "printing/printed_document.h"
#include "sandbox/src/sandbox.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/chrome_views_delegate.h"
#include "views/focus/accelerator_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/views/browser_dialogs.h"
#endif

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
    exit(!chromeos::CrosLibrary::EnsureLoaded());
#endif
}

void RunUIMessageLoop(BrowserProcess* browser_process) {
#if defined(TOOLKIT_VIEWS)
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
#elif defined(OS_LINUX)
  MessageLoopForUI::current()->Run(NULL);
#elif defined(OS_POSIX)
  MessageLoopForUI::current()->Run();
#endif
}

#if defined(OS_POSIX)
// See comment in BrowserMain, where sigaction is called.
void SIGCHLDHandler(int signal) {
}

// Common code between SIG{HUP, INT, TERM}Handler.
void GracefulShutdownHandler(int signal, const int expected_signal) {
  DCHECK_EQ(signal, expected_signal);
  LOG(INFO) << "Addressing signal " << expected_signal << " on "
            << PlatformThread::CurrentId();

  bool posted = ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(BrowserList::CloseAllBrowsersAndExit));

  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(expected_signal, &action, NULL) == 0);

  if (posted) {
    LOG(INFO) << "Posted task to UI thread; resetting signal "
              << expected_signal << " handler";
  } else {
    // Without a UI thread to post the exit task to, there aren't many
    // options.  Raise the signal again.  The default handler will pick it up
    // and cause an ungraceful exit.
    LOG(WARNING) << "No UI thread, exiting ungracefully";
    kill(getpid(), signal);

    // The signal may be handled on another thread.  Give that a chance to
    // happen.
    sleep(3);

    // We really should be dead by now.  For whatever reason, we're not. Exit
    // immediately, with the exit status set to the signal number with bit 8
    // set.  On the systems that we care about, this exit status is what is
    // normally used to indicate an exit by this signal's default handler.
    // This mechanism isn't a de jure standard, but even in the worst case, it
    // should at least result in an immediate exit.
    LOG(WARNING) << "Still here, exiting really ungracefully";
    _exit(signal | (1 << 7));
  }
}

// See comment in BrowserMain, where sigaction is called.
void SIGHUPHandler(int signal) {
  GracefulShutdownHandler(signal, SIGHUP);
}

// See comment in BrowserMain, where sigaction is called.
void SIGINTHandler(int signal) {
  GracefulShutdownHandler(signal, SIGINT);
}

// See comment in BrowserMain, where sigaction is called.
void SIGTERMHandler(int signal) {
  GracefulShutdownHandler(signal, SIGTERM);
}

// Sets the file descriptor soft limit to |max_descriptors| or the OS hard
// limit, whichever is lower.
void SetFileDescriptorLimit(unsigned int max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    unsigned int new_limit = max_descriptors;
    if (limits.rlim_max > 0 && limits.rlim_max < max_descriptors) {
      new_limit = limits.rlim_max;
    }
    limits.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      PLOG(INFO) << "Failed to set file descriptor limit";
    }
  } else {
    PLOG(INFO) << "Failed to get file descriptor limit";
  }
}
#endif

#if defined(OS_WIN)
void AddFirstRunNewTabs(BrowserInit* browser_init,
                        const std::vector<std::wstring>& new_tabs) {
  std::vector<std::wstring>::const_iterator it = new_tabs.begin();
  while (it != new_tabs.end()) {
    GURL url(*it);
    if (url.is_valid())
      browser_init->AddFirstRunTab(url);
    ++it;
  }
}
#else
// TODO(cpu): implement first run experience for other platforms.
void AddFirstRunNewTabs(BrowserInit* browser_init,
                        const std::vector<std::wstring>& new_tabs) {
}
#endif

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
}  // namespace

// Main routine for running as the Browser process.
int BrowserMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;
  base::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

  // WARNING: If we get a WM_ENDSESSION objects created on the stack here
  // are NOT deleted. If you need something to run during WM_ENDSESSION add it
  // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

  // TODO(beng, brettw): someday, break this out into sub functions with well
  //                     defined roles (e.g. pre/post-profile startup, etc).

#ifdef TRACK_ALL_TASK_OBJECTS
  // Start tracking the creation and deletion of Task instance.
  // This construction MUST be done before main_message_loop, so that it is
  // destroyed after the main_message_loop.
  tracked_objects::AutoTracking tracking_objects;
#endif

#if defined(OS_POSIX)
  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT - when the user terminates the browser via Ctrl+C.
  // If the browser process is being debugged, GDB will catch the SIGINT first.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);

  const std::wstring fd_limit_string =
      parsed_command_line.GetSwitchValue(switches::kFileDescriptorLimit);
  int fd_limit = 0;
  if (!fd_limit_string.empty()) {
    StringToInt(WideToUTF16Hack(fd_limit_string), &fd_limit);
  }
#if defined(OS_MACOSX)
  // We use quite a few file descriptors for our IPC, and the default limit on
  // the Mac is low (256), so bump it up if there is no explicit override.
  if (fd_limit == 0) {
    fd_limit = 1024;
  }
#endif  // OS_MACOSX
  if (fd_limit > 0) {
    SetFileDescriptorLimit(fd_limit);
  }
#endif  // OS_POSIX

#if defined(OS_WIN)
  // Initialize Winsock.
  net::EnsureWinsockInit();
#endif  // defined(OS_WIN)

  // Do platform-specific things (such as finishing initializing Cocoa)
  // prior to instantiating the message loop. This could be turned into a
  // broadcast notification.
  Platform::WillInitializeMainMessageLoop(parameters);

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  // Initialize statistical testing infrastructure.
  FieldTrialList field_trial;

  std::wstring app_name = chrome::kBrowserAppName;
  std::string thread_name_string = WideToASCII(app_name + L"_BrowserMain");

  const char* thread_name = thread_name_string.c_str();
  PlatformThread::SetName(thread_name);
  main_message_loop.set_thread_name(thread_name);

  // Register the main thread by instantiating it, but don't call any methods.
  ChromeThread main_thread(ChromeThread::UI, MessageLoop::current());

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
  if (parsed_command_line.HasSwitch(switches::kImport)) {
    // We use different BrowserProcess when importing so no GoogleURLTracker is
    // instantiated (as it makes a URLRequest and we don't have an IO thread,
    // see bug #1292702).
    browser_process.reset(new FirstRunBrowserProcess(parsed_command_line));
    is_first_run = false;
  } else {
    browser_process.reset(new BrowserProcessImpl(parsed_command_line));
  }

  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new GetLinuxDistroTask());
  InitCrashReporter();
#endif

#if defined(OS_WIN)
  // IMPORTANT: This piece of code needs to run as early as possible in the
  // process because it will initialize the sandbox broker, which requires the
  // process to swap its window station. During this time all the UI will be
  // broken. This has to run before threads and windows are created.
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services) {
    browser_process->InitBrokerServices(broker_services);
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

  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = file_util::PathExists(local_state_path);

  // Load local state.  This includes the application locale so we know which
  // locale dll to load.
  PrefService* local_state = browser_process->local_state();
  DCHECK(local_state);

  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  local_state->RegisterStringPref(prefs::kApplicationLocale, L"");
  local_state->RegisterBooleanPref(prefs::kMetricsReportingEnabled,
      GoogleUpdateSettings::GetCollectStatsConsent());

#if defined(TOOLKIT_GTK)
  // It is important for this to happen before the first run dialog, as it
  // styles the dialog as well.
  gtk_util::InitRCStyles();
#elif defined(TOOLKIT_VIEWS)
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;
#endif


  if (is_first_run) {
#if defined(OS_WIN)
    // During first run we read the google_update registry key to find what
    // language the user selected when downloading the installer. This
    // becomes our default language in the prefs.
    // Other platforms obey the system locale.
    std::wstring install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang))
      local_state->SetString(prefs::kApplicationLocale, install_lang);
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
    PrefService parent_local_state(parent_profile);
    parent_local_state.RegisterStringPref(prefs::kApplicationLocale,
                                          std::wstring());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state.GetString(prefs::kApplicationLocale));
  }

  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (parameters.ui_task) {
    g_browser_process->set_application_locale("en-US");
  } else {
    // Mac starts it earlier in Platform::WillInitializeMainMessageLoop (because
    // it is needed when loading the MainMenu.nib and the language doesn't depend
    // on anything since it comes from Cocoa.
#if defined(OS_MACOSX)
    g_browser_process->set_application_locale(l10n_util::GetLocaleOverride());
#else
    std::string app_locale = ResourceBundle::InitSharedInstance(
        local_state->GetString(prefs::kApplicationLocale));
    g_browser_process->set_application_locale(app_locale);
#endif  // !defined(OS_MACOSX)
  }

#if defined(OS_CHROMEOS)
  if (parsed_command_line.HasSwitch(switches::kLoginManager)) {
    browser::ShowLoginManager();
  }
#endif  // OS_CHROMEOS

#if defined(OS_LINUX)
  gtk_util::SetDefaultWindowIcon();
#endif

#if defined(OS_WIN)
  // This is experimental code. See first_run_win.cc for more info.
  std::string try_chrome =
      parsed_command_line.GetSwitchValueASCII(switches::kTryChromeAgain);
  if (!try_chrome.empty()) {
    Upgrade::TryResult answer =
        Upgrade::ShowTryChromeDialog(StringToInt(try_chrome));
    if (answer == Upgrade::TD_NOT_NOW)
      return ResultCodes::NORMAL_EXIT_EXP1;
    if (answer == Upgrade::TD_UNINSTALL_CHROME)
      return ResultCodes::NORMAL_EXIT_EXP2;
  }
#endif  // OS_WIN

  BrowserInit browser_init;

#if defined(OS_WIN)
  int rlz_ping_delay = 0;
#endif
  bool homepage_defined = false;
  int import_items = 0;
  int dont_import_items = 0;
  bool first_run_ui_bypass = false;
  if (is_first_run) {
#if defined(OS_WIN)
    // On first run, we need to process the master preferences before the
    // browser's profile_manager object is created, but after ResourceBundle
    // is initialized.
    std::vector<std::wstring> first_run_tabs;
    first_run_ui_bypass = !FirstRun::ProcessMasterPreferences(user_data_dir,
        FilePath(), &first_run_tabs, &rlz_ping_delay, &homepage_defined,
        &import_items, &dont_import_items);
    // The master prefs might specify a set of urls to display.
    if (first_run_tabs.size())
      AddFirstRunNewTabs(&browser_init, first_run_tabs);
#endif  // OS_WIN

    // If we are running in App mode, we do not want to show the importer
    // (first run) UI.
    if (!first_run_ui_bypass &&
        (parsed_command_line.HasSwitch(switches::kApp) ||
         parsed_command_line.HasSwitch(switches::kNoFirstRun))) {
      first_run_ui_bypass = true;
    }
  }

  if (!parsed_command_line.HasSwitch(switches::kNoErrorDialogs)) {
    // Display a warning if the user is running windows 2000.
    // TODO(port): We should probably change this to a "check for minimum
    // requirements" function, implemented by each platform.
    CheckForWin2000();
  }

  if (parsed_command_line.HasSwitch(switches::kEnableFileCookies)) {
    // Enable cookie storage for file:// URLs.  Must do this before the first
    // Profile (and therefore the first CookieMonster) is created.
    net::CookieMonster::EnableFileScheme();
  }

  if (parsed_command_line.HasSwitch(switches::kFixedHttpPort)) {
    std::string http_port_str =
        parsed_command_line.GetSwitchValueASCII(switches::kFixedHttpPort);
    int http_port = StringToInt(http_port_str);
    net::HttpNetworkSession::set_fixed_http_port(http_port);
  }

  if (parsed_command_line.HasSwitch(switches::kFixedHttpsPort)) {
    std::string https_port_str =
        parsed_command_line.GetSwitchValueASCII(switches::kFixedHttpsPort);
    int https_port = StringToInt(https_port_str);
    net::HttpNetworkSession::set_fixed_https_port(https_port);
  }

  // Initialize histogram statistics gathering system.
  StatisticsRecorder statistics;

  // Initialize histogram synchronizer system. This is a singleton and is used
  // for posting tasks via NewRunnableMethod. Its deleted when it goes out of
  // scope. Even though NewRunnableMethod does AddRef and Release, the object
  // will not be deleted after the Task is executed.
  scoped_refptr<HistogramSynchronizer> histogram_synchronizer =
      new HistogramSynchronizer();

  // Initialize the shared instance of user data manager.
  scoped_ptr<UserDataManager> user_data_manager(UserDataManager::Create());

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

  // Create the child threads.  We need to do this since ChromeThread::PostTask
  // silently deletes a posted task if the target message loop isn't created.
  browser_process->db_thread();
  browser_process->file_thread();
  browser_process->process_launcher_thread();
  browser_process->io_thread();

#if defined(OS_WIN)
  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

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
      parsed_command_line.HasSwitch(switches::kShowIcons)) {
    return HandleIconsCommands(parsed_command_line);
  } else if (parsed_command_line.HasSwitch(switches::kMakeDefaultBrowser)) {
    if (ShellIntegration::SetAsDefaultBrowser()) {
      return ResultCodes::NORMAL_EXIT;
    } else {
      return ResultCodes::SHELL_INTEGRATION_FAILED;
    }
  }

  // Try to create/load the profile.
  ProfileManager* profile_manager = browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);

#if defined(OS_WIN)
  if (!profile) {
    // Ideally, we should be able to run w/o access to disk.  For now, we
    // prompt the user to pick a different user-data-dir and restart chrome
    // with the new dir.
    // http://code.google.com/p/chromium/issues/detail?id=11510
    user_data_dir = UserDataDirDialog::RunUserDataDirDialog(user_data_dir);
    if (!parameters.ui_task && browser_shutdown::delete_resources_on_shutdown) {
      // Only delete the resources if we're not running tests. If we're running
      // tests the resources need to be reused as many places in the UI cache
      // SkBitmaps from the ResourceBundle.
      ResourceBundle::CleanupSharedInstance();
    }

    if (!user_data_dir.empty()) {
      // Because of the way CommandLine parses, it's sufficient to append a new
      // --user-data-dir switch.  The last flag of the same name wins.
      // TODO(tc): It would be nice to remove the flag we don't want, but that
      // sounds risky if we parse differently than CommandLineToArgvW.
      CommandLine new_command_line = parsed_command_line;
      new_command_line.AppendSwitchWithValue(switches::kUserDataDir,
                                             user_data_dir.ToWStringHack());
      base::LaunchApp(new_command_line, false, false, NULL);
    }

    return ResultCodes::NORMAL_EXIT;
  }
#else
  // TODO(port): fix this.  See comments near the definition of
  // user_data_dir.  It is better to CHECK-fail here than it is to
  // silently exit because of missing code in the above test.
  CHECK(profile) << "Cannot get default profile.";
#endif

  PrefService* user_prefs = profile->GetPrefs();
  DCHECK(user_prefs);

  // Importing other browser settings is done in a browser-like process
  // that exits when this task has finished.
#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kImport))
    return FirstRun::ImportNow(profile, parsed_command_line);
#endif

  // When another process is running, use it instead of starting us.
  switch (process_singleton.NotifyOtherProcess()) {
    case ProcessSingleton::PROCESS_NONE:
      // No process already running, fall through to starting a new one.
      break;

    case ProcessSingleton::PROCESS_NOTIFIED:
#if defined(OS_LINUX)
      printf("%s\n", base::SysWideToNativeMB(
                 l10n_util::GetString(IDS_USED_EXISTING_BROWSER)).c_str());
#endif
      return ResultCodes::NORMAL_EXIT;

    case ProcessSingleton::PROFILE_IN_USE:
      return ResultCodes::PROFILE_IN_USE;

    default:
      NOTREACHED();
  }

#if defined(OS_WIN)
  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && DoUpgradeTasks(parsed_command_line)) {
    return ResultCodes::NORMAL_EXIT;
  }
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

  process_singleton.Create();

  // Show the First Run UI if this is the first time Chrome has been run on
  // this computer, or we're being compelled to do so by a command line flag.
  // Note that this be done _after_ the PrefService is initialized and all
  // preferences are registered, since some of the code that the importer
  // touches reads preferences.
  if (is_first_run) {
    if (!first_run_ui_bypass) {
      if (!OpenFirstRunDialog(profile, homepage_defined, import_items,
                              dont_import_items, &process_singleton)) {
        // The user cancelled the first run dialog box, we should exit Chrome.
        return ResultCodes::NORMAL_EXIT;
      }
#if defined(OS_POSIX)
      // On Windows, the download is tagged with enable/disable stats so there
      // is no need for this code.

      // If stats reporting was turned on by the first run dialog then toggle
      // the pref.
      if (GoogleUpdateSettings::GetCollectStatsConsent())
        local_state->SetBoolean(prefs::kMetricsReportingEnabled, true);
#endif  // OS_POSIX
    }
    Browser::SetNewHomePagePrefs(user_prefs);
  }

  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  PrepareRestartOnCrashEnviroment(parsed_command_line);

  // Initialize and maintain DNS prefetcher module.
  chrome_browser_net::DnsPrefetcherInit dns_prefetch(user_prefs, local_state);

  scoped_refptr<FieldTrial> socket_late_binding_trial =
      new FieldTrial("SocketLateBinding", 100);
  socket_late_binding_trial->AppendGroup("_disable_late_binding", 50);
  const int late_binding_group =
      socket_late_binding_trial->AppendGroup("_enable_late_binding", 50);
  if (socket_late_binding_trial->group() == late_binding_group)
    net::EnableLateBindingOfSockets(true);

#if defined(OS_WIN)
  // Init common control sex.
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&config);

  win_util::ScopedCOMInitializer com_initializer;

  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  RLZTracker::InitRlzDelayed(base::DIR_MODULE, is_first_run, rlz_ping_delay);
#endif

  // Configure the network module so it has access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

  // Register our global network handler for chrome:// and
  // chrome-extension:// URLs.
  RegisterURLRequestChromeJob();
  RegisterExtensionProtocols();
  RegisterMetadataURLRequestHandler();

  // In unittest mode, this will do nothing.  In normal mode, this will create
  // the global GoogleURLTracker instance, which will promptly go to sleep for
  // five seconds (to avoid slowing startup), and wake up afterwards to see if
  // it should do anything else.  If we don't cause this creation now, it won't
  // happen until someone else asks for the tracker, at which point we may no
  // longer want to sleep for five seconds.
  //
  // A simpler way of doing all this would be to have some function which could
  // give the time elapsed since startup, and simply have the tracker check that
  // when asked to initialize itself, but this doesn't seem to exist.
  //
  // This can't be created in the BrowserProcessImpl constructor because it
  // needs to read prefs that get set after that runs.
  browser_process->google_url_tracker();

  // Have Chrome plugins write their data to the profile directory.
  PluginService::GetInstance()->SetChromePluginDataDir(profile->GetPath());

  // Prepare for memory caching of SDCH dictionaries.
  // Perform A/B test to measure global impact of SDCH support.
  // Set up a field trial to see what disabling SDCH does to latency of page
  // layout globally.
  FieldTrial::Probability kSDCH_DIVISOR = 100;
  FieldTrial::Probability kSDCH_PROBABILITY_PER_GROUP = 50;  // 50% probability.
  scoped_refptr<FieldTrial> sdch_trial =
      new FieldTrial("GlobalSdch", kSDCH_DIVISOR);

  // Use default of "" so that all domains are supported.
  std::string sdch_supported_domain("");
  if (parsed_command_line.HasSwitch(switches::kSdchFilter)) {
    sdch_supported_domain =
        parsed_command_line.GetSwitchValueASCII(switches::kSdchFilter);
  } else {
    sdch_trial->AppendGroup("_global_disable_sdch",
                            kSDCH_PROBABILITY_PER_GROUP);
    int sdch_enabled = sdch_trial->AppendGroup("_global_enable_sdch",
                                               kSDCH_PROBABILITY_PER_GROUP);
    if (sdch_enabled != sdch_trial->group())
      sdch_supported_domain = "never_enabled_sdch_for_any_domain";
  }

  SdchManager sdch_manager;  // Singleton database.
  sdch_manager.set_sdch_fetcher(new SdchDictionaryFetcher);
  sdch_manager.EnableSdchSupport(sdch_supported_domain);

  MetricsService* metrics = NULL;
  if (!parsed_command_line.HasSwitch(switches::kDisableMetrics)) {
    bool enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);
    bool record_only =
        parsed_command_line.HasSwitch(switches::kMetricsRecordingOnly);

#if !defined(GOOGLE_CHROME_BUILD)
    // Disable user metrics completely for non-Google Chrome builds.
    enabled = false;
#endif

    metrics = browser_process->metrics_service();
    DCHECK(metrics);

    // If we're testing then we don't care what the user preference is, we turn
    // on recording, but not reporting, otherwise tests fail.
    if (record_only) {
      metrics->StartRecordingOnly();
    } else {
      // If the user permits metrics reporting with the checkbox in the
      // prefs, we turn on recording.
      metrics->SetUserPermitsUpload(enabled);
      if (enabled)
        metrics->Start();
    }
    chrome_browser_net_websocket_experiment::WebSocketExperimentRunner::Start();
  }

  InstallJankometer(parsed_command_line);

#if defined(OS_WIN) && !defined(GOOGLE_CHROME_BUILD)
  if (parsed_command_line.HasSwitch(switches::kDebugPrint)) {
    printing::PrintedDocument::set_debug_dump_path(
        parsed_command_line.GetSwitchValue(switches::kDebugPrint));
  }
#endif

  HandleTestParameters(parsed_command_line);
  Platform::RecordBreakpadStatusUMA(metrics);

#if defined(OS_CHROMEOS)
  chromeos::ExternalCookieHandler::GetCookies(parsed_command_line, profile);
#endif

  // Stat the directory with the inspector's files so that we can know if we
  // should display the entry in the context menu or not.
  browser_process->CheckForInspectorFiles();

#if defined(OS_CHROMEOS)
  metrics->StartExternalMetrics(profile);
#endif

  int result_code = ResultCodes::NORMAL_EXIT;
  if (parameters.ui_task) {
    // We are in test mode. Run one task and enter the main message loop.
    if (pool)
      pool->Recycle();
    MessageLoopForUI::current()->PostTask(FROM_HERE, parameters.ui_task);
    RunUIMessageLoop(browser_process.get());
  } else {
    // We are in regular browser boot sequence. Open initial stabs and enter
    // the main message loop.
    if (browser_init.Start(parsed_command_line, std::wstring(), profile,
                           &result_code)) {
      // Call Recycle() here as late as possible, before going into the loop
      // because Start() will add things to it while creating the main window.
      if (pool)
        pool->Recycle();
      RunUIMessageLoop(browser_process.get());
    }
  }
  chrome_browser_net_websocket_experiment::WebSocketExperimentRunner::Stop();

  process_singleton.Cleanup();

  if (metrics)
    metrics->Stop();

  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  browser_process.release();
  browser_shutdown::Shutdown();

  return result_code;
}
