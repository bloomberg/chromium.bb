// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEFAULT_ACCESSIBILITY_DELEGATE_H_
#define ASH_DEFAULT_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility_delegate.h"
#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {

class ASH_EXPORT DefaultAccessibilityDelegate : public AccessibilityDelegate {
 public:
  DefaultAccessibilityDelegate();
  virtual ~DefaultAccessibilityDelegate();

  virtual bool IsSpokenFeedbackEnabled() const OVERRIDE;
  virtual void ToggleHighContrast() OVERRIDE;
  virtual bool IsHighContrastEnabled() const OVERRIDE;
  virtual void SetMagnifierEnabled(bool enabled) OVERRIDE;
  virtual void SetMagnifierType(MagnifierType type) OVERRIDE;
  virtual bool IsMagnifierEnabled() const OVERRIDE;
  virtual MagnifierType GetMagnifierType() const OVERRIDE;
  virtual void SetLargeCursorEnabled(bool enabled) OVERRIDE;
  virtual bool IsLargeCursorEnabled() const OVERRIDE;
  virtual void SetAutoclickEnabled(bool enabled) OVERRIDE;
  virtual bool IsAutoclickEnabled() const OVERRIDE;
  virtual void SetVirtualKeyboardEnabled(bool enabled) OVERRIDE;
  virtual bool IsVirtualKeyboardEnabled() const OVERRIDE;
  virtual bool ShouldShowAccessibilityMenu() const OVERRIDE;
  virtual bool IsBrailleDisplayConnected() const OVERRIDE;
  virtual void SilenceSpokenFeedback() const OVERRIDE;
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) OVERRIDE;
  virtual void SaveScreenMagnifierScale(double scale) OVERRIDE;
  virtual double GetSavedScreenMagnifierScale() OVERRIDE;
  virtual void TriggerAccessibilityAlert(AccessibilityAlert alert) OVERRIDE;
  virtual AccessibilityAlert GetLastAccessibilityAlert() OVERRIDE;
  virtual void PlayEarcon(int sound_key) OVERRIDE;
  virtual base::TimeDelta PlayShutdownSound() const OVERRIDE;

 private:
  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  MagnifierType screen_magnifier_type_;
  bool large_cursor_enabled_;
  bool autoclick_enabled_;
  bool virtual_keyboard_enabled_;
  AccessibilityAlert accessibility_alert_;
  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityDelegate);
};

}  // namespace ash

#endif  // DEFAULT_ACCESSIBILITY_DELEGATE_H_
