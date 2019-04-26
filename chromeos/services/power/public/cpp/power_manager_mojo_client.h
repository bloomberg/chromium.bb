// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CLIENT_H_
#define CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace chromeos {

// PowerManagerMojoClient implements the PowerManagerClient interface (which is
// shared with the D-Bus implementation) and forwards calls over mojo to
// PowerManagerMojoController.
class COMPONENT_EXPORT(CHROMEOS_POWER_MOJO_CLIENT) PowerManagerMojoClient
    : public PowerManagerClient,
      public power::mojom::PowerManagerObserver {
 public:
  PowerManagerMojoClient();
  ~PowerManagerMojoClient() override;

  // PowerManagerClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void SetRenderProcessManagerDelegate(
      base::WeakPtr<RenderProcessManagerDelegate> delegate) override;
  void DecreaseScreenBrightness(bool allow_off) override;
  void IncreaseScreenBrightness() override;
  void SetScreenBrightness(
      const power_manager::SetBacklightBrightnessRequest& request) override;
  void GetScreenBrightnessPercent(DBusMethodCallback<double> callback) override;
  void DecreaseKeyboardBrightness() override;
  void IncreaseKeyboardBrightness() override;
  void GetKeyboardBrightnessPercent(
      DBusMethodCallback<double> callback) override;
  const base::Optional<power_manager::PowerSupplyProperties>& GetLastStatus()
      override;
  void RequestStatusUpdate() override;
  void RequestSuspend() override;
  void RequestRestart(power_manager::RequestRestartReason reason,
                      const std::string& description) override;
  void RequestShutdown(power_manager::RequestShutdownReason reason,
                       const std::string& description) override;
  void NotifyUserActivity(power_manager::UserActivityType type) override;
  void NotifyVideoActivity(bool is_fullscreen) override;
  void NotifyWakeNotification() override;
  void SetPolicy(const power_manager::PowerManagementPolicy& policy) override;
  void SetIsProjecting(bool is_projecting) override;
  void SetPowerSource(const std::string& id) override;
  void SetBacklightsForcedOff(bool forced_off) override;
  void GetBacklightsForcedOff(DBusMethodCallback<bool> callback) override;
  void GetSwitchStates(DBusMethodCallback<SwitchStates> callback) override;
  void GetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback)
      override;
  void BlockSuspend(const base::UnguessableToken& token,
                    const std::string& debug_info) override;
  void UnblockSuspend(const base::UnguessableToken& token) override;
  void CreateArcTimers(
      const std::string& tag,
      std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
      DBusMethodCallback<std::vector<TimerId>> callback) override;
  void StartArcTimer(TimerId timer_id,
                     base::TimeTicks absolute_expiration_time,
                     VoidDBusMethodCallback callback) override;
  void DeleteArcTimers(const std::string& tag,
                       VoidDBusMethodCallback callback) override;
  void DeferScreenDim() override;

  // power::mojom::PowerManagerObserver:
  void PowerManagerBecameAvailable(bool available) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void KeyboardBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  power::mojom::PowerManagerControllerPtr* interface_ptr() {
    return &controller_;
  }

  // Init done after |controller_| is bound.
  void InitAfterInterfaceBound();

  // Returns the chromeos::PowerManagerClient implementation, which may either
  // be backed by D-Bus or a fake (if mojo is not enabled), or a
  // PowerManagerMojoClient. This only exists for the sake of SingleProcessMash,
  // where a PowerManagerMojoClient and a PowerManagerClientImpl may coexist in
  // the same process. Otherwise, PowerManagerClient::Get would be enough.
  static ::chromeos::PowerManagerClient* Get();

 private:
  power::mojom::PowerManagerControllerPtr controller_;
  mojo::AssociatedBinding<power::mojom::PowerManagerObserver> binding_{this};

  base::ObserverList<Observer>::Unchecked observers_;

  // The last proto received via mojo; initially empty.
  base::Optional<power_manager::PowerSupplyProperties> proto_;

  base::Optional<bool> service_available_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerMojoClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CLIENT_H_
