// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ppapi_test_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "ppapi/shared_impl/ppapi_switches.h"

using CharType = base::FilePath::CharType;
using StringType = base::FilePath::StringType;

namespace ppapi {

namespace {

struct PluginInfo {
  PluginInfo(const StringType& library_name,
             const StringType& extra_params,
             const StringType& mime_type)
      : library_name(library_name),
        extra_params(extra_params),
        mime_type(mime_type) {}

  StringType library_name;
  // Typically a string of the form #name#description#version
  StringType extra_params;
  StringType mime_type;
};

bool RegisterPlugins(base::CommandLine* command_line,
                     const std::vector<PluginInfo>& plugins) {
  base::FilePath plugin_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &plugin_dir))
    return false;

  StringType args;
  for (const auto& plugin : plugins) {
    if (!args.empty())
      args += FILE_PATH_LITERAL(",");
    base::FilePath plugin_path = plugin_dir.Append(plugin.library_name);
    if (!base::PathExists(plugin_path))
      return false;
    args += plugin_path.value();
    args += plugin.extra_params;
    args += FILE_PATH_LITERAL(";");
    args += plugin.mime_type;
  }
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins, args);
  return true;
}

bool RegisterPluginWithDefaultMimeType(
    base::CommandLine* command_line,
    const base::FilePath::StringType& library_name,
    const base::FilePath::StringType& extra_registration_parameters) {
  std::vector<PluginInfo> plugins;
  plugins.push_back(PluginInfo(library_name, extra_registration_parameters,
                               FILE_PATH_LITERAL("application/x-ppapi-tests")));
  return RegisterPlugins(command_line, plugins);
}

}  // namespace

bool RegisterTestPlugin(base::CommandLine* command_line) {
  return RegisterTestPluginWithExtraParameters(command_line,
                                               FILE_PATH_LITERAL(""));
}

bool RegisterTestPluginWithExtraParameters(
    base::CommandLine* command_line,
    const base::FilePath::StringType& extra_registration_parameters) {
#if defined(OS_WIN)
  base::FilePath::StringType plugin_library = L"ppapi_tests.dll";
#elif defined(OS_MACOSX)
  base::FilePath::StringType plugin_library = "ppapi_tests.plugin";
#elif defined(OS_POSIX)
  base::FilePath::StringType plugin_library = "libppapi_tests.so";
#endif
  return RegisterPluginWithDefaultMimeType(command_line, plugin_library,
                                           extra_registration_parameters);
}

bool RegisterFlashTestPlugin(base::CommandLine* command_line) {
  // Power Saver plugin requires Pepper testing API.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // The Power Saver plugin ignores the data attribute and just draws a
  // checkerboard pattern - while providing some Plugin Power Saver diagnostics.
  //
  // It was originally designed just for Plugin Power Saver tests, but is
  // useful for testing as a fake Flash plugin in a variety of tests.
  base::FilePath::StringType library_name =
      base::FilePath::FromUTF8Unsafe(ppapi::kPowerSaverTestPluginName).value();
  std::vector<PluginInfo> plugins;
  // Register a fake Flash with 100.0 version (to avoid outdated checks).
  base::FilePath::StringType fake_flash_parameter =
      base::FilePath::FromUTF8Unsafe(
          std::string("#") + content::kFlashPluginName + "#Description#100.0")
          .value();
  plugins.push_back(
      PluginInfo(library_name, fake_flash_parameter,
                 FILE_PATH_LITERAL("application/x-shockwave-flash")));
  return RegisterPlugins(command_line, plugins);
}

bool RegisterBlinkTestPlugin(base::CommandLine* command_line) {
#if defined(OS_WIN)
  static const CharType kPluginLibrary[] = L"blink_test_plugin.dll";
  static const CharType kDeprecatedPluginLibrary[] =
      L"blink_deprecated_test_plugin.dll";
#elif defined(OS_MACOSX)
  static const CharType kPluginLibrary[] = "blink_test_plugin.plugin";
  static const CharType kDeprecatedPluginLibrary[] =
      "blink_deprecated_test_plugin.plugin";
#elif defined(OS_POSIX)
  static const CharType kPluginLibrary[] = "libblink_test_plugin.so";
  static const CharType kDeprecatedPluginLibrary[] =
      "libblink_deprecated_test_plugin.so";
#endif
  static const CharType kExtraParameters[] =
      FILE_PATH_LITERAL("#Blink Test Plugin#Interesting description.#0.8");
  static const CharType kDeprecatedExtraParameters[] =
      FILE_PATH_LITERAL("#Blink Deprecated Test Plugin#Description#0.1");

  std::vector<PluginInfo> plugins;
  plugins.push_back(PluginInfo(
      kPluginLibrary, kExtraParameters,
      FILE_PATH_LITERAL("application/x-blink-test-plugin#blinktestplugin")));
  plugins.push_back(
      PluginInfo(kDeprecatedPluginLibrary, kDeprecatedExtraParameters,
                 FILE_PATH_LITERAL("application/"
                                   "x-blink-deprecated-test-plugin#"
                                   "blinkdeprecatedtestplugin")));
  return RegisterPlugins(command_line, plugins);
}

}  // namespace ppapi
