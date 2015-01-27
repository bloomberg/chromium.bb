// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/privet_daemon_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {
namespace {

// The expected response to the Ping D-Bus method.
const char kPingResponse[] = "Hello world!";

// The PrivetClient implementation used in production.
class PrivetClientImpl : public PrivetDaemonClient {
 public:
  PrivetClientImpl();
  ~PrivetClientImpl() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrivetClient overrides:
  void Ping(const PingCallback& callback) override;

 private:
  // Callback for the Ping DBus method.
  void OnPing(const PingCallback& callback, dbus::Response* response);

  // Called when the object is connected to the signal.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  dbus::ObjectProxy* privetd_proxy_;
  base::WeakPtrFactory<PrivetClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetClientImpl);
};

PrivetClientImpl::PrivetClientImpl()
    : privetd_proxy_(nullptr), weak_ptr_factory_(this) {
}

PrivetClientImpl::~PrivetClientImpl() {
}

void PrivetClientImpl::Ping(const PingCallback& callback) {
  dbus::MethodCall method_call(privetd::kPrivetdManagerInterface,
                               privetd::kPingMethod);
  privetd_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PrivetClientImpl::OnPing, weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PrivetClientImpl::Init(dbus::Bus* bus) {
  privetd_proxy_ = bus->GetObjectProxy(
      privetd::kPrivetdServiceName,
      dbus::ObjectPath(privetd::kPrivetdManagerServicePath));
}

void PrivetClientImpl::OnPing(const PingCallback& callback,
                              dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling " << privetd::kPingMethod;
    callback.Run(false);
    return;
  }
  dbus::MessageReader reader(response);
  std::string value;
  if (!reader.PopString(&value)) {
    LOG(ERROR) << "Error reading response from privetd: "
               << response->ToString();
  }
  // Returns success if the expected value is received.
  callback.Run(value == kPingResponse);
}

void PrivetClientImpl::OnSignalConnected(const std::string& interface_name,
                                         const std::string& signal_name,
                                         bool success) {
  LOG_IF(ERROR, !success) << "Failed to connect to " << signal_name;
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////

PrivetDaemonClient::PrivetDaemonClient() {
}

PrivetDaemonClient::~PrivetDaemonClient() {
}

// static
PrivetDaemonClient* PrivetDaemonClient::Create() {
  return new PrivetClientImpl();
}

}  // namespace chromeos
