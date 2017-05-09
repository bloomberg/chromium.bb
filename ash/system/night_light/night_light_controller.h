// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/observer_list.h"

class PrefRegistrySimple;

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

  static void RegisterPrefs(PrefRegistrySimple* registry);

  float color_temperature() const { return color_temperature_; }
  bool enabled() const { return enabled_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Toggle();

  void SetEnabled(bool enabled);

  // Set the screen color temperature. |temperature| should be a value from
  // 0.0f (least warm) to 1.0f (most warm).
  void SetColorTemperature(float temperature);

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  void Refresh();

  void InitFromUserPrefs();

  void PersistUserPrefs();

  void NotifyStatusChanged();

  // The observed session controller instance from which we know when to
  // initialize the NightLight settings from the user preferences.
  SessionController* const session_controller_;

  // The applied color temperature value when NightLight is turned ON. It's a
  // value from 0.0f (least warm, default display color) to 1.0f (most warm).
  float color_temperature_ = 0.5f;

  bool enabled_ = false;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NightLightController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
