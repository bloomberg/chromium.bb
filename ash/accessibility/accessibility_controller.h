// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

#include <memory>

#include "ash/ash_constants.h"
#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityController : public SessionObserver {
 public:
  AccessibilityController();
  ~AccessibilityController() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  void SetLargeCursorEnabled(bool enabled);
  bool IsLargeCursorEnabled() const;

  // Returns true if an accessibility feature is enabled that requires cursor
  // compositing.
  static bool RequiresCursorCompositing(PrefService* prefs);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  void SetPrefServiceForTest(PrefService* prefs);

 private:
  // Before login returns the signin screen profile prefs. After login returns
  // the active user profile prefs.
  PrefService* GetActivePrefService() const;

  void UpdateLargeCursorFromPref();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool large_cursor_enabled_ = false;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;

  PrefService* pref_service_for_test_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
