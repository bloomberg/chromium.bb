// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sticky_keys/sticky_keys_overlay.h"

#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/event.h"

namespace ash {

class StickyKeysOverlayTest : public test::AshTestBase {
 public:
  StickyKeysOverlayTest() {}
  virtual ~StickyKeysOverlayTest() {}
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

// Additional sticky key overlay tests that depend on chromeos::EventRewriter
// are now in chrome/browser/chromeos/events/event_rewriter_unittest.cc .

}  // namespace ash
