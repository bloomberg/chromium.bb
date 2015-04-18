// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ppapi_test_utils.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"

namespace ppapi {

std::string StripTestPrefixes(const std::string& test_name) {
  if (test_name.find("DISABLED_") == 0)
    return test_name.substr(strlen("DISABLED_"));
  return test_name;
}

bool RegisterTestPlugin(base::CommandLine* command_line) {
  return RegisterTestPluginWithExtraParameters(command_line,
                                               FILE_PATH_LITERAL(""));
}

bool RegisterTestPluginWithExtraParameters(
    base::CommandLine* command_line,
    const base::FilePath::StringType& extra_registration_parameters) {
  base::FilePath plugin_dir;
  if (!PathService::Get(base::DIR_MODULE, &plugin_dir))
    return false;

#if defined(OS_WIN)
  base::FilePath plugin_path = plugin_dir.Append(L"ppapi_tests.dll");
#elif defined(OS_MACOSX)
  base::FilePath plugin_path = plugin_dir.Append("ppapi_tests.plugin");
#elif defined(OS_POSIX)
  base::FilePath plugin_path = plugin_dir.Append("libppapi_tests.so");
#endif

  // Append the switch to register the pepper plugin.
  if (!base::PathExists(plugin_path))
    return false;
  base::FilePath::StringType pepper_plugin = plugin_path.value();
  pepper_plugin.append(extra_registration_parameters);
  pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_plugin);
  return true;
}

}  // namespace ppapi
