// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_

#include "ash/shell_delegate.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace chromeos {

struct AccessibilityStatusEventDetails {
  AccessibilityStatusEventDetails(
      bool enabled,
      ash::AccessibilityNotificationVisibility notify);

  AccessibilityStatusEventDetails(
      bool enabled,
      ash::MagnifierType magnifier_type,
      ash::AccessibilityNotificationVisibility notify);

  bool enabled;
  ash::MagnifierType magnifier_type;
  ash::AccessibilityNotificationVisibility notify;
};

// AccessibilityManager changes the statuses of accessibility features
// watching profile notifications and pref-changes.
// TODO(yoshiki): merge MagnificationManager with AccessibilityManager.
class AccessibilityManager : public content::NotificationObserver {
 public:
  // Creates an instance of AccessibilityManager, this should be called once,
  // because only one instance should exist at the same time.
  static void Initialize();
  // Deletes the existing instance of AccessibilityManager.
  static void Shutdown();
  // Returns the existing instance. If there is no instance, returns NULL.
  static AccessibilityManager* Get();

  // On a user's first login into a device, any a11y features enabled/disabled
  // by the user on the login screen are enabled/disabled in the user's profile.
  // This class watches for profile changes and copies settings into the user's
  // profile when it detects a login with a newly created profile.
  class PrefHandler {
   public:
    explicit PrefHandler(const char* pref_path);
    virtual ~PrefHandler();

    // Should be called from AccessibilityManager::SetProfile().
    void HandleProfileChanged(Profile* previous_profile,
                              Profile* current_profile);

   private:
    const char* pref_path_;
  };

  // Enables or disables the large cursor.
  void EnableLargeCursor(bool enabled);
  // Returns true if the large cursor is enabled, or false if not.
  bool IsLargeCursorEnabled();

  // Enables or disable Sticky Keys.
  void EnableStickyKeys(bool enabled);

  // Returns true if the Sticky Keys is enabled, or false if not.
  bool IsStickyKeysEnabled();

  // Enables or disables spoken feedback. Enabling spoken feedback installs the
  // ChromeVox component extension.
  void EnableSpokenFeedback(bool enabled,
                            ash::AccessibilityNotificationVisibility notify);

  // Returns true if spoken feedback is enabled, or false if not.
  bool IsSpokenFeedbackEnabled();

  // Toggles whether Chrome OS spoken feedback is on or off.
  void ToggleSpokenFeedback(ash::AccessibilityNotificationVisibility notify);

  // Speaks the specified string.
  void Speak(const std::string& text);

  // Speaks the given text if the accessibility pref is already set.
  void MaybeSpeak(const std::string& text);

  // Enables or disables the high contrast mode for Chrome.
  void EnableHighContrast(bool enabled);

  // Returns true if High Contrast is enabled, or false if not.
  bool IsHighContrastEnabled();

  // Enables or disables autoclick.
  void EnableAutoclick(bool enabled);

  // Returns true if autoclick is enabled.
  bool IsAutoclickEnabled();

  // Set the delay for autoclicking after stopping the cursor in milliseconds.
  void SetAutoclickDelay(int delay_ms);

  // Returns the autoclick delay in milliseconds.
  int GetAutoclickDelay() const;

  void SetProfileForTest(Profile* profile);

 protected:
  AccessibilityManager();
  virtual ~AccessibilityManager();

 private:
  void LoadChromeVox();
  void LoadChromeVoxToUserScreen();
  void LoadChromeVoxToLockScreen();
  void UnloadChromeVox();
  void UnloadChromeVoxFromLockScreen();

  void UpdateLargeCursorFromPref();
  void UpdateStickyKeysFromPref();
  void UpdateSpokenFeedbackFromPref();
  void UpdateHighContrastFromPref();
  void UpdateAutoclickFromPref();
  void UpdateAutoclickDelayFromPref();
  void LocalePrefChanged();

  void SetProfile(Profile* profile);

  void UpdateChromeOSAccessibilityHistograms();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Profile which has the current a11y context.
  Profile* profile_;

  // Profile which ChromeVox is currently loaded to. If NULL, ChromeVox is not
  // loaded to any profile.
  bool chrome_vox_loaded_on_lock_screen_;
  bool chrome_vox_loaded_on_user_screen_;

  content::NotificationRegistrar notification_registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  scoped_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;

  PrefHandler large_cursor_pref_handler_;
  PrefHandler spoken_feedback_pref_handler_;
  PrefHandler high_contrast_pref_handler_;
  PrefHandler autoclick_pref_handler_;
  PrefHandler autoclick_delay_pref_handler_;

  bool large_cursor_enabled_;
  bool sticky_keys_enabled_;
  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool autoclick_enabled_;
  int autoclick_delay_ms_;

  ash::AccessibilityNotificationVisibility spoken_feedback_notification_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
