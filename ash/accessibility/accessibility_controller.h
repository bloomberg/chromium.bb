// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

#include <memory>

#include "ash/ash_constants.h"
#include "ash/ash_export.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace service_manager {
class Connector;
}

namespace ash {

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityController
    : public mojom::AccessibilityController,
      public SessionObserver {
 public:
  explicit AccessibilityController(service_manager::Connector* connector);
  ~AccessibilityController() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Binds the mojom::AccessibilityController interface to this object.
  void BindRequest(mojom::AccessibilityControllerRequest request);

  void SetLargeCursorEnabled(bool enabled);
  bool IsLargeCursorEnabled() const;

  void SetHighContrastEnabled(bool enabled);
  bool IsHighContrastEnabled() const;

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(mojom::AccessibilityAlert alert);

  // mojom::AccessibilityController:
  void SetClient(mojom::AccessibilityControllerClientPtr client) override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Test helpers:
  void FlushMojoForTest();

 private:
  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  void UpdateLargeCursorFromPref();
  void UpdateHighContrastFromPref();

  service_manager::Connector* connector_ = nullptr;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Binding for mojom::AccessibilityController interface.
  mojo::Binding<mojom::AccessibilityController> binding_;

  // Client interface in chrome browser.
  mojom::AccessibilityControllerClientPtr client_;

  bool large_cursor_enabled_ = false;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;
  bool high_contrast_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
