// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_overlay/keyboard_overlay_view.h"

#include "ash/keyboard_overlay/keyboard_overlay_delegate.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using ui::WebDialogDelegate;

namespace {
// Store the pointer to the view currently shown.
KeyboardOverlayView* g_instance = NULL;
}

KeyboardOverlayView::KeyboardOverlayView(
    content::BrowserContext* context,
    WebDialogDelegate* delegate,
    WebContentsHandler* handler)
    : views::WebDialogView(context, delegate, handler) {
}

KeyboardOverlayView::~KeyboardOverlayView() {
}

void KeyboardOverlayView::ShowDialog(
    content::BrowserContext* context,
    WebContentsHandler* handler,
    const GURL& url) {
  // Ignore the call if another view is already shown.
  if (g_instance)
    return;

  KeyboardOverlayDelegate* delegate = new KeyboardOverlayDelegate(
      l10n_util::GetStringUTF16(IDS_ASH_KEYBOARD_OVERLAY_TITLE), url);
  KeyboardOverlayView* view =
      new KeyboardOverlayView(context, delegate, handler);
  delegate->Show(view);

  g_instance = view;
}

void KeyboardOverlayView::WindowClosing() {
  g_instance = NULL;
}
