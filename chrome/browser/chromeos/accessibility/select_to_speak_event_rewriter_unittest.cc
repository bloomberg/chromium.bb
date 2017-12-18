// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_rewriter.h"

#include <set>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/events/event_rewriter_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

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
    : public SelectToSpeakEventDelegateForTesting {
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

class SelectToSpeakEventRewriterTest : public ash::AshTestBase {
 public:
  SelectToSpeakEventRewriterTest() : generator_(nullptr) {}

  void SetUp() override {
    ash::AshTestBase::SetUp();
    select_to_speak_event_rewriter_.reset(
        new SelectToSpeakEventRewriter(CurrentContext()));
    mouse_event_delegate_.reset(new SelectToSpeakMouseEventDelegate());
    select_to_speak_event_rewriter_->CaptureForwardedEventsForTesting(
        mouse_event_delegate_.get());
    generator_ = &AshTestBase::GetEventGenerator();
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        select_to_speak_event_rewriter_.get());
    CurrentContext()->AddPreTargetHandler(&event_capturer_);
    AutomationManagerAura::GetInstance()->Enable(&profile_);
  }

  void TearDown() override {
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        select_to_speak_event_rewriter_.get());
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
  std::unique_ptr<SelectToSpeakEventRewriter> select_to_speak_event_rewriter_;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventRewriterTest);
};

}  // namespace

TEST_F(SelectToSpeakEventRewriterTest, PressAndReleaseSearchNotHandled) {
  // If the user presses and releases the Search key, with no mouse
  // presses, the key events won't be handled by the SelectToSpeakEventRewriter
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
TEST_F(SelectToSpeakEventRewriterTest, SearchPlusClick) {
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

TEST_F(SelectToSpeakEventRewriterTest, SearchPlusDrag) {
  // Mouse move events should also be captured.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_location(click_location);
  generator_->PressLeftButton();
  EXPECT_EQ(click_location, mouse_event_delegate_->last_mouse_event_location());

  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location);
  EXPECT_EQ(drag_location, mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  generator_->ReleaseLeftButton();
  EXPECT_EQ(drag_location, mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventRewriterTest, SearchPlusDragOnLargeDisplay) {
  // This display has twice the number of pixels per DIP. THis means that
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
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  generator_->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            mouse_event_delegate_->last_mouse_event_location());
  EXPECT_TRUE(mouse_event_delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventRewriterTest, RepeatSearchKey) {
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

TEST_F(SelectToSpeakEventRewriterTest, TapSearchKey) {
  // Tapping the search key should not steal future events.

  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
}

TEST_F(SelectToSpeakEventRewriterTest, SearchPlusClickTwice) {
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

TEST_F(SelectToSpeakEventRewriterTest, SearchPlusKeyIgnoresClicks) {
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
