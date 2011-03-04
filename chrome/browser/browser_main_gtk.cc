// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main_gtk.h"

#include <gtk/gtk.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/result_codes.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/gtk_util.h"

#if defined(USE_NSS)
#include "base/nss_util.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

namespace {

// Indicates that we're currently responding to an IO error (by shutting down).
bool g_in_x11_io_error_handler = false;

int BrowserX11ErrorHandler(Display* d, XErrorEvent* error) {
  if (!g_in_x11_io_error_handler)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableFunction(ui::LogErrorEventDescription, d, *error));
  return 0;
}

int BrowserX11IOErrorHandler(Display* d) {
  // If there's an IO error it likely means the X server has gone away
  if (!g_in_x11_io_error_handler) {
    g_in_x11_io_error_handler = true;
    LOG(ERROR) << "X IO Error detected";
    BrowserList::SessionEnding();
  }

  return 0;
}

}  // namespace

void BrowserMainPartsGtk::PreEarlyInitialization() {
  DetectRunningAsRoot();

  BrowserMainPartsPosix::PreEarlyInitialization();

  SetupSandbox();

#if defined(USE_NSS)
  // We want to be sure to init NSPR on the main thread.
  base::EnsureNSPRInit();
#endif
}

void BrowserMainPartsGtk::DetectRunningAsRoot() {
  if (geteuid() == 0) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    gfx::GtkInitFromCommandLine(command_line);

    // Get just enough of our resource machinery up so we can extract the
    // locale appropriate string. Note that the GTK implementation ignores the
    // passed in parameter and checks the LANG environment variables instead.
    ResourceBundle::InitSharedInstance("");

    std::string message = l10n_util::GetStringFUTF8(
            IDS_REFUSE_TO_RUN_AS_ROOT,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    GtkWidget* dialog = gtk_message_dialog_new(
        NULL,
        static_cast<GtkDialogFlags>(0),
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_CLOSE,
        "%s",
        message.c_str());

    message = l10n_util::GetStringFUTF8(
        IDS_REFUSE_TO_RUN_AS_ROOT_2,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "%s",
                                             message.c_str());

    message = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
    gtk_window_set_title(GTK_WINDOW(dialog), message.c_str());

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    exit(EXIT_FAILURE);
  }
}

void BrowserMainPartsGtk::SetupSandbox() {
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
  if (sandbox_binary && !parsed_command_line().HasSwitch(switches::kNoSandbox))
    sandbox_cmd = sandbox_binary;

  // Tickle the sandbox host and zygote host so they fork now.
  RenderSandboxHostLinux* shost = RenderSandboxHostLinux::GetInstance();
  shost->Init(sandbox_cmd);
  ZygoteHost* zhost = ZygoteHost::GetInstance();
  zhost->Init(sandbox_cmd);
}

void DidEndMainMessageLoop() {
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
#if defined(USE_LINUX_BREAKPAD)
  metrics->RecordBreakpadRegistration(IsCrashReporterEnabled());
#else
  metrics->RecordBreakpadRegistration(false);
#endif
  metrics->RecordBreakpadHasDebugger(base::debug::BeingDebugged());
}

void WarnAboutMinimumSystemRequirements() {
  // Nothing to warn about on GTK right now.
}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return ResultCodes::NORMAL_EXIT;
}

int HandleIconsCommands(const CommandLine &parsed_command_line) {
  return 0;
}

bool CheckMachineLevelInstall() {
  return false;
}

void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line) {
}

void SetBrowserX11ErrorHandlers() {
  // Set up error handlers to make sure profile gets written if X server
  // goes away.
  ui::SetX11ErrorHandlers(BrowserX11ErrorHandler, BrowserX11IOErrorHandler);
}

#if !defined(OS_CHROMEOS)
// static
BrowserMainParts* BrowserMainParts::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return new BrowserMainPartsGtk(parameters);
}
#endif
