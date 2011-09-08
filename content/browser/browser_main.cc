// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main.h"

#include "base/allocator/allocator_shim.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread.h"
#include "content/browser/content_browser_client.h"
#include "content/common/content_switches.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/main_function_params.h"
#include "content/common/result_codes.h"
#include "content/common/sandbox_policy.h"
#include "crypto/nss_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_client_socket.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <ole2.h>
#include <shellapi.h>

#include "base/win/scoped_com_initializer.h"
#include "net/base/winsock_init.h"
#include "sandbox/src/sandbox.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <dbus/dbus-glib.h>
#include <sys/stat.h>

#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

namespace {

#if defined(OS_WIN)
// Windows-specific initialization code for the sandbox broker services.
void InitializeBrokerServices(const MainFunctionParams& parameters,
                              const CommandLine& parsed_command_line) {
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
}
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
void SetupSandbox(const CommandLine& parsed_command_line) {
  // TODO(evanm): move this into SandboxWrapper; I'm just trying to move this
  // code en masse out of chrome_main for now.
  const char* sandbox_binary = NULL;
  struct stat st;

  // In Chromium branded builds, developers can set an environment variable to
  // use the development sandbox. See
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
  if (stat("/proc/self/exe", &st) == 0 && st.st_uid == getuid())
    sandbox_binary = getenv("CHROME_DEVEL_SANDBOX");

#if defined(LINUX_SANDBOX_PATH)
  if (!sandbox_binary)
    sandbox_binary = LINUX_SANDBOX_PATH;
#endif

  std::string sandbox_cmd;
  if (sandbox_binary && !parsed_command_line.HasSwitch(switches::kNoSandbox))
    sandbox_cmd = sandbox_binary;

  // Tickle the sandbox host and zygote host so they fork now.
  RenderSandboxHostLinux* shost = RenderSandboxHostLinux::GetInstance();
  shost->Init(sandbox_cmd);
  ZygoteHost* zhost = ZygoteHost::GetInstance();
  zhost->Init(sandbox_cmd);
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
  } else if (strstr(message, "Theme file for default has no") ||
             strstr(message, "Theme directory") ||
             strstr(message, "theme pixmap")) {
    LOG(ERROR) << "GTK theme error: " << message;
  } else if (strstr(message, "gtk_drag_dest_leave: assertion")) {
    LOG(ERROR) << "Drag destination deleted: http://crbug.com/18557";
  } else if (strstr(message, "Out of memory") &&
             strstr(log_domain, "<unknown>")) {
    LOG(ERROR) << "DBus call timeout or out of memory: "
               << "http://crosbug.com/15496";
  } else {
    LOG(DFATAL) << log_domain << ": " << message;
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

}  // namespace

namespace content {

BrowserMainParts::BrowserMainParts(const MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line_),
      result_code_(content::RESULT_CODE_NORMAL_EXIT) {
}

BrowserMainParts::~BrowserMainParts() {
}

void BrowserMainParts::EarlyInitialization() {
  PreEarlyInitialization();

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif

  // Use NSS for SSL by default.
  // The default client socket factory uses NSS for SSL by default on
  // Windows and Mac.
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (parsed_command_line().HasSwitch(switches::kUseSystemSSL)) {
    net::ClientSocketFactory::UseSystemSSL();
  } else {
#elif defined(USE_NSS)
  if (true) {
#else
  if (false) {
#endif
    // We want to be sure to init NSPR on the main thread.
    crypto::EnsureNSPRInit();
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  SetupSandbox(parsed_command_line());
#endif

  if (parsed_command_line().HasSwitch(switches::kEnableBenchmarking))
    base::FieldTrial::EnableBenchmarking();
  if (parsed_command_line().HasSwitch(switches::kDisableSSLFalseStart))
    net::SSLConfigService::DisableFalseStart();
  if (parsed_command_line().HasSwitch(switches::kEnableSSLCachedInfo))
    net::SSLConfigService::EnableCachedInfo();
  if (parsed_command_line().HasSwitch(switches::kEnableOriginBoundCerts))
    net::SSLConfigService::EnableOriginBoundCerts();
  if (parsed_command_line().HasSwitch(
          switches::kEnableDNSCertProvenanceChecking)) {
    net::SSLConfigService::EnableDNSCertProvenanceChecking();
  }

  // TODO(abarth): Should this move to InitializeNetworkOptions?  This doesn't
  // seem dependent on SSL initialization().
  if (parsed_command_line().HasSwitch(switches::kEnableTcpFastOpen))
    net::set_tcp_fastopen_enabled(true);

  PostEarlyInitialization();
}

void BrowserMainParts::MainMessageLoopStart() {
  PreMainMessageLoopStart();

#if defined(OS_WIN)
  OleInitialize(NULL);

  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (!parameters().ui_task) {
    // Override the configured locale with the user's preferred UI language.
    l10n_util::OverrideLocaleWithUILanguageList();
  }
#endif

  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

  // TODO(viettrungluu): should these really go before setting the thread name?
  system_monitor_.reset(new base::SystemMonitor);
  hi_res_timer_manager_.reset(new HighResolutionTimerManager);

  InitializeMainThread();

  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());

  PostMainMessageLoopStart();
}

void BrowserMainParts::RunMainMessageLoopParts() {
  PreMainMessageLoopRun();

  TRACE_EVENT_BEGIN_ETW("BrowserMain:MESSAGE_LOOP", 0, "");
  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  base::ThreadRestrictions::SetIOAllowed(false);
  MainMessageLoopRun();
  TRACE_EVENT_END_ETW("BrowserMain:MESSAGE_LOOP", 0, "");

  PostMainMessageLoopRun();
}

void BrowserMainParts::InitializeMainThread() {
  const char* kThreadName = "CrBrowserMain";
  base::PlatformThread::SetName(kThreadName);
  main_message_loop().set_thread_name(kThreadName);

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(new BrowserThread(BrowserThread::UI,
                                       MessageLoop::current()));
}

void BrowserMainParts::InitializeToolkit() {
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
  gfx::GtkInitFromCommandLine(parameters().command_line_);
  SetUpGLibLogHandler();
#endif

#if defined(TOOLKIT_GTK)
  // It is important for this to happen before the first run dialog, as it
  // styles the dialog as well.
  gfx::InitRCStyles();
#endif

#if defined(OS_WIN)
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  if (!InitCommonControlsEx(&config))
    LOG_GETLASTERROR(FATAL);
#endif

  ToolkitInitialized();
}

void BrowserMainParts::PreEarlyInitialization() {
}

void BrowserMainParts::PostEarlyInitialization() {
}

void BrowserMainParts::PreMainMessageLoopStart() {
}

void BrowserMainParts::PostMainMessageLoopStart() {
}

void BrowserMainParts::PreMainMessageLoopRun() {
}

void BrowserMainParts::MainMessageLoopRun() {
#if defined(OS_MACOSX)
  MessageLoopForUI::current()->Run();
#else
  MessageLoopForUI::current()->Run(NULL);
#endif
}

void BrowserMainParts::PostMainMessageLoopRun() {
}

void BrowserMainParts::ToolkitInitialized() {
}

}  // namespace content

// Main routine for running as the Browser process.
int BrowserMain(const MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN_ETW("BrowserMain", 0, "");

  scoped_ptr<content::BrowserMainParts> parts(
      content::GetContentClient()->browser()->CreateBrowserMainParts(
          parameters));

  parts->EarlyInitialization();

  // Must happen before we try to use a message loop or display any UI.
  parts->InitializeToolkit();

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

#if defined(OS_WIN)
#if !defined(NO_TCMALLOC)
  // When linking shared libraries, NO_TCMALLOC is defined, and dynamic
  // allocator selection is not supported.

  // Make this call before going multithreaded, or spawning any subprocesses.
  base::allocator::SetupSubprocessAllocator();
#endif
  // The broker service initialization needs to run early because it will
  // initialize the sandbox broker, which requires the process to swap its
  // window station. During this time all the UI will be broken. This has to
  // run before threads and windows are created.
  InitializeBrokerServices(parameters, parameters.command_line_);

  base::win::ScopedCOMInitializer com_initializer;
#endif  // OS_WIN

  // Initialize histogram statistics gathering system.
  base::StatisticsRecorder statistics;

  parts->RunMainMessageLoopParts();

  TRACE_EVENT_END_ETW("BrowserMain", 0, 0);
  return parts->result_code();
}
