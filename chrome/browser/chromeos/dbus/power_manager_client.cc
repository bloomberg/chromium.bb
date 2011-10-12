// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/power_manager_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros/chromeos_power.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
// TODO(sque): Remove chromeos_power.h, crosbug.com/16558

namespace chromeos {

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  explicit PowerManagerClientImpl(dbus::Bus* bus)
      : power_manager_proxy_(NULL),
        weak_ptr_factory_(this) {
    power_manager_proxy_ = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        power_manager::kPowerManagerServicePath);

    // Monitor the D-Bus signal for brightness changes. Only the power
    // manager knows the actual brightness level. We don't cache the
    // brightness level in Chrome as it'll make things less reliable.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kBrightnessChangedSignal,
        base::Bind(&PowerManagerClientImpl::BrightnessChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for power supply polling signals.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPowerSupplyPollSignal,
        base::Bind(&PowerManagerClientImpl::PowerSupplyPollReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~PowerManagerClientImpl() {
  }

  // PowerManagerClient override.
  virtual void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  // PowerManagerClient override.
  virtual void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // PowerManagerClient override.
  virtual void DecreaseScreenBrightness(bool allow_off) {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kDecreaseScreenBrightness);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allow_off);

    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnDecreaseScreenBrightness,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // PowerManagerClient override.
  virtual void IncreaseScreenBrightness() {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kIncreaseScreenBrightness);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnIncreaseScreenBrightness,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Called when a dbus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to signal "
                              << signal_name << ".";
  }

  // Called when a brightness change signal is received.
  void BrightnessChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 brightness_level = 0;
    bool user_initiated = 0;
    if (!(reader.PopInt32(&brightness_level) &&
          reader.PopBool(&user_initiated))) {
      LOG(ERROR) << "Brightness changed signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    VLOG(1) << "Brightness changed to " << brightness_level
            << ": user initiated " << user_initiated;
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrightnessChanged(brightness_level, user_initiated));
  }

  // Called when a response for DecreaseScreenBrightness() is received.
  void OnDecreaseScreenBrightness(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to decrease screen brightness";
      return;
    }
    VLOG(1) << "screen brightness increased: " << response->ToString();
  }

  // Called when a response for IncreaseScreenBrightness() is received.
  void OnIncreaseScreenBrightness(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to increase screen brightness";
      return;
    }
    VLOG(1) << "screen brightness increased: " << response->ToString();
  }

  // Called when a power supply polling signal is received.
  void PowerSupplyPollReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    VLOG(1) << "Received power supply poll signal.";
    GetPowerSupplyInfo();
  }

  // Gets the state of the power supply (line power and battery) from power
  // manager.
  void GetPowerSupplyInfo() {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetAllPropertiesMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnPowerSupplyPoll,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnPowerSupplyPoll(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kGetAllPropertiesMethod
                 << ".";
      return;
    }
    dbus::MessageReader reader(response);
    chromeos::PowerStatus status;
    bool battery_is_charged;
    if (!reader.PopBool(&status.line_power_on) ||
        !reader.PopDouble(&status.battery_energy) ||
        !reader.PopDouble(&status.battery_energy_rate) ||
        !reader.PopDouble(&status.battery_voltage) ||
        !reader.PopInt64(&status.battery_time_to_empty) ||
        !reader.PopInt64(&status.battery_time_to_full) ||
        !reader.PopDouble(&status.battery_percentage) ||
        !reader.PopBool(&status.battery_is_present) ||
        !reader.PopBool(&battery_is_charged)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      return;
    }
    status.battery_state = battery_is_charged ? BATTERY_STATE_FULLY_CHARGED
                                               : BATTERY_STATE_UNKNOWN;

    FOR_EACH_OBSERVER(Observer, observers_, UpdatePowerStatus(status));
  }

  dbus::ObjectProxy* power_manager_proxy_;
  ObserverList<Observer> observers_;
  base::WeakPtrFactory<PowerManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientImpl);
};

// The PowerManagerClient implementation used on Linux desktop,
// which does nothing.
class PowerManagerClientStubImpl : public PowerManagerClient {
  // PowerManagerClient override.
  virtual void AddObserver(Observer* observer) {
  }

  // PowerManagerClient override.
  virtual void RemoveObserver(Observer* observer) {
  }

  // PowerManagerClient override.
  virtual void DecreaseScreenBrightness(bool allow_off) {
    VLOG(1) << "Requested to descrease screen brightness";
  }

  // PowerManagerClient override.
  virtual void IncreaseScreenBrightness() {
    VLOG(1) << "Requested to increase screen brightness";
  }
};

PowerManagerClient::PowerManagerClient() {
}

PowerManagerClient::~PowerManagerClient() {
}

PowerManagerClient* PowerManagerClient::Create(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new PowerManagerClientImpl(bus);
  } else {
    return new PowerManagerClientStubImpl();
  }
}

}  // namespace chromeos
