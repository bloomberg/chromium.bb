// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_change_handler.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

namespace chromeos {
namespace system {

DeviceChangeHandler::DeviceChangeHandler()
    : pointer_device_observer_(new PointerDeviceObserver) {
  pointer_device_observer_->AddObserver(this);
  pointer_device_observer_->Init();

  // Apply settings on startup.
  TouchpadExists(true);
  MouseExists(true);
}

DeviceChangeHandler::~DeviceChangeHandler() {
  pointer_device_observer_->RemoveObserver(this);
}

// When we detect a touchpad is attached, apply touchpad settings of the last
// used profile.
void DeviceChangeHandler::TouchpadExists(bool exists) {
  if (!exists)
    return;

  // Using GetDefaultProfile here because GetLastUsedProfile returns the
  // LoginManager profile in browser tests.
  PrefService* prefs =
      g_browser_process->profile_manager()->GetDefaultProfile()->GetPrefs();

  const bool tap_dragging = prefs->GetBoolean(prefs::kTapDraggingEnabled);
  system::touchpad_settings::SetTapDragging(tap_dragging);

  const bool three_finger_click =
      prefs->GetBoolean(prefs::kEnableTouchpadThreeFingerClick);
  system::touchpad_settings::SetThreeFingerClick(three_finger_click);

  const int sensitivity = prefs->GetInteger(prefs::kTouchpadSensitivity);
  system::touchpad_settings::SetSensitivity(sensitivity);

  // If we are not logged in, use owner preferences.
  PrefService* local_prefs = g_browser_process->local_state();
  const bool tap_to_click =
      g_browser_process->profile_manager()->IsLoggedIn() ?
          prefs->GetBoolean(prefs::kTapToClickEnabled) :
          local_prefs->GetBoolean(prefs::kOwnerTapToClickEnabled);
  system::touchpad_settings::SetTapToClick(tap_to_click);
}

// When we detect a mouse is attached, apply mouse settings of the last
// used profile.
void DeviceChangeHandler::MouseExists(bool exists) {
  if (!exists)
    return;

  // Using GetDefaultProfile here because GetLastUsedProfile returns the
  // LoginManager profile in browser tests.
  PrefService* prefs =
      g_browser_process->profile_manager()->GetDefaultProfile()->GetPrefs();

  const int sensitivity = prefs->GetInteger(prefs::kMouseSensitivity);
  system::mouse_settings::SetSensitivity(sensitivity);

  // If we are not logged in, use owner preferences.
  PrefService* local_prefs = g_browser_process->local_state();
  const bool primary_button_right =
      g_browser_process->profile_manager()->IsLoggedIn() ?
          prefs->GetBoolean(prefs::kPrimaryMouseButtonRight) :
          local_prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight);
  system::mouse_settings::SetPrimaryButtonRight(primary_button_right);
}

}  // namespace system
}  // namespace chromeos

