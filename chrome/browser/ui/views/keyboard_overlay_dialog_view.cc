// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_dialog_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/generated_resources.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {
struct Accelerator {
  ui::KeyboardCode keycode;
  int modifiers;
} const kCloseAccelerators[] = {
  {ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN},
  {ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN},
  {ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN},
};
}  // namespace

KeyboardOverlayDialogView::KeyboardOverlayDialogView(
    Profile* profile,
    WebDialogDelegate* delegate,
    AcceleratorTarget* target)
    : WebDialogView(profile, NULL, delegate),
      target_(target) {
  RegisterDialogAccelerators();
}

KeyboardOverlayDialogView::~KeyboardOverlayDialogView() {
}

void KeyboardOverlayDialogView::RegisterDialogAccelerators() {
  for (size_t i = 0; i < arraysize(kCloseAccelerators); ++i) {
    ui::Accelerator accelerator(kCloseAccelerators[i].keycode,
                                kCloseAccelerators[i].modifiers);
    close_accelerators_.insert(accelerator);
    AddAccelerator(accelerator);
  }

  for (size_t i = 0; i < browser::kAcceleratorMapLength; ++i) {
    ui::Accelerator accelerator(browser::kAcceleratorMap[i].keycode,
                                browser::kAcceleratorMap[i].modifiers);
    // Skip a sole ALT key since it's handled on the keyboard overlay.
    if (ui::Accelerator(ui::VKEY_MENU, ui::EF_NONE) == accelerator) {
      continue;
    }
    // Skip accelerators for closing the dialog since they are already added.
    if (IsCloseAccelerator(accelerator)) {
      continue;
    }
    AddAccelerator(accelerator);
  }
}

bool KeyboardOverlayDialogView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!IsCloseAccelerator(accelerator))
    target_->AcceleratorPressed(accelerator);
  OnDialogClosed(std::string());
  return true;
}

void KeyboardOverlayDialogView::ShowDialog(ui::AcceleratorTarget* target) {
  // Temporarily disable Shift+Alt. crosbug.com/17208.
  chromeos::input_method::InputMethodManager::GetInstance()->DisableHotkeys();

  KeyboardOverlayDelegate* delegate = new KeyboardOverlayDelegate(
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TITLE));
  KeyboardOverlayDialogView* view = new KeyboardOverlayDialogView(
      ProfileManager::GetDefaultProfileOrOffTheRecord(), delegate, target);
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
}

bool KeyboardOverlayDialogView::IsCloseAccelerator(
    const ui::Accelerator& accelerator) {
  return close_accelerators_.find(accelerator) != close_accelerators_.end();
}
