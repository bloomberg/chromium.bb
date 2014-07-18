// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/test/app_shim_host_manager_test_api_mac.h"

#include "apps/app_shim/app_shim_host_manager_mac.h"
#include "base/files/file_path.h"

namespace test {

AppShimHostManagerTestApi::AppShimHostManagerTestApi(
    AppShimHostManager* host_manager)
    : host_manager_(host_manager) {
  DCHECK(host_manager_);
}

apps::UnixDomainSocketAcceptor* AppShimHostManagerTestApi::acceptor() {
  return host_manager_->acceptor_.get();
}

const base::FilePath& AppShimHostManagerTestApi::directory_in_tmp() {
  return host_manager_->directory_in_tmp_;
}

}  // namespace test
