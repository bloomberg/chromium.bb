// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/root_power_manager_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chromeos/dbus/power_manager/input_event.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/root_power_manager_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The RootPowerManagerClient implementation used in production.
class RootPowerManagerClientImpl : public RootPowerManagerClient {
 public:
  explicit RootPowerManagerClientImpl(dbus::Bus* bus)
      : proxy_(NULL),
        weak_ptr_factory_(this) {
    proxy_ = bus->GetObjectProxy(
        power_manager::kRootPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kRootPowerManagerServicePath));

    proxy_->ConnectToSignal(
        power_manager::kRootPowerManagerInterface,
        power_manager::kInputEventSignal,
        base::Bind(&RootPowerManagerClientImpl::InputEventReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&RootPowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        power_manager::kRootPowerManagerInterface,
        power_manager::kSuspendStateChangedSignal,
        base::Bind(&RootPowerManagerClientImpl::SuspendStateChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&RootPowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~RootPowerManagerClientImpl() {
  }

  // RootPowerManagerClient overrides:
  virtual void AddObserver(RootPowerManagerObserver* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(RootPowerManagerObserver* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(RootPowerManagerObserver* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

 private:
  // Called when a D-Bus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    if (!success)
      LOG(WARNING) << "Failed to connect to signal " << signal_name;
  }

  void InputEventReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::InputEvent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kInputEventSignal << " signal";
      return;
    }

    base::TimeTicks timestamp =
        base::TimeTicks::FromInternalValue(proto.timestamp());
    VLOG(1) << "Got " << power_manager::kInputEventSignal << " signal:"
            << " type=" << proto.type() << " timestamp=" << proto.timestamp();
    switch (proto.type()) {
      case power_manager::InputEvent_Type_POWER_BUTTON_DOWN:
      case power_manager::InputEvent_Type_POWER_BUTTON_UP: {
        bool down =
            (proto.type() == power_manager::InputEvent_Type_POWER_BUTTON_DOWN);
        FOR_EACH_OBSERVER(RootPowerManagerObserver, observers_,
                          OnPowerButtonEvent(down, timestamp));
        break;
      }
      case power_manager::InputEvent_Type_LID_OPEN:
      case power_manager::InputEvent_Type_LID_CLOSED: {
        bool open =
            (proto.type() == power_manager::InputEvent_Type_LID_OPEN);
        FOR_EACH_OBSERVER(RootPowerManagerObserver, observers_,
                          OnLidEvent(open, timestamp));
        break;
      }
    }
  }

  void SuspendStateChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::SuspendState proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kSuspendStateChangedSignal << " signal";
      return;
    }

    VLOG(1) << "Got " << power_manager::kSuspendStateChangedSignal << " signal:"
            << " type=" << proto.type() << " wall_time=" << proto.wall_time();
    base::Time wall_time =
        base::Time::FromInternalValue(proto.wall_time());
    switch (proto.type()) {
      case power_manager::SuspendState_Type_SUSPEND_TO_MEMORY:
        last_suspend_wall_time_ = wall_time;
        break;
      case power_manager::SuspendState_Type_RESUME:
        FOR_EACH_OBSERVER(
            RootPowerManagerObserver, observers_,
            OnResume(wall_time - last_suspend_wall_time_));
        break;
    }
  }

  dbus::ObjectProxy* proxy_;

  ObserverList<RootPowerManagerObserver> observers_;

  // Wall time from the latest signal telling us that the system was about to
  // suspend to memory.
  base::Time last_suspend_wall_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RootPowerManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RootPowerManagerClientImpl);
};

// A no-op RootPowerManagerClient implementation used on Linux desktops.
class RootPowerManagerClientStubImpl : public RootPowerManagerClient {
 public:
  RootPowerManagerClientStubImpl() {}
  virtual ~RootPowerManagerClientStubImpl() {}

  // RootPowerManagerClient overrides:
  virtual void AddObserver(RootPowerManagerObserver* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(RootPowerManagerObserver* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(RootPowerManagerObserver* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

 private:
  ObserverList<RootPowerManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RootPowerManagerClientStubImpl);
};

RootPowerManagerClient::RootPowerManagerClient() {
}

RootPowerManagerClient::~RootPowerManagerClient() {
}

RootPowerManagerClient* RootPowerManagerClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new RootPowerManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new RootPowerManagerClientStubImpl();
}

}  // namespace chromeos
