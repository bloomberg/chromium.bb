// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_

#include <string>

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace file_system_provider {
namespace util {

// Constructs a safe mount point path for the provided file system.
base::FilePath GetMountPointPath(Profile* profile,
                                 std::string extension_id,
                                 int file_system_id);

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
