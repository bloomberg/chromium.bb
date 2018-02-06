// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_
#define ASH_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/docked_magnifier_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;

namespace ash {

// Controls the Docked Magnifier (a.k.a. picture-in-picture magnifier) feature,
// which allocates the top portion of the currently active display as a
// magnified viewport of an area around the point of interest in the screen
// (which follows the cursor location, text input caret location, or focus
// changes).
class ASH_EXPORT DockedMagnifierController
    : public mojom::DockedMagnifierController,
      public SessionObserver {
 public:
  DockedMagnifierController();
  ~DockedMagnifierController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void BindRequest(mojom::DockedMagnifierControllerRequest request);

  // Get the Docked Magnifier settings for the current active user prefs.
  bool GetEnabled() const;
  float GetScale() const;

  // Set the Docked Magnifier settings in the current active user prefs.
  void SetEnabled(bool enabled);
  void SetScale(float scale);

  // ash::mojom::DockedMagnifierController:
  void SetClient(mojom::DockedMagnifierClientPtr client) override;
  void CenterOnPoint(const gfx::Point& point_in_screen) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void FlushClientPtrForTesting();

 private:
  void InitFromUserPrefs();

  // Handlers of prefs changes.
  void OnEnabledPrefChanged();
  void OnScalePrefChanged();

  void Refresh();

  void NotifyClientWithStatusChanged();

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  mojo::Binding<mojom::DockedMagnifierController> binding_;

  mojom::DockedMagnifierClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(DockedMagnifierController);
};

}  // namespace ash

#endif  // ASH_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_
