// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/legacy_firewall_manager_win.h"

#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class LegacyFirewallManagerTest : public ::testing::Test {
 public:
  LegacyFirewallManagerTest() : skip_test_(true) {}

 protected:
  // Sets up the test fixture.
  virtual void SetUp() OVERRIDE {
    base::IntegrityLevel level = base::INTEGRITY_UNKNOWN;
    if (GetProcessIntegrityLevel(base::GetCurrentProcessHandle(), &level) &&
        level != base::HIGH_INTEGRITY) {
      LOG(WARNING) << "Not elevated. Skipping the test.";
      return;
    };
    skip_test_ = false;
    base::FilePath exe_path;
    PathService::Get(base::FILE_EXE, &exe_path);
    EXPECT_TRUE(manager_.Init(L"LegacyFirewallManagerTest", exe_path));
    manager_.DeleteRule();
  }

  // Tears down the test fixture.
  virtual void TearDown() OVERRIDE {
    if (!skip_test_)
      manager_.DeleteRule();
  }

  bool skip_test_;
  LegacyFirewallManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyFirewallManagerTest);
};

TEST_F(LegacyFirewallManagerTest, NoRule) {
  if (skip_test_)
    return;
  EXPECT_FALSE(manager_.GetAllowIncomingConnection(NULL));
}

TEST_F(LegacyFirewallManagerTest, AllowRule) {
  if (skip_test_)
    return;
  EXPECT_TRUE(manager_.SetAllowIncomingConnection(true));
  bool allowed = false;
  EXPECT_TRUE(manager_.GetAllowIncomingConnection(&allowed));
  EXPECT_TRUE(allowed);
  manager_.DeleteRule();
  EXPECT_FALSE(manager_.GetAllowIncomingConnection(NULL));
}

TEST_F(LegacyFirewallManagerTest, BlockRule) {
  if (skip_test_)
    return;
  EXPECT_TRUE(manager_.SetAllowIncomingConnection(false));
  bool allowed = true;
  EXPECT_TRUE(manager_.GetAllowIncomingConnection(&allowed));
  EXPECT_FALSE(allowed);
  manager_.DeleteRule();
  EXPECT_FALSE(manager_.GetAllowIncomingConnection(NULL));
}

}  // namespace installer
