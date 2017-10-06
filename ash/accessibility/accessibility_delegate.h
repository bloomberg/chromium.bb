// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility_types.h"
#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.h"

namespace ash {

// A delegate class to control and query accessibility features.
//
// NOTE: Methods in this class are migrating to AccessibilityController to
// support mash (because ash is in a separate process and cannot call back into
// chrome). Add new methods there, not here.
class ASH_EXPORT AccessibilityDelegate {
 public:
  virtual ~AccessibilityDelegate() {}

  // Invoked to toggle spoken feedback for accessibility
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) = 0;

  // Returns true if spoken feedback is enabled.
  virtual bool IsSpokenFeedbackEnabled() const = 0;

  // Invoked to enable the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Returns true if the screen magnifier is enabled.
  virtual bool IsMagnifierEnabled() const = 0;

  // Invoked to enable autoclick.
  virtual void SetAutoclickEnabled(bool enabled) = 0;

  // Returns if autoclick is enabled or not.
  virtual bool IsAutoclickEnabled() const = 0;

  // Invoked to enable or disable the a11y on-screen keyboard.
  virtual void SetVirtualKeyboardEnabled(bool enabled) = 0;

  // Returns if the a11y virtual keyboard is enabled.
  virtual bool IsVirtualKeyboardEnabled() const = 0;

  // Invoked to enable or disable the mono audio output.
  virtual void SetMonoAudioEnabled(bool enabled) = 0;

  // Returns if the mono audio output is enabled.
  virtual bool IsMonoAudioEnabled() const = 0;

  // Invoked to enable or disable caret highlighting.
  virtual void SetCaretHighlightEnabled(bool enabled) = 0;

  // Returns if caret highlighting is enabled.
  virtual bool IsCaretHighlightEnabled() const = 0;

  // Invoked to enable or disable cursor highlighting.
  virtual void SetCursorHighlightEnabled(bool enabled) = 0;

  // Returns if cursor highlighting is enabled.
  virtual bool IsCursorHighlightEnabled() const = 0;

  // Invoked to enable or disable focus highlighting.
  virtual void SetFocusHighlightEnabled(bool enabled) = 0;

  // Returns if focus highlighting is enabled.
  virtual bool IsFocusHighlightEnabled() const = 0;

  // Invoked to enable or disable sticky keys.
  virtual void SetStickyKeysEnabled(bool enabled) = 0;

  // Returns if sticky keys is enabled.
  virtual bool IsStickyKeysEnabled() const = 0;

  // Invoked to enable or disable tap dragging.
  virtual void SetTapDraggingEnabled(bool enabled) = 0;

  // Returns if tap dragging is enabled.
  virtual bool IsTapDraggingEnabled() const = 0;

  // Invoked to enable or disable select-to-speak.
  virtual void SetSelectToSpeakEnabled(bool enabled) = 0;

  // Returns if select-to-speak is enabled.
  virtual bool IsSelectToSpeakEnabled() const = 0;

  // Invoked to enable or disable switch access.
  virtual void SetSwitchAccessEnabled(bool enabled) = 0;

  // Returns if switch access is enabled.
  virtual bool IsSwitchAccessEnabled() const = 0;

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

  // Called when we first detect two fingers are held down, which can be
  // used to toggle spoken feedback on some touch-only devices.
  virtual void OnTwoFingerTouchStart() {}

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  virtual void OnTwoFingerTouchStop() {}

  // Whether or not to enable toggling spoken feedback via holding down
  // two fingers on the screen.
  virtual bool ShouldToggleSpokenFeedbackViaTouch() = 0;

  // Play tick sound indicating spoken feedback will be toggled after countdown.
  virtual void PlaySpokenFeedbackToggleCountdown(int tick_count) = 0;

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // when their mapped event has occurred. The sound key enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  virtual void PlayEarcon(int sound_key) = 0;

  // Initiates play of shutdown sound and returns it's duration.
  virtual base::TimeDelta PlayShutdownSound() const = 0;

  // Forward an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  virtual void HandleAccessibilityGesture(ui::AXGesture gesture) = 0;

  // NOTE: Prefer adding methods to AccessibilityController, see class comment.
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_
