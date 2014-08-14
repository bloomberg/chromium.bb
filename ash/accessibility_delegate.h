// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DELEGATE_H_
#define ASH_ACCESSIBILITY_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/magnifier/magnifier_constants.h"
#include "base/time/time.h"

namespace ash {

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

enum AccessibilityAlert {
  A11Y_ALERT_NONE,
  A11Y_ALERT_WINDOW_NEEDED,
  A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED
};

// A delegate class to control and query accessibility features.
class ASH_EXPORT AccessibilityDelegate {
 public:
  virtual ~AccessibilityDelegate() {}

  // Invoked to toggle spoken feedback for accessibility
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) = 0;

  // Returns true if spoken feedback is enabled.
  virtual bool IsSpokenFeedbackEnabled() const = 0;

  // Invoked to toggle high contrast mode for accessibility.
  virtual void ToggleHighContrast() = 0;

  // Returns true if high contrast mode is enabled.
  virtual bool IsHighContrastEnabled() const = 0;

  // Invoked to enable the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Invoked to change the type of the screen magnifier.
  virtual void SetMagnifierType(MagnifierType type) = 0;

  // Returns true if the screen magnifier is enabled.
  virtual bool IsMagnifierEnabled() const = 0;

  // Returns the current screen magnifier mode.
  virtual MagnifierType GetMagnifierType() const = 0;

  // Invoked to enable Large Cursor.
  virtual void SetLargeCursorEnabled(bool enabled) = 0;

  // Returns ture if Large Cursor is enabled.
  virtual bool IsLargeCursorEnabled() const = 0;

  // Invoked to enable autoclick.
  virtual void SetAutoclickEnabled(bool enabled) = 0;

  // Returns if autoclick is enabled or not.
  virtual bool IsAutoclickEnabled() const = 0;

  // Invoked to enable or disable the a11y on-screen keyboard.
  virtual void SetVirtualKeyboardEnabled(bool enabled) = 0;

  // Returns if the a11y virtual keyboard is enabled.
  virtual bool IsVirtualKeyboardEnabled() const = 0;

  // Returns true when the accessibility menu should be shown.
  virtual bool ShouldShowAccessibilityMenu() const = 0;

  // Returns true if a braille display is connected to the system.
  virtual bool IsBrailleDisplayConnected() const = 0;

  // Cancel all current and queued speech immediately.
  virtual void SilenceSpokenFeedback() const = 0;

  // Saves the zoom scale of the full screen magnifier.
  virtual void SaveScreenMagnifierScale(double scale) = 0;

  // Gets a saved value of the zoom scale of full screen magnifier. If a value
  // is not saved, return a negative value.
  virtual double GetSavedScreenMagnifierScale() = 0;

  // Triggers an accessibility alert to give the user feedback.
  virtual void TriggerAccessibilityAlert(AccessibilityAlert alert) = 0;

  // Gets the last accessibility alert that was triggered.
  virtual AccessibilityAlert GetLastAccessibilityAlert() = 0;

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // when their mapped event has occurred. The sound key enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  virtual void PlayEarcon(int sound_key) = 0;

  // Initiates play of shutdown sound and returns it's duration.
  virtual base::TimeDelta PlayShutdownSound() const = 0;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DELEGATE_H_
