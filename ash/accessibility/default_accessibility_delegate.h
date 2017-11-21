// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_
#define ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_types.h"
#include "base/macros.h"

namespace ash {

class ASH_EXPORT DefaultAccessibilityDelegate : public AccessibilityDelegate {
 public:
  DefaultAccessibilityDelegate();
  ~DefaultAccessibilityDelegate() override;

  bool IsSpokenFeedbackEnabled() const override;
  void SetMagnifierEnabled(bool enabled) override;
  bool IsMagnifierEnabled() const override;
  void SetAutoclickEnabled(bool enabled) override;
  bool IsAutoclickEnabled() const override;
  void SetVirtualKeyboardEnabled(bool enabled) override;
  bool IsVirtualKeyboardEnabled() const override;
  void SetCaretHighlightEnabled(bool enabled) override;
  bool IsCaretHighlightEnabled() const override;
  void SetCursorHighlightEnabled(bool enabled) override;
  bool IsCursorHighlightEnabled() const override;
  void SetFocusHighlightEnabled(bool enabled) override;
  bool IsFocusHighlightEnabled() const override;
  void SetStickyKeysEnabled(bool enabled) override;
  bool IsStickyKeysEnabled() const override;
  void SetTapDraggingEnabled(bool enabled) override;
  bool IsTapDraggingEnabled() const override;
  void SetSelectToSpeakEnabled(bool enabled) override;
  bool IsSelectToSpeakEnabled() const override;
  void SetSwitchAccessEnabled(bool enabled) override;
  bool IsSwitchAccessEnabled() const override;
  bool ShouldShowAccessibilityMenu() const override;
  bool IsBrailleDisplayConnected() const override;
  void SilenceSpokenFeedback() const override;
  void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) override;
  void SaveScreenMagnifierScale(double scale) override;
  double GetSavedScreenMagnifierScale() override;
  bool ShouldToggleSpokenFeedbackViaTouch() override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void PlayEarcon(int sound_key) override;
  base::TimeDelta PlayShutdownSound() const override;
  void HandleAccessibilityGesture(ui::AXGesture gesture) override;

 private:
  bool spoken_feedback_enabled_ = false;
  bool screen_magnifier_enabled_ = false;
  bool autoclick_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool caret_highlight_enabled_ = false;
  bool cursor_highlight_enabled_ = false;
  bool focus_highligh_enabled_ = false;
  bool sticky_keys_enabled_ = false;
  bool tap_dragging_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool switch_access_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityDelegate);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_
