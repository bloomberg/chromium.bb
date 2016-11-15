// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/upstart_client.h"

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

const char kUpstartAuthPolicyPath[] = "/com/ubuntu/Upstart/jobs/authpolicyd";

class UpstartClientImpl : public UpstartClient {
 public:
  UpstartClientImpl() : weak_ptr_factory_(this) {}

  ~UpstartClientImpl() override {}

  // UpstartClient override.
  void StartAuthPolicyService() override {
    dbus::ObjectPath objectPath(kUpstartAuthPolicyPath);
    dbus::ObjectProxy* proxy =
        bus_->GetObjectProxy(kUpstartServiceName, objectPath);
    dbus::MethodCall method_call(kUpstartJobInterface, kUpstartStartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(std::vector<std::string>());
    writer.AppendBool(true);  // Wait for response.
    proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::Bind(&UpstartClientImpl::HandleStartResponse,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  void Init(dbus::Bus* bus) override { bus_ = bus; }

 private:
  void HandleStartResponse(dbus::Response* response) {
    LOG_IF(ERROR, !response) << "Failed to signal Upstart, response is null";
  }

  dbus::Bus* bus_ = nullptr;

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
