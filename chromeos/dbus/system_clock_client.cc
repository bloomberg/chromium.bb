// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/system_clock_client.h"

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The SystemClockClient implementation used in production.
class SystemClockClientImpl : public SystemClockClient {
 public:
  explicit SystemClockClientImpl(dbus::Bus* bus)
      : system_clock_proxy_(NULL),
        weak_ptr_factory_(this) {
    system_clock_proxy_ = bus->GetObjectProxy(
        system_clock::kSystemClockServiceName,
        dbus::ObjectPath(system_clock::kSystemClockServicePath));

    // Monitor the D-Bus signal for TimeUpdated changes.
    system_clock_proxy_->ConnectToSignal(
        system_clock::kSystemClockInterface,
        system_clock::kSystemClockUpdated,
        base::Bind(&SystemClockClientImpl::TimeUpdatedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SystemClockClientImpl::TimeUpdatedConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~SystemClockClientImpl() {
  }

  // SystemClockClient overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

 private:
  // Called when a TimeUpdated signal is received.
  void TimeUpdatedReceived(dbus::Signal* signal) {
    VLOG(1) << "TimeUpdated signal received: " << signal->ToString();
    dbus::MessageReader reader(signal);
    FOR_EACH_OBSERVER(Observer, observers_, SystemClockUpdated());
  }

  // Called when the TimeUpdated signal is initially connected.
  void TimeUpdatedConnected(const std::string& interface_name,
                            const std::string& signal_name,
                            bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to TimeUpdated signal.";
  }

  dbus::ObjectProxy* system_clock_proxy_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SystemClockClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemClockClientImpl);
};

// The SystemClockClient implementation used on Linux desktop,
// which does nothing.
class SystemClockClientStubImpl : public SystemClockClient {
 public:
  SystemClockClientStubImpl() {}
  ~SystemClockClientStubImpl() {}

  // SystemClockClient overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool HasObserver(Observer* observer) OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemClockClientStubImpl);
};

SystemClockClient::SystemClockClient() {
}

SystemClockClient::~SystemClockClient() {
}

// static
SystemClockClient* SystemClockClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new SystemClockClientImpl(bus);
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new SystemClockClientStubImpl();
}

}  // namespace chromeos
