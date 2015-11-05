// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager_chromeos.h"

#include "base/files/file_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_path_override.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "chromeos/chromeos_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/test/action_logger_util.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/chromeos/test/test_native_display_delegate.h"

namespace ash {

namespace {

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() { base::MessageLoop::current()->AddTaskObserver(this); }
  ~TaskObserver() override {
    base::MessageLoop::current()->RemoveTaskObserver(this);
  }

  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

// Run at least one task. WARNING: Will not return unless there is at least one
// task to be processed.
void RunBlockingPoolTask() {
  TaskObserver task_observer;
  base::RunLoop().Run();
}

}  // namespace

class DisplayColorManagerTest : public testing::Test {
 public:
  void SetUp() override {
    pool_owner_.reset(
        new base::SequencedWorkerPoolOwner(3, "DisplayColorManagerTest"));
    log_.reset(new ui::test::ActionLogger());

    native_display_delegate_ =
        new ui::test::TestNativeDisplayDelegate(log_.get());
    configurator_.SetDelegateForTesting(
        scoped_ptr<ui::NativeDisplayDelegate>(native_display_delegate_));

    color_manager_.reset(
        new DisplayColorManager(&configurator_, pool_owner_->pool().get()));

    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &color_path_));

    color_path_ = color_path_.Append(FILE_PATH_LITERAL("ash"))
                      .Append(FILE_PATH_LITERAL("display"))
                      .Append(FILE_PATH_LITERAL("test_data"));
    path_override_.reset(new base::ScopedPathOverride(
        chromeos::DIR_DEVICE_COLOR_CALIBRATION_PROFILES, color_path_));
  }

  void TearDown() override { pool_owner_->pool()->Shutdown(); }

  DisplayColorManagerTest() : test_api_(&configurator_) {}
  ~DisplayColorManagerTest() override {}

 protected:
  scoped_ptr<base::ScopedPathOverride> path_override_;
  base::FilePath color_path_;
  scoped_ptr<ui::test::ActionLogger> log_;
  ui::DisplayConfigurator configurator_;
  ui::DisplayConfigurator::TestApi test_api_;
  ui::test::TestNativeDisplayDelegate* native_display_delegate_;  // not owned
  scoped_ptr<DisplayColorManager> color_manager_;

  base::MessageLoopForUI ui_message_loop_;
  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayColorManagerTest);
};

TEST_F(DisplayColorManagerTest, VCGTOnly) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  scoped_ptr<ui::TestDisplaySnapshot> snapshot =
      make_scoped_ptr(new ui::TestDisplaySnapshot(
          123, gfx::Point(0, 0),                /* origin */
          gfx::Size(0, 0),                      /* physical_size */
          ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
          false,      /* is_aspect_preserving_scaling */
          0x06af5c10, /* product_id */
          modes,      /* modes */
          modes[0] /* current_mode */,
          modes[0] /* native_mode */));
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(snapshot.get());
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  log_->GetActionsAndClear();

  RunBlockingPoolTask();

  EXPECT_TRUE(base::MatchPattern(
      log_->GetActionsAndClear(),
      "set_gamma_ramp(id=123,rgb[0]*rgb[255]=???????????\?)"));
}

TEST_F(DisplayColorManagerTest, NoMatchProductID) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  scoped_ptr<ui::TestDisplaySnapshot> snapshot =
      make_scoped_ptr(new ui::TestDisplaySnapshot(
          123, gfx::Point(0, 0),                /* origin */
          gfx::Size(0, 0),                      /* physical_size */
          ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
          false, /* is_aspect_preserving_scaling */
          0,     /* product_id */
          modes, /* modes */
          modes[0] /* current_mode */,
          modes[0] /* native_mode */));
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(snapshot.get());
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  log_->GetActionsAndClear();
  RunBlockingPoolTask();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

TEST_F(DisplayColorManagerTest, NoVCGT) {
  std::vector<const ui::DisplayMode*> modes;
  ui::DisplayMode mode(gfx::Size(1024, 768), false, 60.0f);
  modes.push_back(&mode);
  scoped_ptr<ui::TestDisplaySnapshot> snapshot =
      make_scoped_ptr(new ui::TestDisplaySnapshot(
          123, gfx::Point(0, 0),                /* origin */
          gfx::Size(0, 0),                      /* physical_size */
          ui::DISPLAY_CONNECTION_TYPE_INTERNAL, /* type */
          false,      /* is_aspect_preserving_scaling */
          0x0dae3211, /* product_id */
          modes,      /* modes */
          modes[0] /* current_mode */,
          modes[0] /* native_mode */));
  std::vector<ui::DisplaySnapshot*> outputs;
  outputs.push_back(snapshot.get());
  native_display_delegate_->set_outputs(outputs);

  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  log_->GetActionsAndClear();
  RunBlockingPoolTask();
  EXPECT_STREQ("", log_->GetActionsAndClear().c_str());
}

}  // namespace ash
