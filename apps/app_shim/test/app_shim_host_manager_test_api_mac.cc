// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/test/app_shim_host_manager_test_api_mac.h"

#include "apps/app_shim/app_shim_host_manager_mac.h"
#include "base/files/file_path.h"

namespace test {

// static
void AppShimHostManagerTestApi::OverrideUserDataDir(
    const base::FilePath& user_data_dir) {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, override_user_data_dir, ());
  override_user_data_dir = user_data_dir;
  AppShimHostManager::g_override_user_data_dir_ = &override_user_data_dir;
}

AppShimHostManagerTestApi::AppShimHostManagerTestApi(
    AppShimHostManager* host_manager)
    : host_manager_(host_manager) {
  DCHECK(host_manager_);
}

IPC::ChannelFactory* AppShimHostManagerTestApi::factory() {
  return host_manager_->factory_.get();
}

}  // namespace test
