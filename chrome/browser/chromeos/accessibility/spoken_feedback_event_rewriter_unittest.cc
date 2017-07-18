// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

// Records all key events.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() {}
  ~EventCapturer() override {}

  void Reset() { events_.clear(); }

  void OnEvent(ui::Event* event) override {
    if (event->IsKeyEvent())
      events_.push_back(base::MakeUnique<ui::KeyEvent>(*event->AsKeyEvent()));
  }

  const std::vector<std::unique_ptr<ui::KeyEvent>>& captured_events() const {
    return events_;
  }

 private:
  std::vector<std::unique_ptr<ui::KeyEvent>> events_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

class TestDelegate : public SpokenFeedbackEventRewriterDelegate {
 public:
  TestDelegate()
      : is_spoken_feedback_enabled_(false), dispatch_result_(false) {}

  ~TestDelegate() override {}

  void set_is_spoken_feedback_enabled(bool enabled) {
    is_spoken_feedback_enabled_ = enabled;
  }

  void set_dispatch_result(bool result) { dispatch_result_ = result; }

 private:
  // SpokenFeedbackEventRewriterDelegate:
  bool IsSpokenFeedbackEnabled() const override {
    return is_spoken_feedback_enabled_;
  }

  bool DispatchKeyEventToChromeVox(const ui::KeyEvent& key_event,
                                   bool capture) override {
    return dispatch_result_;
  }

  bool is_spoken_feedback_enabled_;
  bool dispatch_result_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SpokenFeedbackEventRewriterTest : public ash::AshTestBase {
 public:
  SpokenFeedbackEventRewriterTest()
      : generator_(nullptr),
        spoken_feedback_event_rewriter_(new SpokenFeedbackEventRewriter()) {
    delegate_ = new TestDelegate();
    spoken_feedback_event_rewriter_->SetDelegateForTest(
        std::unique_ptr<TestDelegate>(delegate_));
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    generator_ = &AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(&event_capturer_);
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        spoken_feedback_event_rewriter_.get());
  }

  void TearDown() override {
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        spoken_feedback_event_rewriter_.get());
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

 protected:
  TestDelegate* delegate_;
  ui::test::EventGenerator* generator_;
  EventCapturer event_capturer_;

 private:
  std::unique_ptr<SpokenFeedbackEventRewriter> spoken_feedback_event_rewriter_;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriterTest);
};

TEST_F(SpokenFeedbackEventRewriterTest, KeysNotEatenWithChromeVoxDisabled) {
  // Send Search+Shift+Right.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_EQ(1U, event_capturer_.captured_events().size());
  generator_->PressKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_EQ(2U, event_capturer_.captured_events().size());

  // Mock successful commands lookup and dispatch; shouldn't matter either way.
  delegate_->set_dispatch_result(true);
  generator_->PressKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  ASSERT_EQ(3U, event_capturer_.captured_events().size());

  // Released keys shouldn't get eaten.
  delegate_->set_dispatch_result(false);
  generator_->ReleaseKey(ui::VKEY_RIGHT,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  generator_->ReleaseKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  ASSERT_EQ(6U, event_capturer_.captured_events().size());

  // Try releasing more keys.
  generator_->ReleaseKey(ui::VKEY_RIGHT, 0);
  generator_->ReleaseKey(ui::VKEY_SHIFT, 0);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  ASSERT_EQ(9U, event_capturer_.captured_events().size());
}
