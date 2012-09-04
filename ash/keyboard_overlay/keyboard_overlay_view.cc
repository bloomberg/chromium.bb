// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_overlay/keyboard_overlay_view.h"

#include "ash/keyboard_overlay/keyboard_overlay_delegate.h"
#include "ash/shell.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "grit/ash_strings.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using ui::WebDialogDelegate;

namespace {

// Keys to invoke Cancel (Escape, Ctrl+Alt+/, or Shift+Ctrl+Alt+/).
const struct KeyEventData {
  ui::KeyboardCode key_code;
  int flags;
} kCancelKeys[] = {
  { ui::VKEY_ESCAPE, ui::EF_NONE},
  { ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },
  { ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },
};

}

KeyboardOverlayView::KeyboardOverlayView(
    content::BrowserContext* context,
    WebDialogDelegate* delegate,
    WebContentsHandler* handler)
    : views::WebDialogView(context, delegate, handler) {
}

KeyboardOverlayView::~KeyboardOverlayView() {
}

void KeyboardOverlayView::Cancel() {
  ash::Shell::GetInstance()->overlay_filter()->Deactivate();
  views::Widget* widget = GetWidget();
  if (widget)
    widget->Close();
}

bool KeyboardOverlayView::IsCancelingKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return false;
  // Ignore the caps lock state.
  const int flags = (event->flags() & ~ui::EF_CAPS_LOCK_DOWN);
  for (size_t i = 0; i < arraysize(kCancelKeys); ++i) {
    if ((kCancelKeys[i].key_code == event->key_code()) &&
        (kCancelKeys[i].flags == flags))
      return true;
  }
  return false;
}

aura::Window* KeyboardOverlayView::GetWindow() {
  return GetWidget()->GetNativeWindow();
}

void KeyboardOverlayView::ShowDialog(
    content::BrowserContext* context,
    WebContentsHandler* handler,
    const GURL& url) {
  KeyboardOverlayDelegate* delegate = new KeyboardOverlayDelegate(
      l10n_util::GetStringUTF16(IDS_ASH_KEYBOARD_OVERLAY_TITLE), url);
  KeyboardOverlayView* view =
      new KeyboardOverlayView(context, delegate, handler);
  delegate->Show(view);

  ash::Shell::GetInstance()->overlay_filter()->Activate(view);
}

void KeyboardOverlayView::WindowClosing() {
  Cancel();
}
