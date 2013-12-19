// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kFudgeInt = 100;

}

namespace chromeos {

class KioskModeSettingsTest : public testing::Test {
 protected:
  KioskModeSettingsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {
    CrosSettings* cros_settings = CrosSettings::Get();

    // Remove the real DeviceSettingsProvider and replace it with a stub.
    device_settings_provider_ =
        cros_settings->GetProvider(chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_ != NULL);
    EXPECT_TRUE(
        cros_settings->RemoveSettingsProvider(device_settings_provider_));
    cros_settings->AddSettingsProvider(&stub_settings_provider_);
  }

  virtual ~KioskModeSettingsTest() {
    // Restore the real DeviceSettingsProvider.
    CrosSettings* cros_settings = CrosSettings::Get();
    EXPECT_TRUE(
      cros_settings->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings->AddSettingsProvider(device_settings_provider_);
  }

  virtual void SetUp() OVERRIDE {
    if (!KioskModeSettings::Get()->is_initialized()) {
      KioskModeSettings::Get()->Initialize(
          base::Bind(&KioskModeSettingsTest::SetUp,
                     base::Unretained(this)));
      return;
    }
  }

  virtual void TearDown() OVERRIDE {
    KioskModeSettings::Get()->set_initialized(false);
  }

  void ReInitialize() {
    KioskModeSettings::Get()->set_initialized(false);
    KioskModeSettings::Get()->Initialize(base::Bind(&base::DoNothing));
  }

  void DisableKioskModeSettings() {
    KioskModeSettings::Get()->set_initialized(false);
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;

  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
};

TEST_F(KioskModeSettingsTest, DisabledByDefault) {
  EXPECT_FALSE(KioskModeSettings::Get()->IsKioskModeEnabled());
}

TEST_F(KioskModeSettingsTest, InstanceAvailable) {
  EXPECT_TRUE(KioskModeSettings::Get() != NULL);
  EXPECT_TRUE(KioskModeSettings::Get()->is_initialized());
}

TEST_F(KioskModeSettingsTest, CheckLogoutTimeoutBounds) {
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();

  // Check if we go over max.
  cros_settings->SetInteger(
      kIdleLogoutTimeout,
      KioskModeSettings::kMaxIdleLogoutTimeout + kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutTimeout(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMaxIdleLogoutTimeout));

  // Check if we go under min.
  cros_settings->SetInteger(
      kIdleLogoutTimeout,
      KioskModeSettings::kMinIdleLogoutTimeout - kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutTimeout(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMinIdleLogoutTimeout));

  // Check if we are between max and min.
  cros_settings->SetInteger(
      kIdleLogoutTimeout,
      KioskModeSettings::kMaxIdleLogoutTimeout - kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutTimeout(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMaxIdleLogoutTimeout - kFudgeInt));
}

TEST_F(KioskModeSettingsTest, CheckLogoutWarningDurationBounds) {
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();

  // Check if we go over max.
  cros_settings->SetInteger(
      kIdleLogoutWarningDuration,
      KioskModeSettings::kMaxIdleLogoutWarningDuration + kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutWarningDuration(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMaxIdleLogoutWarningDuration));

  // Check if we go under min.
  cros_settings->SetInteger(
      kIdleLogoutWarningDuration,
      KioskModeSettings::kMinIdleLogoutWarningDuration - kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutWarningDuration(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMinIdleLogoutWarningDuration));

  // Check if we are between max and min.
  cros_settings->SetInteger(
      kIdleLogoutWarningDuration,
      KioskModeSettings::kMaxIdleLogoutWarningDuration - kFudgeInt);
  ReInitialize();
  EXPECT_EQ(KioskModeSettings::Get()->GetIdleLogoutWarningDuration(),
            base::TimeDelta::FromMilliseconds(
                KioskModeSettings::kMaxIdleLogoutWarningDuration - kFudgeInt));
}

TEST_F(KioskModeSettingsTest, UnitializedValues) {
  DisableKioskModeSettings();

  // Time delta initializes to '0' microseconds.
  EXPECT_LT(KioskModeSettings::Get()->GetScreensaverTimeout(),
            base::TimeDelta());
  EXPECT_LT(KioskModeSettings::Get()->GetIdleLogoutTimeout(),
            base::TimeDelta());
  EXPECT_LT(KioskModeSettings::Get()->GetIdleLogoutWarningDuration(),
            base::TimeDelta());
}

}  // namespace chromeos
