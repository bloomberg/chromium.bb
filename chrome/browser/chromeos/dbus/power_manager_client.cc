// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/power_manager_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

PowerSupplyStatus::PowerSupplyStatus()
    : line_power_on(false),
      battery_is_present(false),
      battery_is_full(false),
      battery_seconds_to_empty(0),
      battery_seconds_to_full(0),
      battery_percentage(0) {
}

std::string PowerSupplyStatus::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "line_power_on = %s ",
                      line_power_on ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_is_present = %s ",
                      battery_is_present ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_is_full = %s ",
                      battery_is_full ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_percentage = %f ",
                      battery_percentage);
  base::StringAppendF(&result,
                      "battery_seconds_to_empty = %"PRId64" ",
                      battery_seconds_to_empty);
  base::StringAppendF(&result,
                      "battery_seconds_to_full = %"PRId64" ",
                      battery_seconds_to_full);
  return result;
}

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

    // Monitor the D-Bus signal for power state changed signals.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPowerStateChangedSignal,
        base::Bind(&PowerManagerClientImpl::PowerStateChangedSignalReceived,
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

  // PowerManagerClient override.
  virtual void RequestStatusUpdate() OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetAllPropertiesMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetAllPropertiesMethod,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Requests restart of the system.
  virtual void RequestRestart() OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestRestartMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  };

  // Requests shutdown of the system.
  virtual void RequestShutdown() OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestShutdownMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void CalculateIdleTime(const CalculateIdleTimeCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetIdleTime);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetIdleTime,
                   weak_ptr_factory_.GetWeakPtr(), callback));
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

  // Called when a power state changed signal is received.
  void PowerStateChangedSignalReceived(dbus::Signal* signal) {
    VLOG(1) << "Received power state changed signal.";
    dbus::MessageReader reader(signal);
    std::string power_state_string;
    if (!reader.PopString(&power_state_string)) {
      LOG(ERROR) << "Error reading signal args: " << signal->ToString();
      return;
    }
    if (power_state_string != "on")
      return;
    // Notify all observers of resume event.
    FOR_EACH_OBSERVER(Observer, observers_, SystemResumed());
  }

  // Called when a power supply polling signal is received.
  void PowerSupplyPollReceived(dbus::Signal* unused_signal) {
    VLOG(1) << "Received power supply poll signal.";
    RequestStatusUpdate();
  }

  // Called when GetAllPropertiesMethod call is complete.
  void OnGetAllPropertiesMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kGetAllPropertiesMethod;
      return;
    }
    dbus::MessageReader reader(response);
    PowerSupplyStatus status;
    double unused_battery_voltage = 0.0;
    double unused_battery_energy = 0.0;
    double unused_battery_energy_rate = 0.0;
    if (!reader.PopBool(&status.line_power_on) ||
        !reader.PopDouble(&unused_battery_energy) ||
        !reader.PopDouble(&unused_battery_energy_rate) ||
        !reader.PopDouble(&unused_battery_voltage) ||
        !reader.PopInt64(&status.battery_seconds_to_empty) ||
        !reader.PopInt64(&status.battery_seconds_to_full) ||
        !reader.PopDouble(&status.battery_percentage) ||
        !reader.PopBool(&status.battery_is_present) ||
        !reader.PopBool(&status.battery_is_full)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      return;
    }

    VLOG(1) << "Power status: " << status.ToString();
    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status));
  }

  void OnGetIdleTime(const CalculateIdleTimeCallback& callback,
                     dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kGetIdleTime;
      return;
    }
    dbus::MessageReader reader(response);
    int64 idle_time_ms = 0;
    if (!reader.PopInt64(&idle_time_ms)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      callback.Run(-1);
      return;
    }
    if (idle_time_ms < 0) {
      LOG(ERROR) << "Power manager failed to calculate idle time.";
      callback.Run(-1);
      return;
    }
    callback.Run(idle_time_ms/1000);
  }

  dbus::ObjectProxy* power_manager_proxy_;
  ObserverList<Observer> observers_;
  base::WeakPtrFactory<PowerManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientImpl);
};

// The PowerManagerClient implementation used on Linux desktop,
// which does nothing.
class PowerManagerClientStubImpl : public PowerManagerClient {
 public:
  PowerManagerClientStubImpl()
      : discharging_(true),
        battery_percentage_(80),
        pause_count_(0) {
  }

  virtual ~PowerManagerClientStubImpl() {}

  // PowerManagerClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // PowerManagerClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // PowerManagerClient override.
  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    VLOG(1) << "Requested to descrease screen brightness";
  }

  // PowerManagerClient override.
  virtual void IncreaseScreenBrightness() OVERRIDE {
    VLOG(1) << "Requested to increase screen brightness";
  }

  virtual void RequestStatusUpdate() OVERRIDE {
    if (!timer_.IsRunning()) {
      timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(1000),
          this,
          &PowerManagerClientStubImpl::Update);
    } else {
      timer_.Stop();
    }
  }

  virtual void RequestRestart() OVERRIDE {}

  virtual void RequestShutdown() OVERRIDE {}

  virtual void CalculateIdleTime(const CalculateIdleTimeCallback& callback)
      OVERRIDE {
    callback.Run(0);
  }

 private:
  void Update() {
    // We pause at 0 and 100% so that it's easier to check those conditions.
    if (pause_count_ > 1) {
      pause_count_--;
      return;
    }

    if (battery_percentage_ == 0 || battery_percentage_ == 100) {
      if (pause_count_) {
        pause_count_ = 0;
        discharging_ = !discharging_;
      } else {
        // Pause twice (i.e. skip updating the menu), including the current
        // call to this function.
        pause_count_ = 2;
        return;
      }
    }
    battery_percentage_ += (discharging_ ? -1 : 1);

    const int kSecondsToEmptyFullBattery(3 * 60 * 60);  // 3 hours.

    PowerSupplyStatus status;
    status.line_power_on = !discharging_;
    status.battery_is_present = true;
    status.battery_percentage = battery_percentage_;
    status.battery_seconds_to_empty =
        std::max(1, battery_percentage_ * kSecondsToEmptyFullBattery / 100);
    status.battery_seconds_to_full =
        std::max(static_cast<int64>(1),
                 kSecondsToEmptyFullBattery - status.battery_seconds_to_empty);

    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status));
  }

  bool discharging_;
  int battery_percentage_;
  int pause_count_;
  ObserverList<Observer> observers_;
  base::RepeatingTimer<PowerManagerClientStubImpl> timer_;
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
