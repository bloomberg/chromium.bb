// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_paths.h"

#include "base/base_paths.h"
#include "base/base_paths_fuchsia.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"

namespace chromecast {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_CAST_HOME: {
      base::FilePath home = base::GetHomeDir();
#if BUILDFLAG(IS_CAST_DESKTOP_BUILD)
      // When running a development instance as a regular user, use
      // a data directory under $HOME (similar to Chrome).
      *result = home.Append(".config/cast_shell");
#else
      // When running on the actual device, $HOME is set to the user's
      // directory under the data partition.
      *result = home;
#endif
      return true;
    }
#if defined(OS_ANDROID)
    case FILE_CAST_ANDROID_LOG: {
      base::FilePath base_dir;
      CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir));
      *result = base_dir.AppendASCII("cast_shell.log");
      return true;
    }
#endif  // defined(OS_ANDROID)
    case FILE_CAST_CONFIG: {
      base::FilePath data_dir;
#if defined(OS_ANDROID)
      CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_dir));
      *result = data_dir.Append("cast_shell.conf");
#elif defined(OS_FUCHSIA)
      CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
      *result = data_dir.Append(".eureka.conf");
#else
      CHECK(base::PathService::Get(DIR_CAST_HOME, &data_dir));
      *result = data_dir.Append(".eureka.conf");
#endif  // defined(OS_ANDROID)
      return true;
    }
    case FILE_CAST_CRL: {
      base::FilePath data_dir;
#if defined(OS_ANDROID)
      CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_dir));
      *result = data_dir.Append("cast_shell.crl");
#elif defined(OS_FUCHSIA)
      CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
      *result = data_dir.Append(".eureka.crl");
#else
      CHECK(base::PathService::Get(DIR_CAST_HOME, &data_dir));
      *result = data_dir.Append(".eureka.crl");
#endif  // defined(OS_ANDROID)
      return true;
    }
    case FILE_CAST_PAK: {
      base::FilePath base_dir;
#if defined(OS_ANDROID)
      CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir));
      *result = base_dir.Append("paks/cast_shell.pak");
#else
      CHECK(base::PathService::Get(base::DIR_ASSETS, &base_dir));
      *result = base_dir.Append("assets/cast_shell.pak");
#endif  // defined(OS_ANDROID)
      return true;
    }
  }
  return false;
}

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chromecast
