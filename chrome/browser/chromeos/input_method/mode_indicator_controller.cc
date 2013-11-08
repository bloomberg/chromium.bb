// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_controller.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_widget.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace input_method {

ModeIndicatorController::ModeIndicatorController(
    ModeIndicatorWidget* mi_widget)
    : is_focused_(false) {
  mi_widget_.reset(mi_widget);

  InputMethodManager* imm = InputMethodManager::Get();
  DCHECK(imm);
  imm->AddObserver(this);
}

ModeIndicatorController::~ModeIndicatorController() {
  InputMethodManager* imm = InputMethodManager::Get();
  DCHECK(imm);
  imm->RemoveObserver(this);
}

void ModeIndicatorController::SetCursorLocation(
    const gfx::Rect& cursor_location) {
  mi_widget_->SetCursorLocation(cursor_location);
}

void ModeIndicatorController::FocusStateChanged(bool is_focused) {
  is_focused_ = is_focused;
}

void ModeIndicatorController::InputMethodChanged(InputMethodManager* manager,
                                                 bool show_message) {
  if (!show_message)
    return;

  ShowModeIndicator(manager);
}

void ModeIndicatorController::InputMethodPropertyChanged(
    InputMethodManager* manager) {
  // Do nothing.
}

void ModeIndicatorController::ShowModeIndicator(InputMethodManager* manager) {
  // Need the flag, --enable-ime-mode-indicator at this moment.
  // TODO(komatsu): When this is enabled by defalut, delete command_line.h
  // and chromeos_switches.h from the header files.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableIMEModeIndicator))
    return;

  // TODO(komatsu): Show the mode indicator in the right bottom of the
  // display when the launch bar is hidden and the focus is out.  To
  // implement it, we should consider to use message center or system
  // notification.  Note, launch bar can be vertical and can be placed
  // right/left side of display.
  if (!is_focused_)
    return;

  DCHECK(manager);
  DCHECK(mi_widget_.get());

  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor = manager->GetCurrentInputMethod();
  const std::string short_name = UTF16ToUTF8(
      manager->GetInputMethodUtil()->GetInputMethodShortName(descriptor));
  mi_widget_->SetLabelTextUtf8(short_name);

  // Show the widget and hide it after 750msec.
  mi_widget_->Show();
  const int kDelayMSec = 750;
  mi_widget_->DelayHide(kDelayMSec);
}

}  // namespace input_method
}  // namespace chromeos
