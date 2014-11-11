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
#include "dbus/object_proxy.h"

namespace chromeos {
namespace {

// The PrivetClient implementation used in production.
class PrivetClientImpl : public PrivetDaemonClient {
 public:
  PrivetClientImpl();
  ~PrivetClientImpl() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrivetClient overrides:
  void GetSetupStatus(const GetSetupStatusCallback& callback) override;

 private:
  // Called when the setup status changes.
  void OnSetupStatusChanged(dbus::Signal* signal);

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

void PrivetClientImpl::GetSetupStatus(const GetSetupStatusCallback& callback) {
  // TODO(jamescook): Implement this.
  callback.Run(privetd::kSetupCompleted);
}

void PrivetClientImpl::Init(dbus::Bus* bus) {
  privetd_proxy_ =
      bus->GetObjectProxy(privetd::kPrivetdServiceName,
                          dbus::ObjectPath(privetd::kPrivetdServicePath));

  privetd_proxy_->ConnectToSignal(
      privetd::kPrivetdInterface, privetd::kSetupStatusChangedSignal,
      base::Bind(&PrivetClientImpl::OnSetupStatusChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PrivetClientImpl::OnSignalConnected,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PrivetClientImpl::OnSetupStatusChanged(dbus::Signal* signal) {
  // TODO(jamescook): Implement this.
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
