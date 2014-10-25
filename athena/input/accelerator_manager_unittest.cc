// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/public/accelerator_manager.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/input/public/input_manager.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/wm/public/window_manager.h"
#include "ui/events/test/event_generator.h"

namespace athena {
namespace {

const int kInvalidCommandId = -1;

class TestHandler : public AcceleratorHandler {
 public:
  TestHandler() : fired_command_id_(kInvalidCommandId), enabled_(true) {}
  ~TestHandler() override {}

  void set_enabled(bool enabled) { enabled_ = enabled; }

  int GetFiredCommandIdAndReset() {
    int fired = fired_command_id_;
    fired_command_id_ = kInvalidCommandId;
    return fired;
  }

 private:
  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const override {
    return enabled_;
  }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) override {
    fired_command_id_ = command_id;
    return true;
  }

  int fired_command_id_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestHandler);
};

}  // namespace

typedef test::AthenaTestBase InputManagerTest;

TEST_F(InputManagerTest, Basic) {
  enum TestCommandId {
    COMMAND_A,
    COMMAND_B,
    COMMAND_C,
    COMMAND_D,
    COMMAND_E,
  };
  const AcceleratorData data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_A, ui::EF_SHIFT_DOWN, COMMAND_A, AF_RESERVED},
      {TRIGGER_ON_RELEASE, ui::VKEY_B, ui::EF_SHIFT_DOWN, COMMAND_B,
       AF_RESERVED},
      {TRIGGER_ON_PRESS, ui::VKEY_C, ui::EF_SHIFT_DOWN, COMMAND_C,
       AF_RESERVED | AF_DEBUG},
      {TRIGGER_ON_PRESS, ui::VKEY_D, ui::EF_SHIFT_DOWN, COMMAND_D,
       AF_RESERVED | AF_NON_AUTO_REPEATABLE},
      {TRIGGER_ON_PRESS, ui::VKEY_E, ui::EF_SHIFT_DOWN, COMMAND_E, AF_NONE},
  };
  AcceleratorManager* accelerator_manager =
      InputManager::Get()->GetAcceleratorManager();
  TestHandler test_handler;
  accelerator_manager->RegisterAccelerators(
      data, arraysize(data), &test_handler);

  ui::test::EventGenerator generator(root_window());
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());

  // Trigger on press.
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_A, test_handler.GetFiredCommandIdAndReset());
  generator.ReleaseKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());

  // Trigger on release.
  generator.PressKey(ui::VKEY_B, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());
  generator.ReleaseKey(ui::VKEY_B, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_B, test_handler.GetFiredCommandIdAndReset());

  // Disable command.
  test_handler.set_enabled(false);
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());
  test_handler.set_enabled(true);
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_A, test_handler.GetFiredCommandIdAndReset());

  // Debug accelerators.
  accelerator_manager->SetDebugAcceleratorsEnabled(false);
  generator.PressKey(ui::VKEY_C, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());
  accelerator_manager->SetDebugAcceleratorsEnabled(true);
  generator.PressKey(ui::VKEY_C, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_C, test_handler.GetFiredCommandIdAndReset());
  accelerator_manager->SetDebugAcceleratorsEnabled(false);

  // Non auto repeatable
  generator.PressKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_D, test_handler.GetFiredCommandIdAndReset());
  generator.PressKey(ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_IS_REPEAT);
  EXPECT_EQ(kInvalidCommandId, test_handler.GetFiredCommandIdAndReset());

  // TODO(oshima): Add scenario where the key event is consumed by
  // an app.
  generator.PressKey(ui::VKEY_E, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(COMMAND_E, test_handler.GetFiredCommandIdAndReset());
}

TEST_F(InputManagerTest, CloseActivity) {
  ActivityFactory* factory = ActivityFactory::Get();
  Activity* activity1 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());
  Activity::Show(activity1);
  Activity::Delete(activity1);

  Activity* activity2 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());
  Activity::Show(activity2);

  // TODO(oshima): This shouldn't be necessary. Remove this once
  // crbug.com/427113 is fixed.
  RunAllPendingInMessageLoop();

  ui::test::EventGenerator generator(root_window());
  generator.PressKey(ui::VKEY_F6, ui::EF_NONE);
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  Activity::Delete(activity2);
}

}  // namespace athena
