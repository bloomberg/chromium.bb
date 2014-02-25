// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include "ash/magnifier/magnifier_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
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

ash::MagnifierType GetMagnifierType() {
  return MagnificationManager::Get()->GetMagnifierType();
}

void SetMagnifierType(ash::MagnifierType type) {
  return MagnificationManager::Get()->SetMagnifierType(type);
}

}  // namespace

class MagnificationManagerTest : public ash::test::AshTestBase {
 public:
  MagnificationManagerTest() {
  }

  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    MagnificationManager::Initialize();
    ASSERT_TRUE(MagnificationManager::Get());
    MagnificationManager::Get()->SetProfileForTest(&profile_);
  }

  virtual void TearDown() OVERRIDE {
    MagnificationManager::Shutdown();
    ash::test::AshTestBase::TearDown();
  }

  TestingProfile profile_;
};

TEST_F(MagnificationManagerTest, ChangeType) {
  // Set full screen magnifier, and confirm the status is set successfully.
  EnableMagnifier();
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);

  // Set partial screen magnifier, and confirm the change is ignored.
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);

  // Disables magnifier, and confirm the status is set successfully.
  DisableMagnifier();
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);
}

}  // namespace chromeos
