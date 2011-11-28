// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/trace_controller.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/sandbox_policy.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
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

#include "content/browser/system_message_window_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "net/base/winsock_init.h"
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
#include <glib-object.h>
#endif

#if defined(OS_CHROMEOS)
#include <dbus/dbus-glib.h>
#endif

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/stat.h>
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#endif

namespace {

#if defined(OS_POSIX) && !defined(OS_MACOSX)
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

#if defined(OS_LINUX) || defined(OS_OPENBSD)
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
  } else if (strstr(message, "XDG_RUNTIME_DIR variable not set")) {
    LOG(ERROR) << message << " (http://bugs.chromium.org/97293)";
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


// BrowserMainLoop construction / destructione =============================

BrowserMainLoop::BrowserMainLoop(const content::MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(content::RESULT_CODE_NORMAL_EXIT) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif
}

BrowserMainLoop::~BrowserMainLoop() {
#if defined(OS_WIN)
  OleUninitialize();
#endif
}

void BrowserMainLoop::Init() {
  parts_.reset(
      GetContentClient()->browser()->CreateBrowserMainParts(parameters_));
}

// BrowserMainLoop stages ==================================================

void BrowserMainLoop::EarlyInitialization() {
  if (parts_.get())
    parts_->PreEarlyInitialization();

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif

#if !defined(USE_OPENSSL)
  // Use NSS for SSL by default.
  // The default client socket factory uses NSS for SSL by default on
  // Windows and Mac.
  bool init_nspr = false;
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (parsed_command_line_.HasSwitch(switches::kUseSystemSSL)) {
    net::ClientSocketFactory::UseSystemSSL();
  } else {
    init_nspr = true;
  }
#elif defined(USE_NSS)
  init_nspr = true;
#endif
  if (init_nspr) {
    // We want to be sure to init NSPR on the main thread.
    crypto::EnsureNSPRInit();
  }
#endif  // !defined(USE_OPENSSL)

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  SetupSandbox(parsed_command_line_);
#endif

  if (parsed_command_line_.HasSwitch(switches::kDisableSSLFalseStart))
    net::SSLConfigService::DisableFalseStart();
  if (parsed_command_line_.HasSwitch(switches::kEnableSSLCachedInfo))
    net::SSLConfigService::EnableCachedInfo();
  if (parsed_command_line_.HasSwitch(switches::kEnableOriginBoundCerts))
    net::SSLConfigService::EnableOriginBoundCerts();
  if (parsed_command_line_.HasSwitch(
          switches::kEnableDNSCertProvenanceChecking)) {
    net::SSLConfigService::EnableDNSCertProvenanceChecking();
  }

  // TODO(abarth): Should this move to InitializeNetworkOptions?  This doesn't
  // seem dependent on SSL initialization().
  if (parsed_command_line_.HasSwitch(switches::kEnableTcpFastOpen))
    net::set_tcp_fastopen_enabled(true);

  if (parts_.get())
    parts_->PostEarlyInitialization();
}

void BrowserMainLoop::MainMessageLoopStart() {
  if (parts_.get())
    parts_->PreMainMessageLoopStart();

#if defined(OS_WIN)
  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (!parameters_.ui_task) {
    // Override the configured locale with the user's preferred UI language.
    l10n_util::OverrideLocaleWithUILanguageList();
  }
#endif

  // Must first NULL pointer or we hit a DCHECK that the newly constructed
  // message loop is the current one.
  main_message_loop_.reset();
  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

  InitializeMainThread();

  // Start tracing to a file if needed.
  if (base::debug::TraceLog::GetInstance()->IsEnabled())
    TraceController::GetInstance()->InitStartupTracing(parsed_command_line_);

  system_monitor_.reset(new base::SystemMonitor);
  hi_res_timer_manager_.reset(new HighResolutionTimerManager);

  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());

#if defined(OS_WIN)
  system_message_window_.reset(new SystemMessageWindowWin);
#endif

  if (parts_.get())
    parts_->PostMainMessageLoopStart();
}

void BrowserMainLoop::RunMainMessageLoopParts(
    bool* completed_main_message_loop) {
  if (parts_.get())
    parts_->PreMainMessageLoopRun();

  TRACE_EVENT_BEGIN_ETW("BrowserMain:MESSAGE_LOOP", 0, "");
  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  base::ThreadRestrictions::SetIOAllowed(false);

  bool ran_main_loop = false;
  if (parts_.get())
    ran_main_loop = parts_->MainMessageLoopRun(&result_code_);

  if (!ran_main_loop)
    MainMessageLoopRun();

  TRACE_EVENT_END_ETW("BrowserMain:MESSAGE_LOOP", 0, "");

  if (completed_main_message_loop)
    *completed_main_message_loop = true;

  if (parts_.get())
    parts_->PostMainMessageLoopRun();
}

void BrowserMainLoop::InitializeMainThread() {
  const char* kThreadName = "CrBrowserMain";
  base::PlatformThread::SetName(kThreadName);
  main_message_loop_->set_thread_name(kThreadName);

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           MessageLoop::current()));
}

void BrowserMainLoop::InitializeToolkit() {
  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.
  // TODO(stevenjb): Move platform specific code into platform specific Parts
  // (Need to add InitializeToolkit stage to BrowserParts).
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Glib type system initialization. Needed at least for gconf,
  // used in net/proxy/proxy_config_service_linux.cc. Most likely
  // this is superfluous as gtk_init() ought to do this. It's
  // definitely harmless, so retained as a reminder of this
  // requirement for gconf.
  g_type_init();

#if !defined(USE_AURA)
  gfx::GtkInitFromCommandLine(parameters_.command_line);
#endif

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

  if (parts_.get())
    parts_->ToolkitInitialized();
}

void BrowserMainLoop::MainMessageLoopRun() {
  if (parameters_.ui_task)
    MessageLoopForUI::current()->PostTask(FROM_HERE, parameters_.ui_task);

#if defined(OS_MACOSX)
  MessageLoopForUI::current()->Run();
#else
  MessageLoopForUI::current()->RunWithDispatcher(NULL);
#endif
}

}  // namespace content
