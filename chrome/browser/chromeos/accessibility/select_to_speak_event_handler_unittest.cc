// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"

#include <set>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/chromeos/events/event_rewriter_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

using chromeos::SelectToSpeakEventHandler;

namespace {

// Records all key events for testing.
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

class SelectToSpeakMouseEventDelegate final
    : public chromeos::SelectToSpeakEventDelegateForTesting {
 public:
  SelectToSpeakMouseEventDelegate() {}

  void Reset() {
    events_captured_.clear();
    last_mouse_location_.SetPoint(0, 0);
  }

  bool CapturedMouseEvent(ui::EventType event_type) {
    return events_captured_.find(event_type) != events_captured_.end();
  }

  gfx::Point last_mouse_event_location() { return last_mouse_location_; }

  // Overriden from SelectToSpeakForwardedEventDelegateForTesting
  void OnForwardEventToSelectToSpeakExtension(
      const ui::MouseEvent& event) override {
    last_mouse_location_ = event.location();
    events_captured_.insert(event.type());
  }

 private:
  std::set<ui::EventType> events_captured_;
  gfx::Point last_mouse_location_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakMouseEventDelegate);
};

class SelectToSpeakEventHandlerTest : public ash::AshTestBase {
 public:
  SelectToSpeakEventHandlerTest() : generator_(nullptr) {}

  void SetUp() override {
    ash::AshTestBase::SetUp();
    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
    select_to_speak_event_handler_.reset(new SelectToSpeakEventHandler());
    mouse_event_delegate_.reset(new SelectToSpeakMouseEventDelegate());
    select_to_speak_event_handler_->CaptureForwardedEventsForTesting(
        mouse_event_delegate_.get());
    generator_ = &AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(&event_capturer_);
    AutomationManagerAura::GetInstance()->Enable(&profile_);
  }

  void TearDown() override {
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

 protected:
  ui::test::EventGenerator* generator_;
  EventCapturer event_capturer_;
  TestingProfile profile_;
  std::unique_ptr<SelectToSpeakMouseEventDelegate> mouse_event_delegate_;

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

// Note: when running these tests locally on desktop Linux, you may need
// to use xvfb-run, otherwise simulating the Search key press may not work.
TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClick) {
  // If the user holds the Search key and then clicks the mouse button,
  // the mouse events and the key release event get handled by the
  // SelectToSpeakEventRewriter, and mouse events are forwarded to the
  // extension.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(click_location, mouse_event_delegate_->last_mouse_event_location());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));
  EXPECT_EQ(click_location, mouse_event_delegate_->last_mouse_event_location());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusDrag) {
  // Mouse move events should also be captured.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_location(click_location);
  generator_->PressLeftButton();
  EXPECT_EQ(click_location, mouse_event_delegate_->last_mouse_event_location());

  // Drags are not blocked.
  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location);
  EXPECT_EQ(drag_location, mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(drag_location, mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusDragOnLargeDisplay) {
  // This display has twice the number of pixels per DIP. This means that
  // each event coming in in px needs to be divided by two to be converted
  // to DIPs.
  UpdateDisplay("800x600*2");

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location_px = gfx::Point(100, 12);
  generator_->set_current_location(click_location_px);
  generator_->PressLeftButton();
  EXPECT_EQ(gfx::Point(click_location_px.x() / 2, click_location_px.y() / 2),
            mouse_event_delegate_->last_mouse_event_location());

  gfx::Point drag_location_px = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location_px);
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, RepeatSearchKey) {
  // Holding the Search key may generate key repeat events. Make sure it's
  // still treated as if the search key is down.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->set_current_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, TapSearchKey) {
  // Tapping the search key should not steal future events.

  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
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
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  mouse_event_delegate_->Reset();
  EXPECT_FALSE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  EXPECT_FALSE(
      mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
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

  EXPECT_FALSE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  ASSERT_TRUE(event_capturer_.last_mouse_event());
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());

  EXPECT_FALSE(
      mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIsCaptured) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press and release S, key presses should be captured.
  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // Press and release again while still holding down search.
  // The events should continue to be captured.
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // S alone is not captured
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  ASSERT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIgnoresMouse) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press S
  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // Mouse events are passed through like normal.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusMouseIgnoresS) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press the mouse
  event_capturer_.Reset();
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  // S key events are passed through like normal.
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());
  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

}  // namespace chromeos
