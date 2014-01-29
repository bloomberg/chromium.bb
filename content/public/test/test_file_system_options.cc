// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_system_options.h"

#include <string>
#include <vector>

using fileapi::FileSystemOptions;

namespace content {

FileSystemOptions CreateIncognitoFileSystemOptions() {
  std::vector<std::string> additional_allowed_schemes;
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                           additional_allowed_schemes,
                           NULL);
}

FileSystemOptions CreateAllowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back("file");
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes,
                           NULL);
}

FileSystemOptions CreateDisallowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes,
                           NULL);
}

}  // namespace content
