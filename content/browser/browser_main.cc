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
#include "content/browser/browser_thread.h"
#include "content/browser/content_browser_client.h"
#include "content/common/content_switches.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/main_function_params.h"
#include "content/common/sandbox_policy.h"
#include "net/base/network_change_notifier.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/tcp_client_socket.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "sandbox/src/sandbox.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <dbus/dbus-glib.h>
#endif

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

namespace {

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
      parsed_command_line_(parameters.command_line_) {
}

BrowserMainParts::~BrowserMainParts() {
}

void BrowserMainParts::EarlyInitialization() {
  PreEarlyInitialization();

  if (parsed_command_line().HasSwitch(switches::kEnableBenchmarking))
    base::FieldTrial::EnableBenchmarking();

  InitializeSSL();

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
  // seem dependent on InitializeSSL().
  if (parsed_command_line().HasSwitch(switches::kEnableTcpFastOpen))
    net::set_tcp_fastopen_enabled(true);

  PostEarlyInitialization();
}

void BrowserMainParts::MainMessageLoopStart() {
  PreMainMessageLoopStart();

  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

  // TODO(viettrungluu): should these really go before setting the thread name?
  system_monitor_.reset(new base::SystemMonitor);
  hi_res_timer_manager_.reset(new HighResolutionTimerManager);

  InitializeMainThread();

  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());

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
  // Init common control sex.
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

void BrowserMainParts::InitializeSSL() {
}

void BrowserMainParts::ToolkitInitialized() {
}

int BrowserMainParts::TemporaryContinue() {
  return 0;
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
  const CommandLine& parsed_command_line = parameters.command_line_;

#if defined(OS_WIN) && !defined(NO_TCMALLOC)
  // When linking shared libraries, NO_TCMALLOC is defined, and dynamic
  // allocator selection is not supported.

  // Make this call before going multithreaded, or spawning any subprocesses.
  base::allocator::SetupSubprocessAllocator();
#endif  // OS_WIN

  // The broker service initialization needs to run early because it will
  // initialize the sandbox broker, which requires the process to swap its
  // window station. During this time all the UI will be broken. This has to
  // run before threads and windows are created.
  InitializeBrokerServices(parameters, parsed_command_line);

  // Initialize histogram statistics gathering system.
  base::StatisticsRecorder statistics;

  // TODO(jam): bring the content parts from this chrome function here.
  int result_code = parts->TemporaryContinue();

  // Release BrowserMainParts here, before shutting down CrosLibrary, since
  // some of the classes initialized there have CrosLibrary dependencies.
  parts.reset(NULL);

  TRACE_EVENT_END_ETW("BrowserMain", 0, 0);
  return result_code;
}
