// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

// Prints a JSON dict of the file manager strings
// (fileManagerPrivate.getStrings) to stdout.
// It is used as part of the build process to extract strings for
// file manager tests.
int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  // Load resource bundle to get locales/en-US.pak.
  ui::RegisterPathProvider();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

  // Get FileManager strings as JS dict.
  std::unique_ptr<base::Value> dict =
      extensions::FileManagerPrivateStrings::GetStrings(
          "en-US",     // app_locale
          false,       // pdf_view_enabled
          false,       // swf_view_enabled
          false,       // is_device_in_demo_mode
          "unknown");  // lsb_release_board

  // Print JSON to stdout.
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *dict, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT, &json);
  std::cout << json;
}
