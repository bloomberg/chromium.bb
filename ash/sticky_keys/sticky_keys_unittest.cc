// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sticky_keys/sticky_keys_controller.h"

#include <X11/Xlib.h>
#undef None
#undef Bool
#undef RootWindow

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_processor.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/device_data_manager.h"

namespace ash {

namespace {

// The device id of the test touchpad device.
const unsigned int kTouchPadDeviceId = 1;

}  // namespace

// Keeps a buffer of handled events.
class EventBuffer : public ui::EventHandler {
 public:
  EventBuffer() {}
  virtual ~EventBuffer() {}

  void PopEvents(ScopedVector<ui::Event>* events) {
    events->clear();
    events->swap(events_);
  }

 private:
  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    events_.push_back(new ui::KeyEvent(*event));
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->IsMouseWheelEvent()) {
      events_.push_back(
          new ui::MouseWheelEvent(*static_cast<ui::MouseWheelEvent*>(event)));
    } else {
      events_.push_back(new ui::MouseEvent(*event));
    }
  }

  ScopedVector<ui::Event> events_;

  DISALLOW_COPY_AND_ASSIGN(EventBuffer);
};

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

    events_.push_back(new ui::KeyEvent(*event));
  }

  virtual void DispatchMouseEvent(ui::MouseEvent* event,
                                  aura::Window* target) OVERRIDE {
    ASSERT_EQ(delegate_->GetExpectedTarget(), target);
    events_.push_back(
        new ui::MouseEvent(*event, target, target->GetRootWindow()));
  }

  virtual void DispatchScrollEvent(ui::ScrollEvent* event,
                                   aura::Window* target) OVERRIDE {
    events_.push_back(new ui::ScrollEvent(event->native_event()));
  }

  // Returns the count of dispatched events.
  size_t GetEventCount() const {
    return events_.size();
  }

  // Returns the |index|-th dispatched event.
  const ui::Event* GetEvent(size_t index) const {
    return events_[index];
  }

  // Clears all previously dispatched events.
  void ClearEvents() {
    events_.clear();
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

    ui::SetUpTouchPadForTest(kTouchPadDeviceId);
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

  ui::KeyEvent* GenerateKey(bool is_key_press, ui::KeyboardCode code) {
    scoped_xevent_.InitKeyEvent(
        is_key_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
        code,
        0);
    ui::KeyEvent* event =  new ui::KeyEvent(scoped_xevent_, false);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a mouse event backed by a native XInput2 generic button event.
  // This is the standard native event on Chromebooks.
  ui::MouseEvent* GenerateMouseEvent(bool is_button_press) {
    return GenerateMouseEventAt(is_button_press, gfx::Point());
  }

  // Creates a mouse event backed by a native XInput2 generic button event.
  // The |location| should be in physical pixels.
  ui::MouseEvent* GenerateMouseEventAt(bool is_button_press,
                                       const gfx::Point& location) {
    scoped_xevent_.InitGenericButtonEvent(
        kTouchPadDeviceId,
        is_button_press ? ui::ET_MOUSE_PRESSED : ui::ET_MOUSE_RELEASED,
        location,
        0);
    ui::MouseEvent* event = new ui::MouseEvent(scoped_xevent_);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  ui::MouseWheelEvent* GenerateMouseWheelEvent(int wheel_delta) {
    EXPECT_NE(0, wheel_delta);
    scoped_xevent_.InitGenericMouseWheelEvent(
        kTouchPadDeviceId, wheel_delta, 0);
    ui::MouseWheelEvent* event = new ui::MouseWheelEvent(scoped_xevent_);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  ui::ScrollEvent* GenerateScrollEvent(int scroll_delta) {
    scoped_xevent_.InitScrollEvent(kTouchPadDeviceId, // deviceid
                                   0,               // x_offset
                                   scroll_delta,    // y_offset
                                   0,               // x_offset_ordinal
                                   scroll_delta,    // y_offset_ordinal
                                   2);              // finger_count
    ui::ScrollEvent* event = new ui::ScrollEvent(scoped_xevent_);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  ui::ScrollEvent* GenerateFlingScrollEvent(int fling_delta,
                                            bool is_cancel) {
    scoped_xevent_.InitFlingScrollEvent(
        kTouchPadDeviceId, // deviceid
        0,               // x_velocity
        fling_delta,     // y_velocity
        0,               // x_velocity_ordinal
        fling_delta,     // y_velocity_ordinal
        is_cancel);      // is_cancel
    ui::ScrollEvent* event = new ui::ScrollEvent(scoped_xevent_);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a synthesized KeyEvent that is not backed by a native event.
  ui::KeyEvent* GenerateSynthesizedKeyEvent(
      bool is_key_press, ui::KeyboardCode code) {
    ui::KeyEvent* event = new ui::KeyEvent(
        is_key_press ? ui::ET_KEY_PRESSED : ui::ET_MOUSE_RELEASED,
        code, 0, true);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a synthesized MouseEvent that is not backed by a native event.
  ui::MouseEvent* GenerateSynthesizedMouseEvent(bool is_button_press) {
    ui::MouseEvent* event = new ui::MouseEvent(
        is_button_press ? ui::ET_MOUSE_PRESSED : ui::ET_MOUSE_RELEASED,
        gfx::Point(0, 0),
        gfx::Point(0, 0),
        ui::EF_LEFT_MOUSE_BUTTON,
        ui::EF_LEFT_MOUSE_BUTTON);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  // Creates a synthesized ET_MOUSE_MOVED event.
  ui::MouseEvent* GenerateSynthesizedMouseEvent(int x, int y) {
    ui::MouseEvent* event = new ui::MouseEvent(
        ui::ET_MOUSE_MOVED,
        gfx::Point(x, y),
        gfx::Point(x, y),
        ui::EF_LEFT_MOUSE_BUTTON,
        ui::EF_LEFT_MOUSE_BUTTON);
    ui::Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target_);
    return event;
  }

  void SendActivateStickyKeyPattern(StickyKeysHandler* handler,
                                    ui::KeyboardCode key_code) {
    scoped_ptr<ui::KeyEvent> ev;
    ev.reset(GenerateKey(true, key_code));
    handler->HandleKeyEvent(ev.get());
    ev.reset(GenerateKey(false, key_code));
    handler->HandleKeyEvent(ev.get());
  }

  void SendActivateStickyKeyPattern(ui::EventProcessor* dispatcher,
                                    ui::KeyboardCode key_code) {
    scoped_ptr<ui::KeyEvent> ev;
    ev.reset(GenerateKey(true, key_code));
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(ev.get());
    CHECK(!details.dispatcher_destroyed);
    ev.reset(GenerateKey(false, key_code));
    details = dispatcher->OnEventFromSource(ev.get());
    CHECK(!details.dispatcher_destroyed);
  }

  aura::Window* target() { return target_; }

 private:
  // Owned by root window of shell, but we can still delete |target_| safely.
  aura::Window* target_;
  // The root window of |target_|. Not owned.
  aura::Window* root_window_;

  // Used to construct the various X events.
  ui::ScopedXI2Event scoped_xevent_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysTest);
};

TEST_F(StickyKeysTest, BasicOneshotScenarioTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // By typing Shift key, internal state become ENABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());

  // Next keyboard event is shift modified.
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
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

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // By typing shift key, internal state become ENABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // By typing shift key again, internal state become LOCKED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // All keyboard events including keyUp become shift modified.
  ev.reset(GenerateKey(true, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_A));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  // Locked state keeps after normal keyboard event.
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(true, ui::VKEY_B));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  ev.reset(GenerateKey(false, ui::VKEY_B));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_SHIFT_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // By typing shift key again, internal state become back to DISABLED.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NonTargetModifierTest) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_SHIFT_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_SHIFT);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Non target modifier key does not affect internal state
  ev.reset(GenerateKey(true, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  ev.reset(GenerateKey(false, ui::VKEY_MENU));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NormalShortcutTest) {
  // Sticky keys should not be enabled if we perform a normal shortcut.
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+n shortcut.
  ev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(true, ui::VKEY_N));
  sticky_key.HandleKeyEvent(ev.get());
  ev.reset(GenerateKey(false, ui::VKEY_N));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Sticky keys should not be enabled afterwards.
  ev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NormalModifiedClickTest) {
  scoped_ptr<ui::KeyEvent> kev;
  scoped_ptr<ui::MouseEvent> mev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+click.
  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  mev.reset(GenerateMouseEvent(true));
  sticky_key.HandleMouseEvent(mev.get());
  mev.reset(GenerateMouseEvent(false));
  sticky_key.HandleMouseEvent(mev.get());

  // Sticky keys should not be enabled afterwards.
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, MouseMovedModifierTest) {
  scoped_ptr<ui::KeyEvent> kev;
  scoped_ptr<ui::MouseEvent> mev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Press ctrl and handle mouse move events.
  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  mev.reset(GenerateSynthesizedMouseEvent(0, 0));
  sticky_key.HandleMouseEvent(mev.get());
  mev.reset(GenerateSynthesizedMouseEvent(100, 100));
  sticky_key.HandleMouseEvent(mev.get());

  // Sticky keys should be enabled afterwards.
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, NormalModifiedScrollTest) {
  scoped_ptr<ui::KeyEvent> kev;
  scoped_ptr<ui::ScrollEvent> sev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Perform ctrl+scroll.
  kev.reset(GenerateKey(true, ui::VKEY_CONTROL));
  sev.reset(GenerateFlingScrollEvent(0, true));
  sticky_key.HandleScrollEvent(sev.get());
  sev.reset(GenerateScrollEvent(10));
  sticky_key.HandleScrollEvent(sev.get());
  sev.reset(GenerateFlingScrollEvent(10, false));
  sticky_key.HandleScrollEvent(sev.get());

  // Sticky keys should not be enabled afterwards.
  kev.reset(GenerateKey(false, ui::VKEY_CONTROL));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, MouseEventOneshot) {
  scoped_ptr<ui::MouseEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // We should still be in the ENABLED state until we get the mouse
  // release event.
  ev.reset(GenerateMouseEvent(true));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  ev.reset(GenerateMouseEvent(false));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

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

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Pressing modifier key twice should make us enter lock state.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Mouse events should not disable locked mode.
  for (int i = 0; i < 3; ++i) {
    ev.reset(GenerateMouseEvent(true));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    ev.reset(GenerateMouseEvent(false));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  }

  // Test with mouse wheel.
  for (int i = 0; i < 3; ++i) {
    ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
    sticky_key.HandleMouseEvent(ev.get());
    ev.reset(GenerateMouseWheelEvent(-ui::MouseWheelEvent::kWheelDelta));
    sticky_key.HandleMouseEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
  }

  // Test mixed case with mouse events and key events.
  ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
  sticky_key.HandleMouseEvent(ev.get());
  EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
  kev.reset(GenerateKey(true, ui::VKEY_N));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_TRUE(kev->flags() & ui::EF_CONTROL_DOWN);
  kev.reset(GenerateKey(false, ui::VKEY_N));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_TRUE(kev->flags() & ui::EF_CONTROL_DOWN);

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, ScrollEventOneshot) {
  scoped_ptr<ui::ScrollEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  int scroll_deltas[] = {-10, 10};
  for (int i = 0; i < 2; ++i) {
    mock_delegate->ClearEvents();

    // Enable sticky keys.
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
    SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Test a scroll sequence. Sticky keys should only be disabled at the end
    // of the scroll sequence. Fling cancel event starts the scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Scrolls should all be modified but not disable sticky keys.
    for (int j = 0; j < 3; ++j) {
      ev.reset(GenerateScrollEvent(scroll_deltas[i]));
      sticky_key.HandleScrollEvent(ev.get());
      EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
      EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
    }

    // Fling start event ends scroll sequence.
    ev.reset(GenerateFlingScrollEvent(scroll_deltas[i], false));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

    ASSERT_EQ(2U, mock_delegate->GetEventCount());
    EXPECT_EQ(ui::ET_SCROLL_FLING_START, mock_delegate->GetEvent(0)->type());
    EXPECT_FLOAT_EQ(scroll_deltas[i],
                    static_cast<const ui::ScrollEvent*>(
                        mock_delegate->GetEvent(0))->y_offset());
    EXPECT_EQ(ui::ET_KEY_RELEASED, mock_delegate->GetEvent(1)->type());
    EXPECT_EQ(ui::VKEY_CONTROL,
              static_cast<const ui::KeyEvent*>(mock_delegate->GetEvent(1))
                  ->key_code());
  }
}

TEST_F(StickyKeysTest, ScrollDirectionChanged) {
  scoped_ptr<ui::ScrollEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  // Test direction change with both boundary value and negative value.
  const int direction_change_values[2] = {0, -10};
  for (int i = 0; i < 2; ++i) {
    SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Fling cancel starts scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

    // Test that changing directions in a scroll sequence will
    // return sticky keys to DISABLED state.
    for (int j = 0; j < 3; ++j) {
      ev.reset(GenerateScrollEvent(10));
      sticky_key.HandleScrollEvent(ev.get());
      EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
      EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());
    }

    ev.reset(GenerateScrollEvent(direction_change_values[i]));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_FALSE(ev->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  }
}

TEST_F(StickyKeysTest, ScrollEventLocked) {
  scoped_ptr<ui::ScrollEvent> ev;
  scoped_ptr<ui::KeyEvent> kev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  // Lock sticky keys.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());

  // Test scroll events are correctly modified in locked state.
  for (int i = 0; i < 5; ++i) {
    // Fling cancel starts scroll sequence.
    ev.reset(GenerateFlingScrollEvent(0, true));
    sticky_key.HandleScrollEvent(ev.get());

    ev.reset(GenerateScrollEvent(10));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);
    ev.reset(GenerateScrollEvent(-10));
    sticky_key.HandleScrollEvent(ev.get());
    EXPECT_TRUE(ev->flags() & ui::EF_CONTROL_DOWN);

    // Fling start ends scroll sequence.
    ev.reset(GenerateFlingScrollEvent(-10, false));
    sticky_key.HandleScrollEvent(ev.get());
  }

  EXPECT_EQ(STICKY_KEY_STATE_LOCKED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, EventTargetDestroyed) {
  scoped_ptr<ui::KeyEvent> ev;
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  target()->Focus();

  // Go into ENABLED state.
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  // CTRL+J is a special shortcut that will destroy the event target.
  ev.reset(GenerateKey(true, ui::VKEY_J));
  sticky_key.HandleKeyEvent(ev.get());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
  EXPECT_FALSE(target());
}

TEST_F(StickyKeysTest, SynthesizedEvents) {
  // Non-native, internally generated events should be properly handled
  // by sticky keys.
  MockStickyKeysHandlerDelegate* mock_delegate =
      new MockStickyKeysHandlerDelegate(this);
  StickyKeysHandler sticky_key(ui::EF_CONTROL_DOWN, mock_delegate);

  // Test non-native key events.
  scoped_ptr<ui::KeyEvent> kev;
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  kev.reset(GenerateSynthesizedKeyEvent(true, ui::VKEY_K));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_TRUE(kev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  kev.reset(GenerateSynthesizedKeyEvent(false, ui::VKEY_K));
  sticky_key.HandleKeyEvent(kev.get());
  EXPECT_FALSE(kev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());

  // Test non-native mouse events.
  SendActivateStickyKeyPattern(&sticky_key, ui::VKEY_CONTROL);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  scoped_ptr<ui::MouseEvent> mev;
  mev.reset(GenerateSynthesizedMouseEvent(true));
  sticky_key.HandleMouseEvent(mev.get());
  EXPECT_TRUE(mev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED, sticky_key.current_state());

  mev.reset(GenerateSynthesizedMouseEvent(false));
  sticky_key.HandleMouseEvent(mev.get());
  EXPECT_TRUE(mev->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED, sticky_key.current_state());
}

TEST_F(StickyKeysTest, KeyEventDispatchImpl) {
  // Test the actual key event dispatch implementation.
  EventBuffer buffer;
  ScopedVector<ui::Event> events;
  ui::EventProcessor* dispatcher =
      Shell::GetPrimaryRootWindow()->GetHost()->event_processor();
  Shell::GetInstance()->AddPreTargetHandler(&buffer);
  Shell::GetInstance()->sticky_keys_controller()->Enable(true);

  SendActivateStickyKeyPattern(dispatcher, ui::VKEY_CONTROL);
  scoped_ptr<ui::KeyEvent> ev;
  buffer.PopEvents(&events);

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ev.reset(GenerateKey(true, ui::VKEY_C));
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(ev.get());
  buffer.PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C, static_cast<ui::KeyEvent*>(events[0])->key_code());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  // Test key release event is not modified.
  ev.reset(GenerateKey(false, ui::VKEY_C));
  details = dispatcher->OnEventFromSource(ev.get());
  ASSERT_FALSE(details.dispatcher_destroyed);
  buffer.PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[0]->type());
  EXPECT_EQ(ui::VKEY_C,
            static_cast<ui::KeyEvent*>(events[0])->key_code());
  EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);

  Shell::GetInstance()->RemovePreTargetHandler(&buffer);
}

class StickyKeysMouseDispatchTest
    : public StickyKeysTest,
      public ::testing::WithParamInterface<int> {
};

TEST_P(StickyKeysMouseDispatchTest, MouseEventDispatchImpl) {
  int scale_factor = GetParam();
  std::ostringstream display_specs;
  display_specs << "1280x1024*" << scale_factor;
  UpdateDisplay(display_specs.str());

  EventBuffer buffer;
  ScopedVector<ui::Event> events;
  ui::EventProcessor* dispatcher =
      Shell::GetPrimaryRootWindow()->GetHost()->event_processor();
  Shell::GetInstance()->AddPreTargetHandler(&buffer);
  Shell::GetInstance()->sticky_keys_controller()->Enable(true);

  scoped_ptr<ui::MouseEvent> ev;
  SendActivateStickyKeyPattern(dispatcher, ui::VKEY_CONTROL);
  buffer.PopEvents(&events);

  // Test mouse press event is correctly modified and has correct DIP location.
  gfx::Point physical_location(400, 400);
  gfx::Point dip_location(physical_location.x() / scale_factor,
                          physical_location.y() / scale_factor);
  ev.reset(GenerateMouseEventAt(true, physical_location));
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(ev.get());
  buffer.PopEvents(&events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(dip_location.ToString(),
            static_cast<ui::MouseEvent*>(events[0])->location().ToString());

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ev.reset(GenerateMouseEventAt(false, physical_location));
  details = dispatcher->OnEventFromSource(ev.get());
  ASSERT_FALSE(details.dispatcher_destroyed);
  buffer.PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[0]->type());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(dip_location.ToString(),
            static_cast<ui::MouseEvent*>(events[0])->location().ToString());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  Shell::GetInstance()->RemovePreTargetHandler(&buffer);
}

TEST_P(StickyKeysMouseDispatchTest, MouseWheelEventDispatchImpl) {
  int scale_factor = GetParam();
  std::ostringstream display_specs;
  display_specs << "1280x1024*" << scale_factor;
  UpdateDisplay(display_specs.str());

  // Test the actual mouse wheel event dispatch implementation.
  EventBuffer buffer;
  ScopedVector<ui::Event> events;
  ui::EventProcessor* dispatcher =
      Shell::GetPrimaryRootWindow()->GetHost()->event_processor();
  Shell::GetInstance()->AddPreTargetHandler(&buffer);
  Shell::GetInstance()->sticky_keys_controller()->Enable(true);

  scoped_ptr<ui::MouseWheelEvent> ev;
  SendActivateStickyKeyPattern(dispatcher, ui::VKEY_CONTROL);
  buffer.PopEvents(&events);

  // Test positive mouse wheel event is correctly modified and modifier release
  // event is sent.
  ev.reset(GenerateMouseWheelEvent(ui::MouseWheelEvent::kWheelDelta));
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(ev.get());
  ASSERT_FALSE(details.dispatcher_destroyed);
  buffer.PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_EQ(ui::MouseWheelEvent::kWheelDelta / scale_factor,
            static_cast<ui::MouseWheelEvent*>(events[0])->y_offset());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  // Test negative mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(dispatcher, ui::VKEY_CONTROL);
  buffer.PopEvents(&events);

  ev.reset(GenerateMouseWheelEvent(-ui::MouseWheelEvent::kWheelDelta));
  details = dispatcher->OnEventFromSource(ev.get());
  ASSERT_FALSE(details.dispatcher_destroyed);
  buffer.PopEvents(&events);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->IsMouseWheelEvent());
  EXPECT_EQ(-ui::MouseWheelEvent::kWheelDelta / scale_factor,
            static_cast<ui::MouseWheelEvent*>(events[0])->y_offset());
  EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
  EXPECT_EQ(ui::VKEY_CONTROL,
            static_cast<ui::KeyEvent*>(events[1])->key_code());

  Shell::GetInstance()->RemovePreTargetHandler(&buffer);
}

INSTANTIATE_TEST_CASE_P(DPIScaleFactors,
                        StickyKeysMouseDispatchTest,
                        ::testing::Values(1, 2));

}  // namespace ash
