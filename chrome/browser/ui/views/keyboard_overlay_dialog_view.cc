// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_dialog_view.h"

#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/accelerator_table_gtk.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/native_web_keyboard_event.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/events/event.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "views/window/window_gtk.h"
#endif

namespace {
struct Accelerator {
  ui::KeyboardCode keycode;
  bool shift_pressed;
  bool ctrl_pressed;
  bool alt_pressed;
} kCloseAccelerators[] = {
  {ui::VKEY_OEM_2, false, true, true},
  {ui::VKEY_OEM_2, true, true, true},
  {ui::VKEY_ESCAPE, true, false, false},
};
}  // namespace

KeyboardOverlayDialogView::KeyboardOverlayDialogView(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    BrowserView* parent_view)
    : HtmlDialogView(profile, delegate),
      parent_view_(parent_view) {
}

KeyboardOverlayDialogView::~KeyboardOverlayDialogView() {
}

void KeyboardOverlayDialogView::InitDialog() {
  DOMView::Init(profile(), NULL);

  tab_contents_->set_delegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tab_contents_->property_bag(),
                                                  this);

  for (size_t i = 0; i < arraysize(kCloseAccelerators); ++i) {
    views::Accelerator accelerator(kCloseAccelerators[i].keycode,
                                   kCloseAccelerators[i].shift_pressed,
                                   kCloseAccelerators[i].ctrl_pressed,
                                   kCloseAccelerators[i].alt_pressed);
    close_accelerators_.insert(accelerator);
    AddAccelerator(accelerator);
  }

  for (size_t i = 0; i < browser::kAcceleratorMapLength; ++i) {
    views::Accelerator accelerator(browser::kAcceleratorMap[i].keycode,
                                   browser::kAcceleratorMap[i].shift_pressed,
                                   browser::kAcceleratorMap[i].ctrl_pressed,
                                   browser::kAcceleratorMap[i].alt_pressed);
    // Skip a sole ALT key since it's handled on the keyboard overlay.
    if (views::Accelerator(ui::VKEY_MENU, false, false, false) == accelerator) {
      continue;
    }
    // Skip accelerators for closing the dialog since they are already added.
    if (IsCloseAccelerator(accelerator)) {
      continue;
    }
    AddAccelerator(accelerator);
  }

  DOMView::LoadURL(GetDialogContentURL());
}

bool KeyboardOverlayDialogView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (!IsCloseAccelerator(accelerator)) {
    parent_view_->AcceleratorPressed(accelerator);
  }
  OnDialogClosed(std::string());
  return true;
}

void KeyboardOverlayDialogView::ShowDialog(
    gfx::NativeWindow owning_window, BrowserView* parent_view) {
  KeyboardOverlayDelegate* delegate = new KeyboardOverlayDelegate(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TITLE)));
  KeyboardOverlayDialogView* html_view =
      new KeyboardOverlayDialogView(parent_view->browser()->profile(),
                                    delegate,
                                    parent_view);
  delegate->set_view(html_view);
  html_view->InitDialog();
  chromeos::BubbleWindow::Create(owning_window,
                                 gfx::Rect(),
                                 chromeos::BubbleWindow::STYLE_XSHAPE,
                                 html_view);
  html_view->window()->Show();
}

bool KeyboardOverlayDialogView::IsCloseAccelerator(
    const views::Accelerator& accelerator) {
  return close_accelerators_.find(accelerator) != close_accelerators_.end();
}
