// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/upstart/upstart_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

constexpr char kUpstartServiceName[] = "com.ubuntu.Upstart";
constexpr char kUpstartJobInterface[] = "com.ubuntu.Upstart0_6.Job";
constexpr char kStartMethod[] = "Start";
constexpr char kRestartMethod[] = "Restart";
constexpr char kStopMethod[] = "Stop";

constexpr char kUpstartJobsPath[] = "/com/ubuntu/Upstart/jobs/";
constexpr char kAuthPolicyJob[] = "authpolicyd";
constexpr char kKerberosJob[] = "kerberosd";
constexpr char kMediaAnalyticsJob[] = "rtanalytics";
// "wilco_5fdtc_5fdispatcher" below refers to the "wilco_dtc_dispatcher" upstart
// job. Upstart escapes characters that aren't valid in D-Bus object paths
// using underscore as the escape character, followed by the character code in
// hex.
constexpr char kWilcoDtcDispatcherJob[] = "wilco_5fdtc_5fdispatcher";

UpstartClient* g_instance = nullptr;

class UpstartClientImpl : public UpstartClient {
 public:
  explicit UpstartClientImpl(dbus::Bus* bus)
      : bus_(bus), weak_ptr_factory_(this) {}

  ~UpstartClientImpl() override = default;

  // UpstartClient overrides:
  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStartMethod, upstart_env, std::move(callback));
  }

  void StopJob(const std::string& job,
               VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStopMethod, {}, std::move(callback));
  }

  void StartAuthPolicyService() override {
    StartJob(kAuthPolicyJob, {}, EmptyVoidDBusMethodCallback());
  }

  void RestartAuthPolicyService() override {
    CallJobMethod(kAuthPolicyJob, kRestartMethod, {},
                  EmptyVoidDBusMethodCallback());
  }

  void StartKerberosService(VoidDBusMethodCallback callback) override {
    StartJob(kKerberosJob, {}, std::move(callback));
  }

  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           VoidDBusMethodCallback callback) override {
    StartJob(kMediaAnalyticsJob, upstart_env, std::move(callback));
  }

  void RestartMediaAnalytics(VoidDBusMethodCallback callback) override {
    CallJobMethod(kMediaAnalyticsJob, kRestartMethod, {}, std::move(callback));
  }

  void StopMediaAnalytics() override {
    StopJob(kMediaAnalyticsJob, EmptyVoidDBusMethodCallback());
  }

  void StopMediaAnalytics(VoidDBusMethodCallback callback) override {
    StopJob(kMediaAnalyticsJob, std::move(callback));
  }

  void StartWilcoDtcService(VoidDBusMethodCallback callback) override {
    StartJob(kWilcoDtcDispatcherJob, {}, std::move(callback));
  }

  void StopWilcoDtcService(VoidDBusMethodCallback callback) override {
    StopJob(kWilcoDtcDispatcherJob, std::move(callback));
  }

 private:
  void CallJobMethod(const std::string& job,
                     const std::string& method,
                     const std::vector<std::string>& upstart_env,
                     VoidDBusMethodCallback callback) {
    dbus::ObjectProxy* job_proxy = bus_->GetObjectProxy(
        kUpstartServiceName, dbus::ObjectPath(kUpstartJobsPath + job));
    dbus::MethodCall method_call(kUpstartJobInterface, method);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(upstart_env);
    writer.AppendBool(true /* wait for response */);
    job_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response);
  }

  dbus::Bus* bus_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpstartClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpstartClientImpl);
};

}  // namespace

UpstartClient::UpstartClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

UpstartClient::~UpstartClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void UpstartClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new UpstartClientImpl(bus);
}

// static
void UpstartClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (to
  // allow test properties to be set).
  if (!FakeUpstartClient::Get())
    new FakeUpstartClient();
}

// static
void UpstartClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
UpstartClient* UpstartClient::Get() {
  return g_instance;
}

}  // namespace chromeos
