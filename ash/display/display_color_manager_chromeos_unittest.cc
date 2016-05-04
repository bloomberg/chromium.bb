// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager_chromeos.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_path_override.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "chromeos/chromeos_paths.h"
#include "components/quirks/quirks_manager.h"
#include "components/quirks/switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/test/action_logger_util.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/chromeos/test/test_native_display_delegate.h"

namespace ash {

namespace {

const char kResetGammaAction[] = "*set_color_correction(id=123)";
const char kSetGammaAction[] =
    "set_color_correction(id=123,gamma[0]*gamma[255]=???????????\?)";
const char kSetFullCTMAction[] =
    "set_color_correction(id=123,degamma[0]*gamma[0]*ctm[0]*ctm[8]*)";

class DisplayColorManagerForTest : public DisplayColorManager {
 public:
  DisplayColorManagerForTest(ui::DisplayConfigurator* configurator,
                             base::SequencedWorkerPool* blocking_pool)
      : DisplayColorManager(configurator, blocking_pool) {}

  void SetOnFinishedForTest(base::Closure on_finished_for_test) {
    on_finished_for_test_ = on_finished_for_test;
  }

 private:
  void FinishLoadCalibrationForDisplay(int64_t display_id,
                                       int64_t product_id,
                                       bool has_color_correction_matrix,
                                       ui::DisplayConnectionType type,
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

  base::FilePath GetBuiltInDisplayProfileDirectory() const override {
    return color_path_;
  }

  // Unused by these tests.
  base::FilePath GetDownloadDisplayProfileDirectory() const override {
    return base::FilePath();
  }

  // Unused by these tests.
  bool DevicePolicyEnabled() const override { return true; }

  // Unused by these tests.
  void GetDaysSinceOobe(
      quirks::QuirksManager::DaysSinceOobeCallback callback) const override {}

 private:
  ~QuirksManagerDelegateTestImpl() override = default;

  base::FilePath color_path_;

  DISALLOW_COPY_AND_ASSIGN(QuirksManagerDelegateTestImpl);
};

}  // namespace

class DisplayColorManagerTest : public testing::Test {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        quirks::switches::kDisableQuirksClient);

    pool_owner_.reset(
        new base::SequencedWorkerPoolOwner(3, "DisplayColorManagerTest"));
    log_.reset(new ui::test::ActionLogger());

    native_display_delegate_ =
        new ui::test::TestNativeDisplayDelegate(log_.get());
    configurator_.SetDelegateForTesting(
        std::unique_ptr<ui::NativeDisplayDelegate>(native_display_delegate_));

    color_manager_.reset(new DisplayColorManagerForTest(
        &configurator_, pool_owner_->pool().get()));

    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &color_path_));

    color_path_ = color_path_.Append(FILE_PATH_LITERAL("ash"))
                      .Append(FILE_PATH_LITERAL("display"))
                      .Append(FILE_PATH_LITERAL("test_data"));
    path_override_.reset(new base::ScopedPathOverride(
        chromeos::DIR_DEVICE_COLOR_CALIBRATION_PROFILES, color_path_));

    quirks::QuirksManager::Initialize(
        std::unique_ptr<quirks::QuirksManager::Delegate>(
            new QuirksManagerDelegateTestImpl(color_path_)),
        pool_owner_->pool().get(), nullptr, nullptr);
  }

  void TearDown() override {
    quirks::QuirksManager::Shutdown();
    pool_owner_->pool()->Shutdown();
  }

  void WaitOnColorCalibration() {
    base::RunLoop run_loop;
    color_manager_->SetOnFinishedForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  DisplayColorManagerTest() : test_api_(&configurator_) {}
  ~DisplayColorManagerTest() override {}

 protected:
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::FilePath color_path_;
  std::unique_ptr<ui::test::ActionLogger> log_;
  ui::DisplayConfigurator configurator_;
  ui::DisplayConfigurator::TestApi test_api_;
  ui::test::TestNativeDisplayDelegate* native_display_delegate_;  // not owned
  std::unique_ptr<DisplayColorManagerForTest> color_manager_;

  base::MessageLoopForUI ui_message_loop_;
  std::unique_ptr<base::SequencedWorkerPoolOwner> pool_owner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayColorManagerTest);
};

TEST_F(DisplayColorManagerTest, VCGTOnly) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0x06af5c10,                           /* product_id */
      false,                                /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_TRUE(base::MatchPattern(log_->GetActionsAndClear(), kSetGammaAction));
}

TEST_F(DisplayColorManagerTest, VCGTOnlyWithPlatformCTM) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0x06af5c10,                           /* product_id */
      true,                                 /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
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
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0x4c834a42,                           /* product_id */
      true,                                 /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
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
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0x4c834a42,                           /* product_id */
      false,                                /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoMatchProductID) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0,                                    /* product_id */
      false,                                /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoVCGT) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  ui::TestDisplaySnapshot snapshot(
      123, gfx::Point(0, 0),                /* origin */
      gfx::Size(0, 0),                      /* physical_size */
      ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
      false,                                /* is_aspect_preserving_scaling */
      0x0dae3211,                           /* product_id */
      false,                                /* has_color_correction_matrix */
      modes,                                /* modes */
      modes[0] /* current_mode */, modes[0] /* native_mode */);
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(&snapshot);
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_TRUE(
      base::MatchPattern(log_->GetActionsAndClear(), kResetGammaAction));

  WaitOnColorCalibration();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

}  // namespace ash
