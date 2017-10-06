// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/default_accessibility_delegate.h"

#include <limits>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

DefaultAccessibilityDelegate::DefaultAccessibilityDelegate() {}

DefaultAccessibilityDelegate::~DefaultAccessibilityDelegate() {}

bool DefaultAccessibilityDelegate::IsSpokenFeedbackEnabled() const {
  return spoken_feedback_enabled_;
}

void DefaultAccessibilityDelegate::SetMagnifierEnabled(bool enabled) {
  screen_magnifier_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsMagnifierEnabled() const {
  return screen_magnifier_enabled_;
}

void DefaultAccessibilityDelegate::SetAutoclickEnabled(bool enabled) {
  autoclick_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsAutoclickEnabled() const {
  return autoclick_enabled_;
}

void DefaultAccessibilityDelegate::SetVirtualKeyboardEnabled(bool enabled) {
  virtual_keyboard_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsVirtualKeyboardEnabled() const {
  return virtual_keyboard_enabled_;
}

void DefaultAccessibilityDelegate::SetMonoAudioEnabled(bool enabled) {
  mono_audio_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsMonoAudioEnabled() const {
  return mono_audio_enabled_;
}

void DefaultAccessibilityDelegate::SetCaretHighlightEnabled(bool enabled) {
  caret_highlight_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsCaretHighlightEnabled() const {
  return caret_highlight_enabled_;
}

void DefaultAccessibilityDelegate::SetCursorHighlightEnabled(bool enabled) {
  cursor_highlight_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsCursorHighlightEnabled() const {
  return cursor_highlight_enabled_;
}

void DefaultAccessibilityDelegate::SetFocusHighlightEnabled(bool enabled) {
  focus_highligh_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsFocusHighlightEnabled() const {
  return focus_highligh_enabled_;
}

void DefaultAccessibilityDelegate::SetStickyKeysEnabled(bool enabled) {
  sticky_keys_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsStickyKeysEnabled() const {
  return sticky_keys_enabled_;
}

void DefaultAccessibilityDelegate::SetTapDraggingEnabled(bool enabled) {
  tap_dragging_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsTapDraggingEnabled() const {
  return tap_dragging_enabled_;
}

void DefaultAccessibilityDelegate::SetSelectToSpeakEnabled(bool enabled) {
  select_to_speak_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsSelectToSpeakEnabled() const {
  return select_to_speak_enabled_;
}

void DefaultAccessibilityDelegate::SetSwitchAccessEnabled(bool enabled) {
  switch_access_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsSwitchAccessEnabled() const {
  return switch_access_enabled_;
}

bool DefaultAccessibilityDelegate::ShouldShowAccessibilityMenu() const {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  return spoken_feedback_enabled_ || screen_magnifier_enabled_ ||
         autoclick_enabled_ || virtual_keyboard_enabled_ ||
         mono_audio_enabled_ || controller->IsLargeCursorEnabled() ||
         controller->IsHighContrastEnabled();
}

bool DefaultAccessibilityDelegate::IsBrailleDisplayConnected() const {
  return false;
}

void DefaultAccessibilityDelegate::SilenceSpokenFeedback() const {}

void DefaultAccessibilityDelegate::ToggleSpokenFeedback(
    AccessibilityNotificationVisibility notify) {
  spoken_feedback_enabled_ = !spoken_feedback_enabled_;
}

void DefaultAccessibilityDelegate::SaveScreenMagnifierScale(double scale) {}

double DefaultAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

void DefaultAccessibilityDelegate::TriggerAccessibilityAlert(
    AccessibilityAlert alert) {
  accessibility_alert_ = alert;
}

AccessibilityAlert DefaultAccessibilityDelegate::GetLastAccessibilityAlert() {
  return accessibility_alert_;
}

bool DefaultAccessibilityDelegate::ShouldToggleSpokenFeedbackViaTouch() {
  return false;
}

void DefaultAccessibilityDelegate::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {}

void DefaultAccessibilityDelegate::PlayEarcon(int sound_key) {}

base::TimeDelta DefaultAccessibilityDelegate::PlayShutdownSound() const {
  return base::TimeDelta();
}

void DefaultAccessibilityDelegate::HandleAccessibilityGesture(
    ui::AXGesture gesture) {}

}  // namespace ash
