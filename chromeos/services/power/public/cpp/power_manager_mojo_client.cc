// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/power/public/cpp/power_manager_mojo_client.h"

#include "base/feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {

namespace {
PowerManagerMojoClient* g_instance = nullptr;
}  // namespace

PowerManagerMojoClient::PowerManagerMojoClient() {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMojoDBusRelay));
  DCHECK(!g_instance);
  g_instance = this;
}

PowerManagerMojoClient::~PowerManagerMojoClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void PowerManagerMojoClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (service_available_)
    observer->PowerManagerBecameAvailable(*service_available_);
}

void PowerManagerMojoClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PowerManagerMojoClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void PowerManagerMojoClient::SetRenderProcessManagerDelegate(
    base::WeakPtr<RenderProcessManagerDelegate> delegate) {}

void PowerManagerMojoClient::DecreaseScreenBrightness(bool allow_off) {}

void PowerManagerMojoClient::IncreaseScreenBrightness() {}

void PowerManagerMojoClient::SetScreenBrightness(
    const power_manager::SetBacklightBrightnessRequest& request) {}

void PowerManagerMojoClient::GetScreenBrightnessPercent(
    DBusMethodCallback<double> callback) {
  // This extra thunk is only necessary to convert between
  // power::mojom::BrightnessPtr and base::Optional<double>.
  controller_->GetScreenBrightnessPercent(base::BindOnce(
      [](DBusMethodCallback<double> callback,
         power::mojom::BrightnessPtr brightness) {
        if (brightness)
          std::move(callback).Run(brightness->value);
        else
          std::move(callback).Run(base::nullopt);
      },
      std::move(callback)));
}

void PowerManagerMojoClient::DecreaseKeyboardBrightness() {}

void PowerManagerMojoClient::IncreaseKeyboardBrightness() {}

void PowerManagerMojoClient::GetKeyboardBrightnessPercent(
    DBusMethodCallback<double> callback) {}

const base::Optional<power_manager::PowerSupplyProperties>&
PowerManagerMojoClient::GetLastStatus() {
  return proto_;
}

void PowerManagerMojoClient::RequestStatusUpdate() {}

void PowerManagerMojoClient::RequestSuspend() {}

void PowerManagerMojoClient::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  controller_->RequestRestart(reason, description);
}

void PowerManagerMojoClient::RequestShutdown(
    power_manager::RequestShutdownReason reason,
    const std::string& description) {}

void PowerManagerMojoClient::NotifyUserActivity(
    power_manager::UserActivityType type) {}

void PowerManagerMojoClient::NotifyVideoActivity(bool is_fullscreen) {}

void PowerManagerMojoClient::NotifyWakeNotification() {}

void PowerManagerMojoClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {}

void PowerManagerMojoClient::SetIsProjecting(bool is_projecting) {}

void PowerManagerMojoClient::SetPowerSource(const std::string& id) {}

void PowerManagerMojoClient::SetBacklightsForcedOff(bool forced_off) {}

void PowerManagerMojoClient::GetBacklightsForcedOff(
    DBusMethodCallback<bool> callback) {}

void PowerManagerMojoClient::GetSwitchStates(
    DBusMethodCallback<SwitchStates> callback) {}

void PowerManagerMojoClient::GetInactivityDelays(
    DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback) {
}

void PowerManagerMojoClient::BlockSuspend(const base::UnguessableToken& token,
                                          const std::string& debug_info) {}

void PowerManagerMojoClient::UnblockSuspend(
    const base::UnguessableToken& token) {}

void PowerManagerMojoClient::CreateArcTimers(
    const std::string& tag,
    std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
    DBusMethodCallback<std::vector<TimerId>> callback) {}

void PowerManagerMojoClient::StartArcTimer(
    TimerId timer_id,
    base::TimeTicks absolute_expiration_time,
    VoidDBusMethodCallback callback) {}

void PowerManagerMojoClient::DeleteArcTimers(const std::string& tag,
                                             VoidDBusMethodCallback callback) {}

void PowerManagerMojoClient::DeferScreenDim() {}

void PowerManagerMojoClient::PowerManagerBecameAvailable(bool available) {
  service_available_ = available;
  for (auto& observer : observers_)
    observer.PowerManagerBecameAvailable(available);
}

void PowerManagerMojoClient::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (auto& observer : observers_)
    observer.ScreenBrightnessChanged(change);
}

void PowerManagerMojoClient::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (auto& observer : observers_)
    observer.KeyboardBrightnessChanged(change);
}

void PowerManagerMojoClient::SuspendDone(base::TimeDelta sleep_duration) {
  for (auto& observer : observers_)
    observer.SuspendDone(sleep_duration);
}

void PowerManagerMojoClient::InitAfterInterfaceBound() {
  DCHECK(controller_.is_bound());
  power::mojom::PowerManagerObserverAssociatedPtrInfo ptr_info;
  binding_.Bind(mojo::MakeRequest(&ptr_info));
  controller_->SetObserver(std::move(ptr_info));
}

// static
PowerManagerClient* PowerManagerMojoClient::Get() {
  DCHECK_EQ(base::FeatureList::IsEnabled(chromeos::features::kMojoDBusRelay),
            !!g_instance);
  return g_instance ? g_instance : ::chromeos::PowerManagerClient::Get();
}

}  // namespace chromeos
