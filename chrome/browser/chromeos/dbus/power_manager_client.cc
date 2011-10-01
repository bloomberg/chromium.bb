// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/power_manager_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  explicit PowerManagerClientImpl(dbus::Bus* bus)
      : power_manager_proxy_(NULL) {
    power_manager_proxy_ = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        power_manager::kPowerManagerServicePath);

    // Monitor the D-Bus signal for brightness changes. Only the power
    // manager knows the actual brightness level. We don't cache the
    // brightness level in Chrome as it'll make things less reliable.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kBrightnessChangedSignal,
        base::Bind(&PowerManagerClientImpl::BrightnessChangedReceived, this),
        base::Bind(&PowerManagerClientImpl::BrightnessChangedConnected, this));
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
                   this));
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
                   this));
  }

 private:
  friend class base::RefCountedThreadSafe<PowerManagerClientImpl>;
  virtual ~PowerManagerClientImpl() {
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

  // Called when the brightness change signal is initially connected.
  void BrightnessChangedConnected(const std::string& interface_name,
                                  const std::string& signal_name,
                                  bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to brightness changed signal.";
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

  dbus::ObjectProxy* power_manager_proxy_;
  ObserverList<Observer> observers_;

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
