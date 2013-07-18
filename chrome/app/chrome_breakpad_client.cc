// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_breakpad_client.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_WIN)
#include "base/file_version_info.h"
#endif

#if defined(OS_POSIX)
#include "chrome/common/dump_without_crashing.h"
#endif

namespace chrome {

ChromeBreakpadClient::ChromeBreakpadClient() {}

ChromeBreakpadClient::~ChromeBreakpadClient() {}

#if defined(OS_WIN)
bool ChromeBreakpadClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    *crash_dir = base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    return true;
  }

  return false;
}

void ChromeBreakpadClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build) {
  DCHECK(product_name);
  DCHECK(version);
  DCHECK(special_build);

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(exe_path));

  if (version_info.get()) {
    // Get the information from the file.
    *version = version_info->product_version();
    if (!version_info->is_official_build())
      version->append(base::ASCIIToUTF16("-devel"));

    const CommandLine& command = *CommandLine::ForCurrentProcess();
    if (command.HasSwitch(switches::kChromeFrame)) {
      *product_name = base::ASCIIToUTF16("ChromeFrame");
    } else {
      *product_name = version_info->product_short_name();
    }

    *special_build = version_info->special_build();
  } else {
    // No version info found. Make up the values.
    *product_name = base::ASCIIToUTF16("Chrome");
    *version = base::ASCIIToUTF16("0.0.0.0-devel");
  }
}
#endif

bool ChromeBreakpadClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    base::FilePath crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    PathService::Override(chrome::DIR_CRASH_DUMPS, crash_dumps_dir_path);
  }

  return PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

#if defined(OS_POSIX)
void ChromeBreakpadClient::SetDumpWithoutCrashingFunction(void (*function)()) {
  logging::SetDumpWithoutCrashingFunction(function);
}
#endif

}  // namespace chrome
