// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_finder.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

#if defined(OS_WIN)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  // Add user-level location.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string home_dir;
  if (env->GetVar("userprofile", &home_dir)) {
    base::FilePath default_location(base::UTF8ToWide(home_dir));
    if (base::win::GetVersion() < base::win::VERSION_VISTA) {
      default_location = default_location.Append(
          L"Local Settings\\Application Data");
    } else {
      default_location = default_location.Append(L"AppData\\Local");
    }
    locations->push_back(default_location);
  }

  // Add the system-level location.
  std::string program_dir;
  if (env->GetVar("ProgramFiles", &program_dir))
    locations->push_back(base::FilePath(base::UTF8ToWide(program_dir)));
  if (env->GetVar("ProgramFiles(x86)", &program_dir))
    locations->push_back(base::FilePath(base::UTF8ToWide(program_dir)));
}
#elif defined(OS_LINUX)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  locations->push_back(base::FilePath("/opt/google/chrome"));
  locations->push_back(base::FilePath("/usr/local/bin"));
  locations->push_back(base::FilePath("/usr/local/sbin"));
  locations->push_back(base::FilePath("/usr/bin"));
  locations->push_back(base::FilePath("/usr/sbin"));
  locations->push_back(base::FilePath("/bin"));
  locations->push_back(base::FilePath("/sbin"));
}
#endif

}  // namespace

namespace internal {

bool FindExe(
    const base::Callback<bool(const base::FilePath&)>& exists_func,
    const std::vector<base::FilePath>& rel_paths,
    const std::vector<base::FilePath>& locations,
    base::FilePath* out_path) {
  for (size_t i = 0; i < rel_paths.size(); ++i) {
    for (size_t j = 0; j < locations.size(); ++j) {
      base::FilePath path = locations[j].Append(rel_paths[i]);
      if (exists_func.Run(path)) {
        *out_path = path;
        return true;
      }
    }
  }
  return false;
}

}  // namespace internal

#if defined(OS_MACOSX)
void GetApplicationDirs(std::vector<base::FilePath>* locations);
#endif

bool FindChrome(base::FilePath* browser_exe) {
#if defined(OS_WIN)
  base::FilePath browser_exes_array[] = {
      base::FilePath(L"Google\\Chrome\\Application\\chrome.exe"),
      base::FilePath(L"Chromium\\Application\\chrome.exe")
  };
#elif defined(OS_MACOSX)
  base::FilePath browser_exes_array[] = {
      base::FilePath("Google Chrome.app/Contents/MacOS/Google Chrome"),
      base::FilePath("Chromium.app/Contents/MacOS/Chromium")
  };
#elif defined(OS_LINUX)
  base::FilePath browser_exes_array[] = {
      base::FilePath("google-chrome"),
      base::FilePath("chrome"),
      base::FilePath("chromium"),
      base::FilePath("chromium-browser")
  };
#endif
  std::vector<base::FilePath> browser_exes(
      browser_exes_array, browser_exes_array + arraysize(browser_exes_array));
  std::vector<base::FilePath> locations;
  GetApplicationDirs(&locations);
  return internal::FindExe(
      base::Bind(&file_util::PathExists),
      browser_exes,
      locations,
      browser_exe);
}
