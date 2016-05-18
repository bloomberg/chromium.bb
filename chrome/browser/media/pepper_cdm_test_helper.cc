// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/pepper_cdm_test_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"

#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.

const char kClearKeyCdmDisplayName[] = "Clear Key CDM";

const char kClearKeyCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
    "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
    "libclearkeycdmadapter.so";
#endif

const char kClearKeyCdmPepperMimeType[] = "application/x-ppapi-clearkey-cdm";

base::FilePath::StringType BuildPepperCdmRegistration(
    const std::string& adapter_file_name,
    const std::string& display_name,
    const std::string& mime_type,
    bool expect_adapter_exists) {
  base::FilePath adapter_path;
  PathService::Get(base::DIR_MODULE, &adapter_path);
  adapter_path = adapter_path.AppendASCII(adapter_file_name);
  DCHECK_EQ(expect_adapter_exists, base::PathExists(adapter_path));

  base::FilePath::StringType pepper_cdm_registration = adapter_path.value();

  std::string string_to_append = "#";
  string_to_append.append(display_name);
  string_to_append.append("#CDM#0.1.0.0;");
  string_to_append.append(mime_type);

#if defined(OS_WIN)
  pepper_cdm_registration.append(base::ASCIIToUTF16(string_to_append));
#else
  pepper_cdm_registration.append(string_to_append);
#endif

  return pepper_cdm_registration;
}

void RegisterPepperCdm(base::CommandLine* command_line,
                       const std::string& adapter_file_name,
                       const std::string& display_name,
                       const std::string& mime_type,
                       bool expect_adapter_exists) {
  base::FilePath::StringType pepper_cdm_registration =
      BuildPepperCdmRegistration(adapter_file_name, display_name, mime_type,
                                 expect_adapter_exists);

  // Append the switch to register the CDM Adapter.
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                   pepper_cdm_registration);
}
