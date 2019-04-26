// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CONTROLLER_H_
#define CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CONTROLLER_H_

#include "base/macros.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/power/public/mojom/power_manager.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

// PowerManagerMojoController simply passes incoming mojo messages through to
// the D-Bus-backed (or fake) PowerManagerClient. It informs |client_| of
// changes by passing along PowerManagerClient::Observer notifications.
class COMPONENT_EXPORT(CHROMEOS_POWER_MOJO_CONTROLLER)
    PowerManagerMojoController : public power::mojom::PowerManagerController,
                                 public PowerManagerClient::Observer {
 public:
  PowerManagerMojoController();
  ~PowerManagerMojoController() override;

  void BindRequest(power::mojom::PowerManagerControllerRequest request);

  // mojom::PowerManagerMojoController:
  void SetObserver(
      power::mojom::PowerManagerObserverAssociatedPtrInfo client) override;
  void SetScreenBrightness(
      const power_manager::SetBacklightBrightnessRequest& request) override;
  void GetScreenBrightnessPercent(
      GetScreenBrightnessPercentCallback callback) override;
  void RequestRestart(power_manager::RequestRestartReason reason,
                      const std::string& description) override;

  // PowerManagerClient::Observer:
  void PowerManagerBecameAvailable(bool available) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void KeyboardBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

 private:
  mojo::BindingSet<power::mojom::PowerManagerController> binding_set_;
  power::mojom::PowerManagerObserverAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerMojoController);
};

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_POWER_PUBLIC_CPP_POWER_MANAGER_MOJO_CONTROLLER_H_
