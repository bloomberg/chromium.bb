// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {
namespace file_system_provider {
namespace util {

namespace {

// Root mount path for all of the provided file systems.
const base::FilePath::CharType kProvidedMountPointRoot[] =
    FILE_PATH_LITERAL("/provided");

}  // namespace

base::FilePath GetMountPointPath(Profile* profile,
                                 std::string extension_id,
                                 int file_system_id) {
  chromeos::User* const user =
      chromeos::UserManager::IsInitialized()
          ? chromeos::UserManager::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : NULL;
  const std::string user_suffix = user ? "-" + user->username_hash() : "";
  return base::FilePath(kProvidedMountPointRoot).AppendASCII(
      extension_id + "-" + base::IntToString(file_system_id) + user_suffix);
}

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos
