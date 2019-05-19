// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/switch_access_event_handler.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// Records all key events for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() = default;
  ~EventCapturer() override = default;

  void Reset() { last_key_event_.reset(); }

  ui::KeyEvent* last_key_event() { return last_key_event_.get(); }

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    last_key_event_.reset(new ui::KeyEvent(*event));
  }

  std::unique_ptr<ui::KeyEvent> last_key_event_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

class TestDelegate : public mojom::SwitchAccessEventHandlerDelegate {
 public:
  TestDelegate() : binding_(this) {}
  ~TestDelegate() override = default;

  mojom::SwitchAccessEventHandlerDelegatePtr BindInterface() {
    mojom::SwitchAccessEventHandlerDelegatePtr ptr;
    binding_.Bind(MakeRequest(&ptr));
    return ptr;
  }

  ui::KeyEvent* last_key_event() { return key_events_.back().get(); }

 private:
  // SwitchAccessEventHandlerDelegate:
  void DispatchKeyEvent(std::unique_ptr<ui::Event> event) override {
    DCHECK(event->IsKeyEvent());
    key_events_.push_back(std::make_unique<ui::KeyEvent>(event->AsKeyEvent()));
  }

  mojo::Binding<mojom::SwitchAccessEventHandlerDelegate> binding_;
  std::vector<std::unique_ptr<ui::KeyEvent>> key_events_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SwitchAccessEventHandlerTest : public AshTestBase {
 public:
  SwitchAccessEventHandlerTest() = default;
  ~SwitchAccessEventHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
    delegate_ = std::make_unique<TestDelegate>();

    generator_ = AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(&event_capturer_);

    controller_ = Shell::Get()->accessibility_controller();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalAccessibilitySwitchAccess);
    controller_->SetSwitchAccessEnabled(true);
    controller_->SetSwitchAccessEventHandlerDelegate(
        delegate_->BindInterface());
    controller_->SetSwitchAccessIgnoreVirtualKeyEvent(false);
  }

  void TearDown() override {
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    AshTestBase::TearDown();
  }

  // Flush messages to the delegate before callers check its state.
  TestDelegate* GetDelegate() {
    controller_->FlushMojoForTest();
    return delegate_.get();
  }

 protected:
  ui::test::EventGenerator* generator_ = nullptr;
  EventCapturer event_capturer_;
  AccessibilityController* controller_ = nullptr;

 private:
  std::unique_ptr<TestDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandlerTest);
};

TEST_F(SwitchAccessEventHandlerTest, CaptureSpecifiedKeys) {
  Shell::Get()->accessibility_controller()->SetSwitchAccessKeysToCapture(
      {ui::VKEY_1, ui::VKEY_2, ui::VKEY_3});

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  ui::KeyEvent* key_event_1 = GetDelegate()->last_key_event();
  EXPECT_TRUE(key_event_1);

  // Press the "2" key.
  generator_->PressKey(ui::VKEY_2, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_2, ui::EF_NONE);

  // We received a new event.
  EXPECT_NE(GetDelegate()->last_key_event(), key_event_1);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  ui::KeyEvent* key_event_2 = GetDelegate()->last_key_event();
  EXPECT_TRUE(key_event_2);

  // Press the "3" key.
  generator_->PressKey(ui::VKEY_3, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_3, ui::EF_NONE);

  // We received a new event.
  EXPECT_NE(GetDelegate()->last_key_event(), key_event_2);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  ui::KeyEvent* key_event_3 = GetDelegate()->last_key_event();
  EXPECT_TRUE(key_event_3);
}

TEST_F(SwitchAccessEventHandlerTest, IgnoreKeysNotSpecifiedForCapture) {
  // Switch Access only captures select keys. Ignore all others.
  Shell::Get()->accessibility_controller()->SetSwitchAccessKeysToCapture(
      {ui::VKEY_1, ui::VKEY_2, ui::VKEY_3});

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "X" key.
  generator_->PressKey(ui::VKEY_X, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_X, ui::EF_NONE);

  // We received an event, but did not handle it.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SwitchAccessEventHandlerTest, KeysNoLongerCaptureAfterUpdate) {
  // Set Switch Access to capture the keys {1, 2, 3}.
  Shell::Get()->accessibility_controller()->SetSwitchAccessKeysToCapture(
      {ui::VKEY_1, ui::VKEY_2, ui::VKEY_3});

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  ui::KeyEvent* key_event_1 = GetDelegate()->last_key_event();
  EXPECT_TRUE(key_event_1);

  // Update the Switch Access keys to capture {2, 3, 4}.
  Shell::Get()->accessibility_controller()->SetSwitchAccessKeysToCapture(
      {ui::VKEY_2, ui::VKEY_3, ui::VKEY_4});

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // We received a new event.
  EXPECT_NE(event_capturer_.last_key_event(), key_event_1);

  // The event was NOT handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SwitchAccessEventHandlerTest, ForwardKeyEvents) {
  Shell::Get()->accessibility_controller()->SetSwitchAccessKeysToCapture(
      {ui::VKEY_1, ui::VKEY_2, ui::VKEY_3});

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Tell the Switch Access Event Handler to forward key events.
  Shell::Get()->accessibility_controller()->ForwardKeyEventsToSwitchAccess(
      true);

  // Press the "T" key.
  generator_->PressKey(ui::VKEY_T, ui::EF_NONE);

  // The event should be handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_TRUE(GetDelegate()->last_key_event());

  // Release the "T" key.
  generator_->ReleaseKey(ui::VKEY_T, ui::EF_NONE);

  // The event should be handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_TRUE(GetDelegate()->last_key_event());

  // Tell the Switch Access Event Handler to stop forwarding key events.
  Shell::Get()->accessibility_controller()->ForwardKeyEventsToSwitchAccess(
      false);

  // Press the "T" key.
  generator_->PressKey(ui::VKEY_T, ui::EF_NONE);

  // The release event is not handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());

  // Release the "T" key.
  generator_->ReleaseKey(ui::VKEY_T, ui::EF_NONE);
}

}  // namespace
}  // namespace ash
