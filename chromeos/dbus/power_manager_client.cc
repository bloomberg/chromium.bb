// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_manager_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/timer.h"
#include "chromeos/dbus/power_state_control.pb.h"
#include "chromeos/dbus/power_supply_properties.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  explicit PowerManagerClientImpl(dbus::Bus* bus)
      : power_manager_proxy_(NULL),
        weak_ptr_factory_(this) {
    power_manager_proxy_ = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));

    session_manager_proxy_ = bus->GetObjectProxy(
        login_manager::kSessionManagerServiceName,
        dbus::ObjectPath(login_manager::kSessionManagerServicePath));

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

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPowerSupplyPollSignal,
        base::Bind(&PowerManagerClientImpl::PowerSupplyPollReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPowerStateChangedSignal,
        base::Bind(&PowerManagerClientImpl::PowerStateChangedSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kButtonEventSignal,
        base::Bind(&PowerManagerClientImpl::ButtonEventSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    session_manager_proxy_->ConnectToSignal(
        chromium::kChromiumInterface,
        chromium::kLockScreenSignal,
        base::Bind(&PowerManagerClientImpl::ScreenLockSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    session_manager_proxy_->ConnectToSignal(
        chromium::kChromiumInterface,
        chromium::kUnlockScreenSignal,
        base::Bind(&PowerManagerClientImpl::ScreenUnlockSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        chromium::kChromiumInterface,
        chromium::kUnlockScreenFailedSignal,
        base::Bind(&PowerManagerClientImpl::ScreenUnlockFailedSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kIdleNotifySignal,
        base::Bind(&PowerManagerClientImpl::IdleNotifySignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kActiveNotifySignal,
        base::Bind(&PowerManagerClientImpl::ActiveNotifySignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSoftwareScreenDimmingRequestedSignal,
        base::Bind(
            &PowerManagerClientImpl::SoftwareScreenDimmingRequestedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~PowerManagerClientImpl() {
  }

  // PowerManagerClient overrides:

  virtual void AddObserver(Observer* observer) OVERRIDE {
    CHECK(observer);  // http://crbug.com/119976
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kDecreaseScreenBrightness);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allow_off);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kIncreaseScreenBrightness);
  }

  virtual void SetScreenBrightnessPercent(double percent, bool gradual) {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kSetScreenBrightnessPercent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendDouble(percent);
    writer.AppendInt32(
        gradual ?
        power_manager::kBrightnessTransitionGradual :
        power_manager::kBrightnessTransitionInstant);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetScreenBrightnessPercent);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetScreenBrightnessPercent,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void RequestStatusUpdate(UpdateRequestType update_type) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetPowerSupplyPropertiesMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetPowerSupplyPropertiesMethod,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void RequestRestart() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestRestartMethod);
  };

  virtual void RequestShutdown() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestShutdownMethod);
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

  virtual void RequestIdleNotification(int64 threshold) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestIdleNotification);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(threshold);

    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void RequestActiveNotification() OVERRIDE {
    RequestIdleNotification(0);
  }

  virtual void RequestPowerStateOverrides(
      uint32 request_id,
      uint32 duration,
      int overrides,
      const PowerStateRequestIdCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kStateOverrideRequest);
    dbus::MessageWriter writer(&method_call);

    PowerStateControl protobuf;
    protobuf.set_request_id(request_id);
    protobuf.set_duration(duration);
    protobuf.set_disable_idle_dim(overrides & DISABLE_IDLE_DIM);
    protobuf.set_disable_idle_blank(overrides & DISABLE_IDLE_BLANK);
    protobuf.set_disable_idle_suspend(overrides & DISABLE_IDLE_SUSPEND);
    protobuf.set_disable_lid_suspend(overrides & DISABLE_IDLE_LID_SUSPEND);

    writer.AppendProtoAsArrayOfBytes(protobuf);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnPowerStateOverride,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void NotifyScreenLockRequested() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestLockScreenMethod);
  }

  virtual void NotifyScreenLockCompleted() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kScreenIsLockedMethod);
  }

  virtual void NotifyScreenUnlockRequested() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestUnlockScreenMethod);
  }

  virtual void NotifyScreenUnlockCompleted() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kScreenIsUnlockedMethod);
  }

 private:
  // Called when a dbus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to signal "
                              << signal_name << ".";
  }

  // Make a method call to power manager with no arguments and no response.
  void SimpleMethodCallToPowerManager(const std::string& method_name) {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

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
    FOR_EACH_OBSERVER(Observer, observers_, SystemResumed());
  }

  void ButtonEventSignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string button_name;
    bool down = false;
    int64 timestamp_internal = 0;
    if (!reader.PopString(&button_name) ||
        !reader.PopBool(&down) ||
        !reader.PopInt64(&timestamp_internal)) {
      LOG(ERROR) << "Button signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    base::TimeTicks timestamp =
        base::TimeTicks::FromInternalValue(timestamp_internal);

    if (button_name == power_manager::kPowerButtonName) {
      FOR_EACH_OBSERVER(
          Observer, observers_, PowerButtonStateChanged(down, timestamp));
    } else if (button_name == power_manager::kLockButtonName) {
      FOR_EACH_OBSERVER(
          Observer, observers_, LockButtonStateChanged(down, timestamp));
    }
  }

  void PowerSupplyPollReceived(dbus::Signal* unused_signal) {
    VLOG(1) << "Received power supply poll signal.";
    RequestStatusUpdate(UPDATE_POLL);
  }

  void OnGetPowerSupplyPropertiesMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetPowerSupplyPropertiesMethod;
      return;
    }

    dbus::MessageReader reader(response);
    PowerSupplyProperties protobuf;
    reader.PopArrayOfBytesAsProto(&protobuf);

    PowerSupplyStatus status;
    status.line_power_on = protobuf.line_power_on();
    status.battery_seconds_to_empty = protobuf.battery_time_to_empty();
    status.battery_seconds_to_full = protobuf.battery_time_to_full();
    status.averaged_battery_time_to_empty =
        protobuf.averaged_battery_time_to_empty();
    status.averaged_battery_time_to_full =
        protobuf.averaged_battery_time_to_full();
    status.battery_percentage = protobuf.battery_percentage();
    status.battery_is_present = protobuf.battery_is_present();
    status.battery_is_full = protobuf.battery_is_charged();
    status.is_calculating_battery_time = protobuf.is_calculating_battery_time();

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

  void OnPowerStateOverride(const PowerStateRequestIdCallback& callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kStateOverrideRequest;
      return;
    }

    dbus::MessageReader reader(response);
    uint32 request_id = 0;
    if (!reader.PopUint32(&request_id)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      callback.Run(0);
      return;
    }

    callback.Run(request_id);
  }

  void OnGetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetScreenBrightnessPercent;
      return;
    }
    dbus::MessageReader reader(response);
    double percent = 0.0;
    if (!reader.PopDouble(&percent))
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
    callback.Run(percent);
  }

  void ScreenLockSignalReceived(dbus::Signal* signal) {
    // TODO(flackr): This warning is actually a signal that things are working
    // as expected. As per http://crbug.com/126217, this will help determine
    // if the problem is with dbus or in chrome.
    LOG(WARNING) << "LockScreen signal received from power manager.";
    FOR_EACH_OBSERVER(Observer, observers_, LockScreen());
  }

  void ScreenUnlockSignalReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, UnlockScreen());
  }

  void ScreenUnlockFailedSignalReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, UnlockScreenFailed());
  }

  void IdleNotifySignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int64 threshold = 0;
    if (!reader.PopInt64(&threshold)) {
      LOG(ERROR) << "Idle Notify signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    DCHECK_GT(threshold, 0);

    VLOG(1) << "Idle Notify: " << threshold;
    FOR_EACH_OBSERVER(Observer, observers_, IdleNotify(threshold));
  }

  void ActiveNotifySignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int64 threshold = 0;
    if (!reader.PopInt64(&threshold)) {
      LOG(ERROR) << "Active Notify signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    DCHECK_EQ(threshold, 0);

    VLOG(1) << "Active Notify.";
    FOR_EACH_OBSERVER(Observer, observers_, ActiveNotify());
  }

  void SoftwareScreenDimmingRequestedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 signal_state = 0;
    if (!reader.PopInt32(&signal_state)) {
      LOG(ERROR) << "Screen dimming signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }

    Observer::ScreenDimmingState state = Observer::SCREEN_DIMMING_NONE;
    switch (signal_state) {
      case power_manager::kSoftwareScreenDimmingNone:
        state = Observer::SCREEN_DIMMING_NONE;
        break;
      case power_manager::kSoftwareScreenDimmingIdle:
        state = Observer::SCREEN_DIMMING_IDLE;
        break;
      default:
        LOG(ERROR) << "Unhandled screen dimming state " << signal_state;
    }
    FOR_EACH_OBSERVER(Observer, observers_, ScreenDimmingRequested(state));
  }

  dbus::ObjectProxy* power_manager_proxy_;
  dbus::ObjectProxy* session_manager_proxy_;
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
        battery_percentage_(81),
        pause_count_(0) {
  }

  virtual ~PowerManagerClientStubImpl() {}

  // PowerManagerClient overrides:

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    VLOG(1) << "Requested to descrease screen brightness";
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    VLOG(1) << "Requested to increase screen brightness";
  }

  virtual void SetScreenBrightnessPercent(double percent,
                                          bool gradual) OVERRIDE {
    VLOG(1) << "Requested to set screen brightness to " << percent << "% "
            << (gradual ? "gradually" : "instantaneously");
  }

  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) OVERRIDE {
    callback.Run(100.0);
  }

  virtual void RequestStatusUpdate(UpdateRequestType update_type) OVERRIDE {
    if (update_type == UPDATE_INITIAL) {
      Update();
      return;
    }
    if (!timer_.IsRunning() && update_type == UPDATE_USER) {
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

  virtual void RequestIdleNotification(int64 threshold) OVERRIDE {}
  virtual void RequestActiveNotification() OVERRIDE {}
  virtual void RequestPowerStateOverrides(
      uint32 request_id,
      uint32 duration,
      int overrides,
      const PowerStateRequestIdCallback& callback) OVERRIDE {}

  virtual void NotifyScreenLockRequested() OVERRIDE {
    FOR_EACH_OBSERVER(Observer, observers_, LockScreen());
  }
  virtual void NotifyScreenLockCompleted() OVERRIDE {}
  virtual void NotifyScreenUnlockRequested() OVERRIDE {
    FOR_EACH_OBSERVER(Observer, observers_, UnlockScreen());
  }

  virtual void NotifyScreenUnlockCompleted() OVERRIDE {}

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
    battery_percentage_ += (discharging_ ? -5 : 5);
    battery_percentage_ = std::min(std::max(battery_percentage_, 0), 100);

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

PowerManagerClient* PowerManagerClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new PowerManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new PowerManagerClientStubImpl();
}

}  // namespace chromeos
