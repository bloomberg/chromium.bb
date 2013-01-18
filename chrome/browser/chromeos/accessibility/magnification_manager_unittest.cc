// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include "ash/magnifier/magnifier_constants.h"
#include "ash/test/ash_test_base.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class MockMagnificationObserver : public content::NotificationObserver {
 public:
  MockMagnificationObserver() : observed_(false),
                                observed_enabled_(false),
                                observed_type_(ash::kDefaultMagnifierType) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources());
  }

  bool observed() { return observed_; }
  bool observed_enabled() { return observed_enabled_; }
  ash::MagnifierType observed_type() { return observed_type_; }

  void reset() { observed_ = false; }

 private:
  // content::NotificationObserver implimentation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER:
        accessibility::AccessibilityStatusEventDetails* accessibility_status =
            content::Details<accessibility::AccessibilityStatusEventDetails>(
                details).ptr();

        observed_ = true;
        observed_enabled_ = accessibility_status->enabled;
        observed_type_ = accessibility_status->magnifier_type;
    }
  }

  bool observed_;
  bool observed_enabled_;
  ash::MagnifierType observed_type_;

  content::NotificationRegistrar registrar_;
};

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

}  // anonymous namespace

class MagnificationManagerTest : public ash::test::AshTestBase {
 public:
  MagnificationManagerTest() {
  }

  void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    MagnificationManager::Initialize();
    ASSERT_TRUE(MagnificationManager::Get());
  }

  void TearDown() OVERRIDE {
    MagnificationManager::Shutdown();
    ash::test::AshTestBase::TearDown();
  }
};

TEST_F(MagnificationManagerTest, MagnificationObserver) {
  MockMagnificationObserver* observer = new MockMagnificationObserver();

  EXPECT_FALSE(observer->observed());

  // Set full screen magnifier, and confirm the observer is called.
  EnableMagnifier();
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(observer->observed());
  EXPECT_TRUE(observer->observed_enabled());
  EXPECT_EQ(observer->observed_type(), ash::MAGNIFIER_FULL);
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);
  observer->reset();

  // Set full screen magnifier again, and confirm the observer is not called.
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_FALSE(observer->observed());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);
  observer->reset();

  // Set partial screen magnifier, and confirm the observer is called.
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_TRUE(observer->observed());
  EXPECT_TRUE(observer->observed_enabled());
  EXPECT_EQ(observer->observed_type(), ash::MAGNIFIER_PARTIAL);
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);
  observer->reset();

  // Set partial screen magnifier again, and confirm the observer is not called.
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_FALSE(observer->observed());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);
  observer->reset();

  // Disables magnifier, and confirm the observer is called.
  DisableMagnifier();
  EXPECT_TRUE(observer->observed());
  EXPECT_FALSE(observer->observed_enabled());
  EXPECT_EQ(observer->observed_type(), ash::MAGNIFIER_PARTIAL);
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);
  observer->reset();

  // Disables magnifier again, and confirm the observer is NOT called.
  DisableMagnifier();
  EXPECT_FALSE(observer->observed());
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);
  observer->reset();
}

TEST_F(MagnificationManagerTest, ChangeType) {
  // Set full screen magnifier, and confirm the status is set successfully.
  EnableMagnifier();
  SetMagnifierType(ash::MAGNIFIER_FULL);
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_FULL);

  // Set partial screen magnifier, and confirm the status is set successfully.
  SetMagnifierType(ash::MAGNIFIER_PARTIAL);
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);

  // Disables magnifier, and confirm the status is set successfully.
  DisableMagnifier();
  EXPECT_FALSE(IsMagnifierEnabled());
  EXPECT_EQ(GetMagnifierType(), ash::MAGNIFIER_PARTIAL);
}

}  // namespace chromeos
