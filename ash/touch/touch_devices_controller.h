// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_
#define ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {

// Different sources for requested touchscreen enabled/disabled state.
enum class TouchscreenEnabledSource {
  // User-requested state set via a debug accelerator and stored in a pref.
  USER_PREF,
  // Transient global state used to disable touchscreen via power button.
  GLOBAL,
};

// Controls the enabled state of touchpad and touchscreen. Must be initialized
// after Shell and SessionController.
class ASH_EXPORT TouchDevicesController : public SessionObserver {
 public:
  TouchDevicesController();
  ~TouchDevicesController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Toggles the status of touchpad between enabled and disabled.
  void ToggleTouchpad();

  // Returns the current touchscreen enabled status as specified by |source|.
  // Note that the actual state of the touchscreen device is automatically
  // determined based on the requests of multiple sources.
  bool GetTouchscreenEnabled(TouchscreenEnabledSource source) const;

  // Sets |source|'s requested touchscreen enabled status to |enabled|. Note
  // that the actual state of the touchscreen device is automatically determined
  // based on the requests of multiple sources.
  void SetTouchscreenEnabled(bool enabled, TouchscreenEnabledSource source);

 private:
  // Overridden from SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial state.
  void ObservePrefs(PrefService* prefs);

  // Updates the actual enabled/disabled status of the touchpad.
  void UpdateTouchpadEnabled();

  // Updates the actual enabled/disabled status of the touchscreen. Touchscreen
  // is enabled if all the touchscreen enabled sources are enabled.
  void UpdateTouchscreenEnabled();

  // The touchscreen state which is associated with the global touchscreen
  // enabled source.
  bool global_touchscreen_enabled_ = true;

  // Observes user profile prefs for touch devices.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TouchDevicesController);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_DEVICES_CONTROLLER_H_
