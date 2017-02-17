// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"

#include <set>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/views_delegate.h"

using chromeos::SelectToSpeakEventHandler;

namespace {

// Records all key events.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() {}
  ~EventCapturer() override {}

  void Reset() {
    last_key_event_.reset(nullptr);
    last_mouse_event_.reset(nullptr);
  }

  ui::KeyEvent* last_key_event() { return last_key_event_.get(); }
  ui::MouseEvent* last_mouse_event() { return last_mouse_event_.get(); }

 private:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_mouse_event_.reset(new ui::MouseEvent(*event));
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    last_key_event_.reset(new ui::KeyEvent(*event));
  }

  std::unique_ptr<ui::KeyEvent> last_key_event_;
  std::unique_ptr<ui::MouseEvent> last_mouse_event_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

class SelectToSpeakAccessibilityEventDelegate
    : public ash::test::TestAccessibilityEventDelegate {
 public:
  SelectToSpeakAccessibilityEventDelegate() {}
  ~SelectToSpeakAccessibilityEventDelegate() override {}

  void Reset() { events_captured_.clear(); }

  bool CapturedAXEvent(ui::AXEvent event_type) {
    return events_captured_.find(event_type) != events_captured_.end();
  }

  // Overriden from TestAccessibilityEventDelegate.
  void NotifyAccessibilityEvent(views::View* view,
                                ui::AXEvent event_type) override {
    events_captured_.insert(event_type);
  }

 private:
  std::set<ui::AXEvent> events_captured_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakAccessibilityEventDelegate);
};

class SelectToSpeakEventHandlerTest : public ash::test::AshTestBase {
 public:
  SelectToSpeakEventHandlerTest()
      : generator_(nullptr),
        select_to_speak_event_handler_(new SelectToSpeakEventHandler()) {}

  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    event_delegate_.reset(new SelectToSpeakAccessibilityEventDelegate());
    ash_test_helper()->views_delegate()->set_test_accessibility_event_delegate(
        event_delegate_.get());
    generator_ = &AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(select_to_speak_event_handler_.get());
    CurrentContext()->AddPreTargetHandler(&event_capturer_);
    AutomationManagerAura::GetInstance()->Enable(&profile_);
  }

  void TearDown() override {
    CurrentContext()->RemovePreTargetHandler(
        select_to_speak_event_handler_.get());
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    ash::test::AshTestBase::TearDown();
  }

 protected:
  ui::test::EventGenerator* generator_;
  EventCapturer event_capturer_;
  TestingProfile profile_;
  std::unique_ptr<SelectToSpeakAccessibilityEventDelegate> event_delegate_;

 private:
  std::unique_ptr<SelectToSpeakEventHandler> select_to_speak_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventHandlerTest);
};

}  // namespace

namespace chromeos {

TEST_F(SelectToSpeakEventHandlerTest, PressAndReleaseSearchNotHandled) {
  // If the user presses and releases the Search key, with no mouse
  // presses, the key events won't be handled by the SelectToSpeakEventHandler
  // and the normal behavior will occur.

  EXPECT_FALSE(event_capturer_.last_key_event());

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClick) {
  // If the user holds the Search key and then clicks the mouse button,
  // the mouse events and the key release event get hancled by the
  // SelectToSpeakEventHandler, and accessibility events are generated.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, RepeatSearchKey) {
  // Holding the Search key may generate key repeat events. Make sure it's
  // still treated as if the search key is down.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClickTwice) {
  // Same as SearchPlusClick, above, but test that the user can keep
  // holding down Search and click again.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  event_delegate_->Reset();
  EXPECT_FALSE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));
  EXPECT_FALSE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusMouseThenCancel) {
  // If the user holds the Search key and then presses the mouse button,
  // but then releases the Search key first while the mouse is still down,
  // a cancel AX event is sent and the subsequent mouse up is canceled too.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());

  EXPECT_TRUE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_CANCELED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusKeyIgnoresClicks) {
  // If the user presses the Search key and then some other key,
  // we should assume the user does not want select-to-speak, and
  // click events should be ignored.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->PressKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  ASSERT_TRUE(event_capturer_.last_mouse_event());
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());

  EXPECT_FALSE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  ASSERT_TRUE(event_capturer_.last_mouse_event());
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());

  EXPECT_FALSE(event_delegate_->CapturedAXEvent(ui::AX_EVENT_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, TappingControlStopsSpeech) {
  SpeechMonitor monitor;
  EXPECT_FALSE(monitor.DidStop());
  generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(monitor.DidStop());
}

TEST_F(SelectToSpeakEventHandlerTest, TappingSearchStopsSpeech) {
  SpeechMonitor monitor;
  EXPECT_FALSE(monitor.DidStop());
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(monitor.DidStop());
}

TEST_F(SelectToSpeakEventHandlerTest, TappingShiftDoesNotStopSpeech) {
  SpeechMonitor monitor;
  generator_->PressKey(ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN);
  generator_->ReleaseKey(ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(monitor.DidStop());
}

TEST_F(SelectToSpeakEventHandlerTest, PressingControlZDoesNotStopSpeech) {
  SpeechMonitor monitor;
  generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  generator_->PressKey(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
  generator_->ReleaseKey(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
  generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(monitor.DidStop());
}

}  // namespace chromeos
