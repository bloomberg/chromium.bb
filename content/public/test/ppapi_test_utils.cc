// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ppapi_test_utils.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "ppapi/shared_impl/ppapi_constants.h"

namespace ppapi {

namespace {

bool RegisterPlugin(
    base::CommandLine* command_line,
    const base::FilePath::StringType& library_name,
    const base::FilePath::StringType& extra_registration_parameters) {
  base::FilePath plugin_dir;
  if (!PathService::Get(base::DIR_MODULE, &plugin_dir))
    return false;

  base::FilePath plugin_path = plugin_dir.Append(library_name);

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
  return RegisterPlugin(command_line, plugin_library,
                        extra_registration_parameters);
}

bool RegisterPowerSaverTestPlugin(base::CommandLine* command_line) {
  base::FilePath::StringType library_name =
      base::FilePath::FromUTF8Unsafe(ppapi::kPowerSaverTestPluginName).value();
  return RegisterPlugin(command_line, library_name, FILE_PATH_LITERAL(""));
}

}  // namespace ppapi
