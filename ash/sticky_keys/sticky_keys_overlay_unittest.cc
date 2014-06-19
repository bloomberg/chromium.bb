// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sticky_keys/sticky_keys_overlay.h"

#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"

namespace ash {

class StickyKeysOverlayTest : public test::AshTestBase {
 public:
  StickyKeysOverlayTest() :
      controller_(NULL),
      overlay_(NULL) {}

  virtual ~StickyKeysOverlayTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();

    controller_ = Shell::GetInstance()->sticky_keys_controller();
    controller_->Enable(true);
    overlay_ = controller_->GetOverlayForTest();
    ASSERT_TRUE(overlay_);
  }

  void PressAndReleaseKey(ui::KeyboardCode code) {
    SendKeyEvent(ui::ET_KEY_PRESSED, code);
    SendKeyEvent(ui::ET_KEY_RELEASED, code);
  }

  void SendKeyEvent(ui::EventType type, ui::KeyboardCode code) {
    ui::KeyEvent event(type, code, 0, false);
    ui::Event::DispatcherApi dispatcher(&event);
    dispatcher.set_target(Shell::GetInstance()->GetPrimaryRootWindow());

    ui::EventDispatchDetails details = Shell::GetPrimaryRootWindow()->
        GetHost()->event_processor()->OnEventFromSource(&event);
    CHECK(!details.dispatcher_destroyed);
  }

  StickyKeysController* controller_;
  StickyKeysOverlay* overlay_;
};

TEST_F(StickyKeysOverlayTest, OverlayVisibility) {
  StickyKeysOverlay overlay;
  EXPECT_FALSE(overlay.is_visible());
  overlay.Show(true);
  EXPECT_TRUE(overlay.is_visible());
}

TEST_F(StickyKeysOverlayTest, ModifierKeyState) {
  StickyKeysOverlay overlay;
  overlay.SetModifierKeyState(ui::EF_SHIFT_DOWN, STICKY_KEY_STATE_DISABLED);
  overlay.SetModifierKeyState(ui::EF_ALT_DOWN, STICKY_KEY_STATE_LOCKED);
  overlay.SetModifierKeyState(ui::EF_CONTROL_DOWN, STICKY_KEY_STATE_ENABLED);

  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay.GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay.GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay.GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_F(StickyKeysOverlayTest, OneModifierEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing modifier key should show overlay.
  PressAndReleaseKey(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  PressAndReleaseKey(ui::VKEY_T);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_F(StickyKeysOverlayTest, TwoModifiersEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing two modifiers should show overlay.
  PressAndReleaseKey(ui::VKEY_SHIFT);
  PressAndReleaseKey(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  PressAndReleaseKey(ui::VKEY_N);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  PressAndReleaseKey(ui::VKEY_LMENU);
  PressAndReleaseKey(ui::VKEY_LMENU);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  PressAndReleaseKey(ui::VKEY_D);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedAndNormalModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  PressAndReleaseKey(ui::VKEY_CONTROL);
  PressAndReleaseKey(ui::VKEY_CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  PressAndReleaseKey(ui::VKEY_SHIFT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  PressAndReleaseKey(ui::VKEY_D);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifiersDisabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Enable modifiers.
  PressAndReleaseKey(ui::VKEY_CONTROL);
  PressAndReleaseKey(ui::VKEY_SHIFT);
  PressAndReleaseKey(ui::VKEY_SHIFT);
  PressAndReleaseKey(ui::VKEY_LMENU);

  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Disable modifiers and overlay should be hidden.
  PressAndReleaseKey(ui::VKEY_CONTROL);
  PressAndReleaseKey(ui::VKEY_CONTROL);
  PressAndReleaseKey(ui::VKEY_SHIFT);
  PressAndReleaseKey(ui::VKEY_LMENU);
  PressAndReleaseKey(ui::VKEY_LMENU);

  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn all modifiers on.
  controller_->SetModifiersEnabled(true, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off Mod3.
  controller_->SetModifiersEnabled(false, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr.
  controller_->SetModifiersEnabled(true, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr and Mod3.
  controller_->SetModifiersEnabled(false, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
}

}  // namespace ash
