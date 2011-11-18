// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_parts_gtk.h"

#include <gtk/gtk.h>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

ChromeBrowserPartsGtk::ChromeBrowserPartsGtk()
    : content::BrowserMainParts() {
}

void ChromeBrowserPartsGtk::PreEarlyInitialization() {
  DetectRunningAsRoot();
}

void ChromeBrowserPartsGtk::PostEarlyInitialization() {
}

void ChromeBrowserPartsGtk::ToolkitInitialized() {
}

void ChromeBrowserPartsGtk::PreMainMessageLoopStart() {
}

void ChromeBrowserPartsGtk::PostMainMessageLoopStart() {
}

void ChromeBrowserPartsGtk::PreMainMessageLoopRun() {
}

bool ChromeBrowserPartsGtk::MainMessageLoopRun(int* result_code) {
  return false;
}

void ChromeBrowserPartsGtk::PostMainMessageLoopRun() {
}

void ChromeBrowserPartsGtk::DetectRunningAsRoot() {
  if (geteuid() == 0) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kUserDataDir))
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

// static
void ChromeBrowserPartsGtk::ShowMessageBox(const char* message) {
  GtkWidget* dialog = gtk_message_dialog_new(
      NULL,
      static_cast<GtkDialogFlags>(0),
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE,
      "%s",
      message);

  gtk_window_set_title(GTK_WINDOW(dialog), message);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
