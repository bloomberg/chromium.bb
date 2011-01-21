// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window.h"

#include <gtk/gtk.h>

#include <string>

#include "app/app_paths.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "grit/app_locale_settings.h"
#include "third_party/cros/chromeos_cros_api.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "views/focus/accelerator_handler.h"

int main(int argc, char** argv) {
  // Initialize gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);

  // Initialize Chrome stuff.
  base::AtExitManager exit_manager;
  base::EnableTerminationOnHeapCorruption();
  app::RegisterPathProvider();
  ui::RegisterPathProvider();
  CommandLine::Init(argc, argv);

  // Check if the UI language code is passed from the command line,
  // otherwise, default to "en-US".
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string ui_language_code =
      command_line.GetSwitchValueASCII(switches::kCandidateWindowLang);
  if (ui_language_code.empty()) {
    ui_language_code = "en-US";
  }
  ResourceBundle::InitSharedInstance(ui_language_code);

  // Change the UI font if needed.
  const std::string font_name =
      l10n_util::GetStringUTF8(IDS_UI_FONT_FAMILY_CROS);
  // The font name should not be empty here, but just in case.
  if (font_name != "default" && !font_name.empty()) {
    // Don't use gtk_util::SetGtkFont() in chrome/browser/ui/gtk not to
    // introduce a dependency to it.
    g_object_set(gtk_settings_get_default(),
                 "gtk-font-name", font_name.c_str(), NULL);
  }

  // Load libcros.
  chrome::RegisterPathProvider();  // for libcros.so.
  chromeos::CrosLibraryLoader lib_loader;
  std::string error_string;
  CHECK(lib_loader.Load(&error_string))
      << "Failed to load libcros, " << error_string;

  // Create the main message loop.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  // Create the candidate window controller.
  chromeos::CandidateWindowController controller;
  if (!controller.Init()) {
    return 1;
  }

  // Start the main loop.
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);

  return 0;
}
