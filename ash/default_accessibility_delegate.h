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

  virtual bool IsSpokenFeedbackEnabled() const override;
  virtual void ToggleHighContrast() override;
  virtual bool IsHighContrastEnabled() const override;
  virtual void SetMagnifierEnabled(bool enabled) override;
  virtual void SetMagnifierType(MagnifierType type) override;
  virtual bool IsMagnifierEnabled() const override;
  virtual MagnifierType GetMagnifierType() const override;
  virtual void SetLargeCursorEnabled(bool enabled) override;
  virtual bool IsLargeCursorEnabled() const override;
  virtual void SetAutoclickEnabled(bool enabled) override;
  virtual bool IsAutoclickEnabled() const override;
  virtual void SetVirtualKeyboardEnabled(bool enabled) override;
  virtual bool IsVirtualKeyboardEnabled() const override;
  virtual bool ShouldShowAccessibilityMenu() const override;
  virtual bool IsBrailleDisplayConnected() const override;
  virtual void SilenceSpokenFeedback() const override;
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) override;
  virtual void SaveScreenMagnifierScale(double scale) override;
  virtual double GetSavedScreenMagnifierScale() override;
  virtual void TriggerAccessibilityAlert(AccessibilityAlert alert) override;
  virtual AccessibilityAlert GetLastAccessibilityAlert() override;
  virtual void PlayEarcon(int sound_key) override;
  virtual base::TimeDelta PlayShutdownSound() const override;

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
