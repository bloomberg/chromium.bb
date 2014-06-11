// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/public/accelerator_manager.h"

#include "athena/input/public/input_manager.h"
#include "athena/test/athena_test_base.h"
#include "ui/aura/test/event_generator.h"

namespace athena {
namespace {

const int kInvalidCommandId = -1;

class TestHandler : public AcceleratorHandler {
 public:
  TestHandler() : fired_command_id_(kInvalidCommandId), enabled_(true) {}
  virtual ~TestHandler() {}

  void set_enabled(bool enabled) { enabled_ = enabled; }

  int GetFiredCommandIdAndReset() {
    int fired = fired_command_id_;
    fired_command_id_ = kInvalidCommandId;
    return fired;
  }

 private:
  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE {
    return enabled_;
  }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    fired_command_id_ = command_id;
    return true;
  }

  int fired_command_id_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestHandler);
};

}  // namespace athena

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

  aura::test::EventGenerator generator(root_window());
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

}  // namespace athena
