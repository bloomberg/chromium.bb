// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

class SessionController;

// Controls the NightLight feature that adjusts the color temperature of the
// screen.
class ASH_EXPORT NightLightController : public SessionObserver {
 public:
  class Observer {
   public:
    // Emitted when the NightLight status is changed.
    virtual void OnNightLightEnabledChanged(bool enabled) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit NightLightController(SessionController* session_controller);
  ~NightLightController() override;

  // Returns true if the NightLight feature is enabled in the flags.
  static bool IsFeatureEnabled();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the NightLight settings stored in the current active user prefs.
  bool GetEnabled() const;
  float GetColorTemperature() const;

  // Set the desired NightLight settings in the current active user prefs.
  void SetEnabled(bool enabled);
  void SetColorTemperature(float temperature);

  void Toggle();

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  void Refresh();

  void StartWatchingPrefsChanges();

  void InitFromUserPrefs();

  void NotifyStatusChanged();

  // Called when the user pref for the enabled status of NightLight is changed.
  void OnEnabledPrefChanged();

  // Called when the user pref for the color temperature is changed.
  void OnColorTemperaturePrefChanged();

  // The observed session controller instance from which we know when to
  // initialize the NightLight settings from the user preferences.
  SessionController* const session_controller_;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  // The registrar used to watch NightLight prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the NightLight settings
  // controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NightLightController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
