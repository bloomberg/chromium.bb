// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/active_directory_test_helper.h"

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_paths.h"

namespace chromeos {

namespace active_directory_test_helper {

InstallAttributes::LockResult LockDevice(const std::string& domain) {
  base::RunLoop run_loop;
  InstallAttributes::LockResult return_lock_result;
  InstallAttributes::Get()->LockDevice(
      policy::DEVICE_MODE_ENTERPRISE_AD, "", domain, "device_id",
      base::AdaptCallbackForRepeating(base::BindOnce(
          [](base::OnceClosure closure,
             InstallAttributes::LockResult* return_lock_result,
             InstallAttributes::LockResult lock_result) {
            *return_lock_result = lock_result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &return_lock_result)));
  run_loop.Run();
  return return_lock_result;
}

void OverridePaths() {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  base::ScopedAllowBlockingForTesting allow_io;
  RegisterStubPathOverrides(user_data_dir);
}

}  // namespace active_directory_test_helper

}  // namespace chromeos
