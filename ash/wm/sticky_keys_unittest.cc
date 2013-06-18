// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/sticky_keys.h"

#include <X11/Xlib.h>
#undef None
#undef Bool

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_vector.h"
#include "ui/base/x/x11_util.h"

namespace ash {

// A stub implementation of EventTarget.
class StubEventTarget : public ui::EventTarget {
 public:
  StubEventTarget() {}
  virtual ~StubEventTarget() {}

  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE {
    return true;
  }

  virtual EventTarget* GetParentTarget() OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubEventTarget);
};

// A testable and StickyKeysHandler.
class MockStickyKeysHandlerDelegate :
    public StickyKeysHandler::StickyKeysHandlerDelegate {
 public:
  MockStickyKeysHandlerDelegate() {}
  virtual ~MockStickyKeysHandlerDelegate() {}

  // StickyKeysHandler override.
  virtual void DispatchKeyEvent(ui::KeyEvent* event,
                                aura::Window* target) OVERRIDE {
    key_events_.push_back(event->Copy());
  }

  // Returns the count of dispatched keyboard event.
  size_t GetKeyEventCount() const {
    return key_events_.size();
  }

  // Returns the |index|-th dispatched keyboard event.
  const ui::KeyEvent* GetKeyEvent(size_t index) const {
    return key_events_[index];
  }

 private:
  ScopedVector<ui::KeyEvent> key_events_;

  DISALLOW_COPY_AND_ASSIGN(MockStickyKeysHandlerDelegate);
};

class StickyKeysTest : public test::AshTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    target_.reset(new StubEventTarget());
    test::AshTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    test::AshTestBase::TearDown();
    target_.reset();
  }

  // Generates keyboard event.
  ui::KeyEvent* GenerateKey(bool is_key_press, ui::KeyboardCode code) {
    XEvent* xev = new XEvent();
    ui::InitXKeyEventForTesting(
        is_key_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
        code,
        0,
        xev);
    xevs_.push_back(xev);
    ui::KeyEvent* event =  new ui::KeyEvent(xev, false);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_.get());
    return event;
  }

 private:
  scoped_ptr<StubEventTarget> target_;
  ScopedVector<XEvent> xevs_;
};

TEST_F(StickyKeysTest, BasicOneshotScenarioTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate();
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  // By typing Shift key, internal state become ENABLED.
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());

  // Next keyboard event is shift modified.
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  // Making sure Shift up keyboard event is dispatched.
  ASSERT_EQ(2U, mock_delegate->GetKeyEventCount());
  EXPECT_EQ(ui::VKEY_A, mock_delegate->GetKeyEvent(0)->key_code());
  EXPECT_EQ(ui::ET_KEY_PRESSED, mock_delegate->GetKeyEvent(0)->type());
  EXPECT_EQ(ui::VKEY_SHIFT, mock_delegate->GetKeyEvent(1)->key_code());
  EXPECT_EQ(ui::ET_KEY_RELEASED, mock_delegate->GetKeyEvent(1)->type());

  // Enabled state is one shot, so next key event should not be shift modified.
  ev.reset(GenerateKey(true, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_FALSE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_FALSE(ev->flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(StickyKeysTest, BasicLockedScenarioTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate();
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  // By typing shift key, internal state become ENABLED.
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  // By typing shift key again, internal state become LOCKED.
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  // All keyboard events including keyUp become shift modified.
  ev.reset(GenerateKey(true, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  // Locked state keeps after normal keyboard event.
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_B));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_B));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());

  // By typing shift key again, internal state become back to DISABLED.
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NonTargetModifierTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate();
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(false, ui::VKEY_SHIFT));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());
}

}  // namespace ash
