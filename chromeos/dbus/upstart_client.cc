// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/upstart_client.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

const char kUpstartServiceName[] = "com.ubuntu.Upstart";
const char kUpstartJobInterface[] = "com.ubuntu.Upstart0_6.Job";
const char kUpstartStartMethod[] = "Start";
const char kUpstartRestartMethod[] = "Restart";
const char kUpstartStopMethod[] = "Stop";

const char kUpstartAuthPolicyPath[] = "/com/ubuntu/Upstart/jobs/authpolicyd";
const char kUpstartMediaAnalyticsPath[] =
    "/com/ubuntu/Upstart/jobs/rtanalytics";

class UpstartClientImpl : public UpstartClient {
 public:
  UpstartClientImpl() : weak_ptr_factory_(this) {}

  ~UpstartClientImpl() override {}

  // UpstartClient override.
  void StartAuthPolicyService() override {
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartStartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    auth_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::HandleAuthResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void RestartAuthPolicyService() override {
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartRestartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    auth_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::HandleAuthResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void StartMediaAnalytics(const UpstartCallback& callback) override {
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartStartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    ma_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::HandleStartMediaAnalyticsResponse,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RestartMediaAnalytics(const UpstartCallback& callback) override {
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartRestartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    ma_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::HandleStartMediaAnalyticsResponse,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void StopMediaAnalytics() override {
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartStopMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    ma_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::HandleStopMediaAnalyticsResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    bus_ = bus;
    auth_proxy_ = bus_->GetObjectProxy(
        kUpstartServiceName, dbus::ObjectPath(kUpstartAuthPolicyPath));
    ma_proxy_ = bus_->GetObjectProxy(
        kUpstartServiceName, dbus::ObjectPath(kUpstartMediaAnalyticsPath));
  }

 private:
  void HandleAuthResponse(dbus::Response* response) {
    LOG_IF(ERROR, !response) << "Failed to signal Upstart, response is null";
  }

  void HandleStartMediaAnalyticsResponse(const UpstartCallback& callback,
                                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to signal Upstart, response is null.";
      callback.Run(false);
      return;
    }
    callback.Run(true);
  }

  void HandleStopMediaAnalyticsResponse(dbus::Response* response) {
    LOG_IF(ERROR, !response) << "Failed to signal Upstart, response is null";
  }

  dbus::Bus* bus_ = nullptr;
  dbus::ObjectProxy* auth_proxy_ = nullptr;
  dbus::ObjectProxy* ma_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpstartClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpstartClientImpl);
};

}  // namespace

UpstartClient::UpstartClient() {}

UpstartClient::~UpstartClient() {}

// static
UpstartClient* UpstartClient::Create() {
  return new UpstartClientImpl();
}

}  // namespace chromeos
