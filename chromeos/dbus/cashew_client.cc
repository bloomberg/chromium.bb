// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cashew_client.h"

#include "base/bind.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Does nothing.
// This method is used to handle results of RequestDataPlansUpdate method call.
void DoNothing(dbus::Response* response) {
}

// The CashewClient implementation.
class CashewClientImpl : public CashewClient {
 public:
  explicit CashewClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
            cashew::kCashewServiceName,
            dbus::ObjectPath(cashew::kCashewServicePath))),
        weak_ptr_factory_(this) {
    proxy_->ConnectToSignal(
        cashew::kCashewServiceInterface,
        cashew::kMonitorDataPlanUpdate,
        base::Bind(&CashewClientImpl::OnDataPlansUpdate,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CashewClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // CashewClient override.
  virtual void SetDataPlansUpdateHandler(
      const DataPlansUpdateHandler& handler) OVERRIDE {
    data_plans_update_handler_ = handler;
  }

  // CashewClient override.
  virtual void ResetDataPlansUpdateHandler() OVERRIDE {
    data_plans_update_handler_.Reset();
  }

  // CashewClient override.
  virtual void RequestDataPlansUpdate(const std::string& service) OVERRIDE {
    dbus::MethodCall method_call(cashew::kCashewServiceInterface,
                                 cashew::kRequestDataPlanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(service);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&DoNothing));
  }

 private:
  // Handles DataPlansUpdate signal.
  void OnDataPlansUpdate(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string service;
    if (!reader.PopString(&service)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    scoped_ptr<Value> value(dbus::PopDataAsValue(&reader));
    ListValue* data_plans = NULL;
    if (!value.get() || !value->GetAsList(&data_plans)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!data_plans_update_handler_.is_null())
      data_plans_update_handler_.Run(service, *data_plans);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool successed) {
    LOG_IF(ERROR, !successed) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;
  DataPlansUpdateHandler data_plans_update_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CashewClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CashewClientImpl);
};

// A stub implementaion of CashewClient.
class CashewClientStubImpl : public CashewClient {
 public:
  CashewClientStubImpl() {}

  virtual ~CashewClientStubImpl() {}

  // CashewClient override.
  virtual void SetDataPlansUpdateHandler(
      const DataPlansUpdateHandler& handler) OVERRIDE {}

  // CashewClient override.
  virtual void ResetDataPlansUpdateHandler() OVERRIDE {}

  // CashewClient override.
  virtual void RequestDataPlansUpdate(const std::string& service) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CashewClientStubImpl);
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// CashewClient

CashewClient::CashewClient() {}

CashewClient::~CashewClient() {}

// static
CashewClient* CashewClient::Create(DBusClientImplementationType type,
                                   dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new CashewClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new CashewClientStubImpl();
}

}  // namespace chromeos
