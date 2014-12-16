// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/peer_daemon_manager_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"

namespace chromeos {
namespace {

// TODO(benchan): Move these constants to system_api.
namespace peerd {
const char kPeerdServiceName[] = "org.chromium.peerd";
const char kPeerdServicePath[] = "/org/chromium/peerd";
const char kManagerInterface[] = "org.chromium.peerd.Manager";
const char kStartMonitoringMethod[] = "StartMonitoring";
const char kStopMonitoringMethod[] = "StopMonitoring";
const char kExposeServiceMethod[] = "ExposeService";
const char kRemoveExposedServiceMethod[] = "RemoveExposedService";
const char kPingMethod[] = "Ping";
}  // namespace peerd

// The PeerDaemonManagerClient implementation used in production.
class PeerDaemonManagerClientImpl : public PeerDaemonManagerClient {
 public:
  PeerDaemonManagerClientImpl();
  ~PeerDaemonManagerClientImpl() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PeerDaemonManagerClient overrides:
  void StartMonitoring(
      const std::vector<std::string>& requested_technologies,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) override;
  void StopMonitoring(const std::string& monitoring_token,
                      const VoidDBusMethodCallback& callback) override;
  void ExposeService(
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) override;
  void RemoveExposedService(const std::string& service_token,
                            const VoidDBusMethodCallback& callback) override;
  void Ping(const StringDBusMethodCallback& callback) override;

 private:
  void OnStringDBusMethod(const StringDBusMethodCallback& callback,
                          dbus::Response* response);
  void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                        dbus::Response* response);

  dbus::ObjectProxy* peer_daemon_proxy_;
  base::WeakPtrFactory<PeerDaemonManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PeerDaemonManagerClientImpl);
};

PeerDaemonManagerClientImpl::PeerDaemonManagerClientImpl()
    : peer_daemon_proxy_(nullptr), weak_ptr_factory_(this) {
}

PeerDaemonManagerClientImpl::~PeerDaemonManagerClientImpl() {
}

void PeerDaemonManagerClientImpl::StartMonitoring(
    const std::vector<std::string>& requested_technologies,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kStartMonitoringMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfStrings(requested_technologies);
  dbus::AppendValueData(&writer, options);
  peer_daemon_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PeerDaemonManagerClientImpl::StopMonitoring(
    const std::string& monitoring_token,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kStopMonitoringMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(monitoring_token);
  peer_daemon_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PeerDaemonManagerClientImpl::ExposeService(
    const std::string& service_id,
    const std::map<std::string, std::string>& service_info,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kExposeServiceMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(service_id);

  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{ss}", &array_writer);
  for (const auto& entry : service_info) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(entry.first);
    dict_entry_writer.AppendString(entry.second);
    array_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&array_writer);

  dbus::AppendValueData(&writer, options);
  peer_daemon_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PeerDaemonManagerClientImpl::RemoveExposedService(
    const std::string& service_token,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kRemoveExposedServiceMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(service_token);
  peer_daemon_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PeerDaemonManagerClientImpl::Ping(
    const StringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kPingMethod);
  dbus::MessageWriter writer(&method_call);
  peer_daemon_proxy_->CallMethod(
      &method_call,
      dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void PeerDaemonManagerClientImpl::OnStringDBusMethod(
    const StringDBusMethodCallback& callback,
    dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
    return;
  }

  dbus::MessageReader reader(response);
  std::string result;
  if (!reader.PopString(&result)) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
    return;
  }

  callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
}

void PeerDaemonManagerClientImpl::OnVoidDBusMethod(
    const VoidDBusMethodCallback& callback,
    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

void PeerDaemonManagerClientImpl::Init(dbus::Bus* bus) {
  peer_daemon_proxy_ =
      bus->GetObjectProxy(peerd::kPeerdServiceName,
                          dbus::ObjectPath(peerd::kPeerdServicePath));
}

}  // namespace


PeerDaemonManagerClient::PeerDaemonManagerClient() {
}

PeerDaemonManagerClient::~PeerDaemonManagerClient() {
}

// static
PeerDaemonManagerClient* PeerDaemonManagerClient::Create() {
  return new PeerDaemonManagerClientImpl();
}

}  // namespace chromeos
