// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/chromeos_paths.h"
#include "components/quirks/quirks_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/chromeos/test/action_logger_util.h"
#include "ui/display/manager/chromeos/test/test_native_display_delegate.h"
#include "ui/display/manager/fake_display_snapshot.h"

namespace ash {

namespace {

constexpr gfx::Size kDisplaySize(1024, 768);
const char kResetGammaAction[] = "*set_color_correction(id=123)";
const char kSetGammaAction[] =
    "set_color_correction(id=123,gamma[0]*gamma[255]=???????????\?)";
const char kSetFullCTMAction[] =
    "set_color_correction(id=123,degamma[0]*gamma[0]*ctm[0]*ctm[8]*)";

class DisplayColorManagerForTest : public DisplayColorManager {
 public:
  explicit DisplayColorManagerForTest(
      display::DisplayConfigurator* configurator)
      : DisplayColorManager(configurator) {}

  void SetOnFinishedForTest(base::Closure on_finished_for_test) {
    on_finished_for_test_ = on_finished_for_test;
  }

 private:
  void FinishLoadCalibrationForDisplay(int64_t display_id,
                                       int64_t product_id,
                                       bool has_color_correction_matrix,
                                       display::DisplayConnectionType type,
                                       const base::FilePath& path,
                                       bool file_downloaded) override {
    DisplayColorManager::FinishLoadCalibrationForDisplay(
        display_id, product_id, has_color_correction_matrix, type, path,
        file_downloaded);
    // If path is empty, there is no icc file, and the DCM's work is done.
    if (path.empty() && !on_finished_for_test_.is_null()) {
      on_finished_for_test_.Run();
      on_finished_for_test_.Reset();
    }
  }

  void UpdateCalibrationData(
      int64_t display_id,
      int64_t product_id,
      std::unique_ptr<ColorCalibrationData> data) override {
    DisplayColorManager::UpdateCalibrationData(display_id, product_id,
                                               std::move(data));
    if (!on_finished_for_test_.is_null()) {
      on_finished_for_test_.Run();
      on_finished_for_test_.Reset();
    }
  }

  base::Closure on_finished_for_test_;

  DISALLOW_COPY_AND_ASSIGN(DisplayColorManagerForTest);
};

// Implementation of QuirksManager::Delegate to fake chrome-restricted parts.
class QuirksManagerDelegateTestImpl : public quirks::QuirksManager::Delegate {
 public:
  QuirksManagerDelegateTestImpl(base::FilePath color_path)
      : color_path_(color_path) {}

  // Unused by these tests.
  std::string GetApiKey() const override { return std::string(); }

  base::FilePath GetDisplayProfileDirectory() const override {
    return color_path_;
  }

  bool DevicePolicyEnabled() const override { return true; }

 private:
  ~QuirksManagerDelegateTestImpl() override = default;

  base::FilePath color_path_;

  DISALLOW_COPY_AND_ASSIGN(QuirksManagerDelegateTestImpl);
};

}  // namespace

class DisplayColorManagerTest : public testing::Test {
 public:
  void SetUp() override {
    log_.reset(new display::test::ActionLogger());

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(log_.get());
    configurator_.SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));

    color_manager_ =
        std::make_unique<DisplayColorManagerForTest>(&configurator_);

    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &color_path_));

    color_path_ = color_path_.Append(FILE_PATH_LITERAL("ash"))
                      .Append(FILE_PATH_LITERAL("display"))
                      .Append(FILE_PATH_LITERAL("test_data"));
    path_override_.reset(new base::ScopedPathOverride(
        chromeos::DIR_DEVICE_DISPLAY_PROFILES, color_path_));

    quirks::QuirksManager::Initialize(
        std::unique_ptr<quirks::QuirksManager::Delegate>(
            new QuirksManagerDelegateTestImpl(color_path_)),
        nullptr, nullptr);
  }

  void TearDown() override {
    quirks::QuirksManager::Shutdown();
  }

  void WaitOnColorCalibration() {
    base::RunLoop run_loop;
    color_manager_->SetOnFinishedForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  DisplayColorManagerTest() : test_api_(&configurator_) {}
  ~DisplayColorManagerTest() override = default;

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::FilePath color_path_;
  std::unique_ptr<display::test::ActionLogger> log_;
  display::DisplayConfigurator configurator_;
  display::DisplayConfigurator::TestApi test_api_;
  display::test::TestNativeDisplayDelegate*
      native_display_delegate_;  // not owned
  std::unique_ptr<DisplayColorManagerForTest> color_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayColorManagerTest);
};

TEST_F(DisplayColorManagerTest, VCGTOnly) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x06af5c10)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, VCGTOnlyWithPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x06af5c10)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  log_->GetActionsAndClear();
  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, FullWithPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(true)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kSetFullCTMAction));
}

TEST_F(DisplayColorManagerTest, FullWithoutPlatformCTM) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x4c834a42)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoMatchProductID) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  // NOTE: If product_id == 0, there is no thread switching in Quirks or Display
  // code, so we shouldn't call WaitOnColorCalibration().
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoVCGT) {
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(kDisplaySize)
          .SetCurrentMode(kDisplaySize)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetHasColorCorrectionMatrix(false)
          .SetProductCode(0x0dae3211)
          .Build();
  std::vector<display::DisplaySnapshot*> outputs({snapshot.get()});
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

}  // namespace ash
