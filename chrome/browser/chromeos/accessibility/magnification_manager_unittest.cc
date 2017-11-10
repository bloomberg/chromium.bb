// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include "ash/public/cpp/accessibility_types.h"
#include "ash/test/ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

void EnableMagnifier() {
  return MagnificationManager::Get()->SetMagnifierEnabled(true);
}

void DisableMagnifier() {
  return MagnificationManager::Get()->SetMagnifierEnabled(false);
}

bool IsMagnifierEnabled() {
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

}  // namespace

class MagnificationManagerTest : public ash::AshTestBase {
 public:
  MagnificationManagerTest() = default;
  ~MagnificationManagerTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    MagnificationManager::Initialize();
    ASSERT_TRUE(MagnificationManager::Get());
    MagnificationManager::Get()->SetProfileForTest(&profile_);
  }

  void TearDown() override {
    MagnificationManager::Shutdown();
    ash::AshTestBase::TearDown();
  }

  TestingProfile profile_;
};

TEST_F(MagnificationManagerTest, EnableDisable) {
  // Set full screen magnifier, and confirm the status is set successfully.
  EnableMagnifier();
  EXPECT_TRUE(IsMagnifierEnabled());

  // Disables magnifier, and confirm the status is set successfully.
  DisableMagnifier();
  EXPECT_FALSE(IsMagnifierEnabled());
}

}  // namespace chromeos
