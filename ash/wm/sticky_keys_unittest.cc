// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/sticky_keys.h"

#include <X11/Xlib.h>
#undef None
#undef Bool

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"
#include "ui/events/x/events_x_utils.h"

namespace ash {

// A testable and StickyKeysHandler.
class MockStickyKeysHandlerDelegate :
    public StickyKeysHandler::StickyKeysHandlerDelegate {
 public:
  class Delegate {
   public:
    virtual aura::Window* GetExpectedTarget() = 0;
    virtual void OnShortcutPressed() = 0;

   protected:
    virtual ~Delegate() {}
  };

  MockStickyKeysHandlerDelegate(Delegate* delegate) : delegate_(delegate) {}

  virtual ~MockStickyKeysHandlerDelegate() {}

  // StickyKeysHandler override.
  virtual void DispatchKeyEvent(ui::KeyEvent* event,
                                aura::Window* target) OVERRIDE {
    ASSERT_EQ(delegate_->GetExpectedTarget(), target);

    // Detect a special shortcut when it is dispatched. This shortcut will
    // not be hit in the LOCKED state as this case does not involve the
    // delegate.
    if (event->type() == ui::ET_KEY_PRESSED &&
        event->key_code() == ui::VKEY_J &&
        event->flags() | ui::EF_CONTROL_DOWN) {
      delegate_->OnShortcutPressed();
    }

    events_.push_back(event->Copy());
  }

  virtual void DispatchMouseEvent(ui::MouseEvent* event,
                                  aura::Window* target) OVERRIDE {
    ASSERT_EQ(delegate_->GetExpectedTarget(), target);
    events_.push_back(new ui::MouseEvent(event->native_event()));
  }

  // Returns the count of dispatched events.
  size_t GetEventCount() const {
    return events_.size();
  }

  // Returns the |index|-th dispatched event.
  const ui::Event* GetEvent(size_t index) const {
    return events_[index];
  }

 private:
  ScopedVector<ui::Event> events_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockStickyKeysHandlerDelegate);
};

class StickyKeysTest : public test::AshTestBase,
                       public MockStickyKeysHandlerDelegate::Delegate {
 protected:
  StickyKeysTest()
      : target_(NULL),
        root_window_(NULL) {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();

    // |target_| owned by root window of shell. It is still safe to delete
    // it ourselves.
    target_ = CreateTestWindowInShellWithId(0);
    root_window_ = target_->GetRootWindow();
  }

  virtual void TearDown() OVERRIDE {
    test::AshTestBase::TearDown();
  }

  // Overridden from MockStickyKeysHandlerDelegate::Delegate:
  virtual aura::Window* GetExpectedTarget() OVERRIDE {
    return target_ ? target_ : root_window_;
  }

  virtual void OnShortcutPressed() OVERRIDE {
    if (target_) {
      delete target_;
      target_ = NULL;
    }
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
    dispatcher.set_target(target_);
    return event;
  }

  ui::MouseEvent* GenerateMouseEvent(bool is_button_press) {
    XEvent* xev = new XEvent();
    ui::InitXButtonEventForTesting(
        is_button_press ? ui::ET_MOUSE_PRESSED : ui::ET_MOUSE_RELEASED,
        0,
        xev);
    xevs_.push_back(xev);
    ui::MouseEvent* event = new ui::MouseEvent(xev);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  ui::MouseWheelEvent* GenerateMouseWheelEvent(int wheel_delta) {
    EXPECT_FALSE(wheel_delta == 0);
    XEvent* xev = new XEvent();
    ui::InitXMouseWheelEventForTesting(wheel_delta, 0, xev);
    xevs_.push_back(xev);
    ui::MouseWheelEvent* event = new ui::MouseWheelEvent(xev);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  aura::Window* target() { return target_; }

 private:
  // Owned by root window of shell, but we can still delete |target_| safely.
  aura::Window* target_;
  // The root window of |target_|. Not owned.
  aura::Window* root_window_;
  ScopedVector<XEvent> xevs_;
};

TEST_F(StickyKeysTest, BasicOneshotScenarioTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
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
  ASSERT_EQ(2U, mock_delegate->GetEventCount());
  EXPECT_EQ(ui::ET_KEY_PRESSED, mock_delegate->GetEvent(0)->type());
  EXPECT_EQ(ui::VKEY_A,
            static_cast<const ui::KeyEvent*>(mock_delegate->GetEvent(0))
                ->key_code());
  EXPECT_EQ(ui::ET_KEY_RELEASED, mock_delegate->GetEvent(1)->type());
  EXPECT_EQ(ui::VKEY_SHIFT,
            static_cast<const ui::KeyEvent*>(mock_delegate->GetEvent(1))
                ->key_code());

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
      new MockStickyKeysHandlerDelegate(this);
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
      new MockStickyKeysHandlerDelegate(this);
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

TEST_F(StickyKeysTest, NormalShortcutTest) {
  // Sticky keys should not be enabled if we perform a normal shortcut.
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  // Perform ctrl+n shortcut.
  ev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(true, ui::VKEY_N));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(false, ui::VKEY_N));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  // Sticky keys should not be enabled afterwards.
  ev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, MouseEventOneshot) {
  scoped_ptr<ui::MouseEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  // We should still be in the ENABLED state until we get the mouse
  // release event.
  ev.reset(GenerateMouseEvent(true));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  ev.reset(GenerateMouseEvent(false));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  // Making sure modifier key release event is dispatched in the right order.
  ASSERT_EQ(2u, mock_delegate->GetEventCount());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, mock_delegate->GetEvent(0)->type());
  EXPECT_EQ(ui::ET_KEY_RELEASED, mock_delegate->GetEvent(1)->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<const ui::KeyEvent*>(mock_delegate->GetEvent(1))
                ->key_code());

  // Enabled state is one shot, so next click should not be control modified.
  ev.reset(GenerateMouseEvent(true));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_FALSE(ev->flags() & ui::EF_CONTROL_DOWN);

  ev.reset(GenerateMouseEvent(false));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_FALSE(ev->flags() & ui::EF_CONTROL_DOWN);
}

TEST_F(StickyKeysTest, MouseEventLocked) {
  scoped_ptr<ui::MouseEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());

  // Pressing modifier key twice should make us enter lock state.
  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());

  // Mouse events should not disable locked mode.
  for (int i = 0; i < 3; ++i) {
    ev.reset(GenerateMouseEvent(true));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    ev.reset(GenerateMouseEvent(false));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());
  }

  // Test with mouse wheel.
  for (int i = 0; i < 3; ++i) {
    ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
    sticky_key.HandleMouseEvent(ev.get());
    ev.reset(GenerateMouseWheelEvent(-ui::MouseWheelEvent::kWheelDelta));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());
  }

  // Test mixed case with mouse events and key events.
  ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  kev.reset(GenerateKey(true, ui::VKEY_N));
  sticky_key.HandleKeyEvent(kev.get());
  kev.reset(GenerateKey(false, ui::VKEY_N));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(StickyKeysHandler::LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, EventTargetDestroyed) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  target()->Focus();

  // Go into ENABLED state.
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  ev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::ENABLED, sticky_key.current_state());

  // CTRL+J is a special shortcut that will destroy the event target.
  ev.reset(GenerateKey(true, ui::VKEY_J));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(StickyKeysHandler::DISABLED, sticky_key.current_state());
  EXPECT_FALSE(target());
}

}  // namespace ash
