// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_dialog_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using ui::WebDialogDelegate;

namespace {
// Store the pointer to the view currently shown.
KeyboardOverlayDialogView* g_instance = NULL;
}

KeyboardOverlayDialogView::KeyboardOverlayDialogView(
    Profile* profile,
    WebDialogDelegate* delegate)
    : WebDialogView(profile, NULL, delegate) {
}

KeyboardOverlayDialogView::~KeyboardOverlayDialogView() {
}

void KeyboardOverlayDialogView::ShowDialog() {
  // Ignore the call if another view is already shown.
  if (g_instance)
    return;

  KeyboardOverlayDelegate* delegate = new KeyboardOverlayDelegate(
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TITLE));
  KeyboardOverlayDialogView* view = new KeyboardOverlayDialogView(
      ProfileManager::GetDefaultProfileOrOffTheRecord(), delegate);
  delegate->set_view(view);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view;
  widget->Init(params);

  // Show the widget at the bottom of the work area.
  gfx::Size size;
  delegate->GetDialogSize(&size);
  gfx::Rect rect = gfx::Screen::GetMonitorNearestWindow(
      view->GetWidget()->GetNativeView()).work_area();
  gfx::Rect bounds((rect.width() - size.width()) / 2,
                   rect.height() - size.height(),
                   size.width(),
                   size.height());
  view->GetWidget()->SetBounds(bounds);
  view->GetWidget()->Show();

  g_instance = view;
}

void KeyboardOverlayDialogView::WindowClosing() {
  g_instance = NULL;
}
