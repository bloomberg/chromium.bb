// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmUtilTest);
};

TEST_F(PluginVmUtilTest, IsPluginVmAllowedForProfile) {
  profile_ = std::make_unique<TestingProfile>();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(profile_.get()));
}

}  // namespace plugin_vm
