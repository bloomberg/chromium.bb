// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/power/public/cpp/power_manager_mojo_controller.h"

#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {

PowerManagerMojoController::PowerManagerMojoController() = default;

PowerManagerMojoController::~PowerManagerMojoController() {
  if (PowerManagerClient::Get()->HasObserver(this))
    PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerManagerMojoController::BindRequest(
    power::mojom::PowerManagerControllerRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

void PowerManagerMojoController::SetObserver(
    power::mojom::PowerManagerObserverAssociatedPtrInfo client) {
  DCHECK(!client_.is_bound());
  client_.Bind(std::move(client));

  PowerManagerClient::Get()->AddObserver(this);
}

void PowerManagerMojoController::SetScreenBrightness(
    const power_manager::SetBacklightBrightnessRequest& request) {
  PowerManagerClient::Get()->SetScreenBrightness(request);
}

void PowerManagerMojoController::GetScreenBrightnessPercent(
    GetScreenBrightnessPercentCallback callback) {
  // This extra thunk is only necessary to convert between
  // power::mojom::BrightnessPtr and base::Optional<double>.
  PowerManagerClient::Get()->GetScreenBrightnessPercent(base::BindOnce(
      [](GetScreenBrightnessPercentCallback callback,
         base::Optional<double> value) {
        if (value)
          std::move(callback).Run(power::mojom::Brightness::New(*value));
        else
          std::move(callback).Run({});
      },
      std::move(callback)));
}

void PowerManagerMojoController::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  PowerManagerClient::Get()->RequestRestart(reason, description);
}

void PowerManagerMojoController::PowerManagerBecameAvailable(bool available) {
  client_->PowerManagerBecameAvailable(available);
}

void PowerManagerMojoController::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  client_->ScreenBrightnessChanged(change);
}

void PowerManagerMojoController::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  client_->KeyboardBrightnessChanged(change);
}

void PowerManagerMojoController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  client_->SuspendDone(sleep_duration);
}

}  // namespace chromeos
