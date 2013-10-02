// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_controller.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_widget.h"

namespace chromeos {
namespace input_method {

namespace {
// Show mode indicator with the current ime's short name.
void ShowModeIndicator(InputMethodManager* manager,
                       ModeIndicatorWidget* mi_widget) {
  DCHECK(manager);
  DCHECK(mi_widget);

  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor = manager->GetCurrentInputMethod();
  const std::string short_name = UTF16ToUTF8(
      manager->GetInputMethodUtil()->GetInputMethodShortName(descriptor));
  mi_widget->SetLabelTextUtf8(short_name);

  // Show the widget and hide it after 750msec.
  mi_widget->Show();
  const int kDelayMSec = 750;
  mi_widget->DelayHide(kDelayMSec);
}
}  // namespace

ModeIndicatorController::ModeIndicatorController(
    ModeIndicatorWidget *mi_widget) {
  mi_widget_.reset(mi_widget);
}

ModeIndicatorController::~ModeIndicatorController() {
}

void ModeIndicatorController::SetCursorLocation(
    const gfx::Rect& cursor_location) {
  mi_widget_->SetCursorLocation(cursor_location);
}

void ModeIndicatorController::InputMethodChanged(InputMethodManager* manager,
                                                 bool show_message) {
  if (!show_message)
    return;

  ShowModeIndicator(manager, mi_widget_.get());
}

void ModeIndicatorController::InputMethodPropertyChanged(
    InputMethodManager* manager) {
  // Do nothing.
}

}  // namespace input_method
}  // namespace chromeos
