// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_gtk.h"

#include <gtk/gtk.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/gtk_util.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

namespace {

// Indicates that we're currently responding to an IO error (by shutting down).
bool g_in_x11_io_error_handler = false;

// Number of seconds to wait for UI thread to get an IO error if we get it on
// the background thread.
const int kWaitForUIThreadSeconds = 10;

int BrowserX11ErrorHandler(Display* d, XErrorEvent* error) {
  if (!g_in_x11_io_error_handler)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableFunction(ui::LogErrorEventDescription, d, *error));
  return 0;
}


// This function is used to help us diagnose crash dumps that happen
// during the shutdown process.
NOINLINE void WaitingForUIThreadToHandleIOError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(kWaitForUIThreadSeconds);
}

int BrowserX11IOErrorHandler(Display* d) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Wait for the UI thread (which has a different connection to the X server)
    // to get the error. We can't call shutdown from this thread without
    // tripping an error. Doing it through a function so that we'll be able
    // to see it in any crash dumps.
    WaitingForUIThreadToHandleIOError();
    return 0;
  }
  // If there's an IO error it likely means the X server has gone away
  if (!g_in_x11_io_error_handler) {
    g_in_x11_io_error_handler = true;
    LOG(ERROR) << "X IO Error detected";
    BrowserList::SessionEnding();
  }

  return 0;
}

}  // namespace

ChromeBrowserMainPartsGtk::ChromeBrowserMainPartsGtk(
    const MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

void ChromeBrowserMainPartsGtk::PreEarlyInitialization() {
  DetectRunningAsRoot();

  ChromeBrowserMainPartsPosix::PreEarlyInitialization();
}

void ChromeBrowserMainPartsGtk::DetectRunningAsRoot() {
  if (geteuid() == 0) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (parsed_command_line().HasSwitch(switches::kUserDataDir))
      return;

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

    LOG(ERROR) << "Startup refusing to run as root.";
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

void ShowMissingLocaleMessageBox() {
  GtkWidget* dialog = gtk_message_dialog_new(
      NULL,
      static_cast<GtkDialogFlags>(0),
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE,
      "%s",
      chrome_browser::kMissingLocaleDataMessage);

  gtk_window_set_title(GTK_WINDOW(dialog),
                       chrome_browser::kMissingLocaleDataTitle);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void RecordBrowserStartupTime() {
  // Not implemented on GTK for now.
}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return content::RESULT_CODE_NORMAL_EXIT;
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
