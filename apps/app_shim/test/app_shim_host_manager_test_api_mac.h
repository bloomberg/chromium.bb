// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_TEST_APP_SHIM_HOST_MANAGER_TEST_API_MAC_H
#define APPS_APP_SHIM_TEST_APP_SHIM_HOST_MANAGER_TEST_API_MAC_H

#include "base/basictypes.h"

class AppShimHostManager;

namespace base {
class FilePath;
}

namespace IPC {
class ChannelFactory;
}

namespace test {

class AppShimHostManagerTestApi {
 public:
  static void OverrideUserDataDir(const base::FilePath& user_data_dir);

  explicit AppShimHostManagerTestApi(AppShimHostManager* host_manager);

  IPC::ChannelFactory* factory();

 private:
  AppShimHostManager* host_manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerTestApi);
};

}  // namespace test

#endif  // APPS_APP_SHIM_TEST_APP_SHIM_HOST_MANAGER_TEST_API_MAC_H
