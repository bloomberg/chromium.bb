// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/engine/test_user_share.h"

#include "base/compiler_specific.h"

namespace browser_sync {

TestUserShare::TestUserShare() {}

TestUserShare::~TestUserShare() {
  CHECK(!user_share_.dir_manager.get());
}

void TestUserShare::SetUp() {
  setter_upper_.SetUp();
  // HACK: We have two scoped_ptrs to the same object.  But we make
  // sure to release one in TearDown.
  user_share_.dir_manager.reset(setter_upper_.manager());
  user_share_.name = setter_upper_.name();
}

void TestUserShare::TearDown() {
  // Make sure we don't free the manager twice.
  ignore_result(user_share_.dir_manager.release());
  setter_upper_.TearDown();
}

sync_api::UserShare* TestUserShare::user_share() {
  return &user_share_;
}

}  // namespace browser_sync
