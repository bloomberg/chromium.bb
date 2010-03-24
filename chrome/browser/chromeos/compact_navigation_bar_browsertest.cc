// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace {

// TODO(oshima): Refactor and move to common place (probably in chrome/test/).
class Key {
 public:
  // Creates a key with modifiers.
  Key(int keyval, bool shift, bool ctrl, bool alt)
      : keyval_(keyval),
        modifier_(0) {
    if (shift)
      modifier_ |= GDK_SHIFT_MASK;
    if (ctrl)
      modifier_ |= GDK_CONTROL_MASK;
    if (alt)
      modifier_ |= GDK_MOD1_MASK;
  }

  void PressOn(views::Window* window) const {
    Process(window, true);
  }

  void ReleaseOn(views::Window* window) const {
    Process(window, false);
  }

 private:
  // Injects a synthesized key event into gdk event queue.
  void Process(views::Window* window, bool press) const {
    GdkKeymapKey* keys;
    gint n_keys;
    gdk_keymap_get_entries_for_keyval(
        gdk_keymap_get_default(),
        keyval_,
        &keys,
        &n_keys);

    gfx::NativeWindow native_window = window->GetNativeWindow();
    GdkEvent* event = gdk_event_new(press ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
    GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
    if (press)
      key_event->state = modifier_ | GDK_KEY_PRESS_MASK;
    else
      key_event->state = modifier_ | GDK_KEY_RELEASE_MASK;

    key_event->window = GTK_WIDGET(native_window)->window;
    key_event->send_event = true;
    key_event->time = GDK_CURRENT_TIME;
    key_event->keyval = keyval_;
    key_event->hardware_keycode = keys[0].keycode;
    key_event->group = keys[0].group;

    gdk_event_put(event);
  }

  int keyval_;
  int modifier_;
  DISALLOW_COPY_AND_ASSIGN(Key);
};

}  // namespace

namespace chromeos {

class CompactNavigationBarTest : public InProcessBrowserTest {
 public:
  CompactNavigationBarTest() {
  }

  BrowserView* browser_view() const {
    return static_cast<BrowserView*>(browser()->window());
  }

  bool IsViewIdVisible(int id) const {
    return browser_view()->GetViewByID(id)->IsVisible();
  }

  void KeyDown(const Key& key) {
    key.PressOn(browser_view()->GetWidget()->GetWindow());
  }

  void KeyUp(const Key& key) {
    key.ReleaseOn(browser_view()->GetWidget()->GetWindow());
  }
};

IN_PROC_BROWSER_TEST_F(CompactNavigationBarTest, TestBasic) {
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_TOOLBAR));

  browser()->ToggleCompactNavigationBar();
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_TOOLBAR));

  browser()->ToggleCompactNavigationBar();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_TOOLBAR));
}

IN_PROC_BROWSER_TEST_F(CompactNavigationBarTest, TestAccelerator) {
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));

  // ctrl-shift-c should toggle compact navigation bar.
  Key ctrl_shift_c(base::VKEY_C, true, true, false);
  KeyDown(ctrl_shift_c);
  KeyUp(ctrl_shift_c);
  RunAllPendingEvents();
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  KeyDown(ctrl_shift_c);
  KeyUp(ctrl_shift_c);
  RunAllPendingEvents();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));

  // but ctrl-alt-c should not.
  Key ctrl_alt_c(base::VKEY_C, true, false, true);
  KeyDown(ctrl_alt_c);
  KeyUp(ctrl_alt_c);
  RunAllPendingEvents();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
}

}  // namespace chromeos

