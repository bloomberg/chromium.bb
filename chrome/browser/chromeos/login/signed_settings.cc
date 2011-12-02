// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
using content::BrowserThread;
using google::protobuf::RepeatedPtrField;
using std::string;

const char kDevicePolicyType[] = "google/chromeos/device";

SignedSettings::SignedSettings()
    : service_(OwnershipService::GetSharedInstance()) {
}

SignedSettings::~SignedSettings() {}

// static
bool SignedSettings::PolicyIsSane(const em::PolicyFetchResponse& value,
                                  em::PolicyData* poldata) {
  if (value.has_policy_data()) {
    poldata->ParseFromString(value.policy_data());
    if (poldata->has_policy_type() &&
        poldata->policy_type() == kDevicePolicyType &&
        poldata->has_policy_value()) {
      return true;
    }
  }
  return false;
}

// static
SignedSettings::ReturnCode SignedSettings::MapKeyOpCode(
    OwnerManager::KeyOpCode return_code) {
  return (return_code == OwnerManager::KEY_UNAVAILABLE ?
          KEY_UNAVAILABLE : BAD_SIGNATURE);
}

class StorePolicyOp : public SignedSettings {
 public:
  StorePolicyOp(em::PolicyFetchResponse* policy,
                SignedSettings::Delegate<bool>* d);
  virtual ~StorePolicyOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(bool value);
  // Implementation of OwnerManager::Delegate
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  void OnBoolComplete(bool success);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code, bool value);

  em::PolicyFetchResponse* policy_;
  SignedSettings::Delegate<bool>* d_;

  void RequestStorePolicy();
};

class RetrievePolicyOp : public SignedSettings {
 public:
  explicit RetrievePolicyOp(
      SignedSettings::Delegate<const em::PolicyFetchResponse&>* d);
  virtual ~RetrievePolicyOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(const em::PolicyFetchResponse& value);
  // Implementation of OwnerManager::Delegate
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  void OnStringComplete(const std::string& serialized_proto);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code,
                       const em::PolicyFetchResponse& value);

  void ProcessPolicy(const std::string& serialized_proto);

  em::PolicyFetchResponse policy_;
  SignedSettings::Delegate<const em::PolicyFetchResponse&>* d_;
};

// static
SignedSettings* SignedSettings::CreateStorePolicyOp(
    em::PolicyFetchResponse* policy,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  DCHECK(policy != NULL);
  return new StorePolicyOp(policy, d);
}

// static
SignedSettings* SignedSettings::CreateRetrievePolicyOp(
    SignedSettings::Delegate<const em::PolicyFetchResponse&>* d) {
  DCHECK(d != NULL);
  return new RetrievePolicyOp(d);
}


StorePolicyOp::StorePolicyOp(em::PolicyFetchResponse* policy,
                             SignedSettings::Delegate<bool>* d)
    : policy_(policy),
      d_(d) {
}

StorePolicyOp::~StorePolicyOp() {}

void StorePolicyOp::OnBoolComplete(bool success) {
  if (success)
    Succeed(true);
  else
    Fail(NOT_FOUND);
}

void StorePolicyOp::Execute() {
  // get protobuf contents to sign
  if (!policy_->has_policy_data())
    Fail(OPERATION_FAILED);
  else if (!policy_->has_policy_data_signature())
    service_->StartSigningAttempt(policy_->policy_data(), this);
  else
    RequestStorePolicy();
}

void StorePolicyOp::Fail(SignedSettings::ReturnCode code) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StorePolicyOp::PerformCallback, this, code, false));
}

void StorePolicyOp::Succeed(bool ignored) {
  SignedSettings::ReturnCode code = SUCCESS;
  bool to_ret = true;
  em::PolicyData poldata;
  if (SignedSettings::PolicyIsSane(*policy_, &poldata)) {
  } else {
    code = NOT_FOUND;
    to_ret = false;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StorePolicyOp::PerformCallback, this, code, to_ret));
}

void StorePolicyOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                    const std::vector<uint8>& payload) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&StorePolicyOp::OnKeyOpComplete, this, return_code,
                   payload));
    return;
  }
  VLOG(2) << "StorePolicyOp::OnKeyOpComplete return_code = " << return_code;
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS) {
    policy_->set_policy_data_signature(std::string(payload.begin(),
                                                   payload.end()));
    RequestStorePolicy();
    return;
  }
  Fail(SignedSettings::MapKeyOpCode(return_code));
}

void StorePolicyOp::RequestStorePolicy() {
  std::string serialized;
  if (policy_->SerializeToString(&serialized)) {
    DBusThreadManager::Get()->GetSessionManagerClient()->StorePolicy(
        serialized,
        base::Bind(&StorePolicyOp::OnBoolComplete, this));
  } else {
    Fail(OPERATION_FAILED);
  }
}

void StorePolicyOp::PerformCallback(SignedSettings::ReturnCode code,
                                    bool value) {
  d_->OnSettingsOpCompleted(code, value);
}

RetrievePolicyOp::RetrievePolicyOp(
    SignedSettings::Delegate<const em::PolicyFetchResponse&>* d)
    : d_(d) {
}

RetrievePolicyOp::~RetrievePolicyOp() {}

void RetrievePolicyOp::Execute() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RetrievePolicy(
      base::Bind(&RetrievePolicyOp::OnStringComplete, this));
}

void RetrievePolicyOp::Fail(SignedSettings::ReturnCode code) {
  VLOG(2) << "RetrievePolicyOp::Execute() failed with " << code;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RetrievePolicyOp::PerformCallback, this, code,
                 em::PolicyFetchResponse()));
}

void RetrievePolicyOp::Succeed(const em::PolicyFetchResponse& value) {
  em::PolicyData poldata;
  if (SignedSettings::PolicyIsSane(value, &poldata)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RetrievePolicyOp::PerformCallback, this, SUCCESS, value));
  } else {
    Fail(NOT_FOUND);
  }
}

void RetrievePolicyOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& payload) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RetrievePolicyOp::OnKeyOpComplete, this, return_code,
                   payload));
    return;
  }
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS)
    Succeed(policy_);
  else
    Fail(SignedSettings::MapKeyOpCode(return_code));
}

void RetrievePolicyOp::OnStringComplete(const std::string& serialized_proto) {
  ProcessPolicy(serialized_proto);
}

void RetrievePolicyOp::ProcessPolicy(const std::string& serialized_proto) {
  if (serialized_proto.empty() || !policy_.ParseFromString(serialized_proto) ||
      (!policy_.has_policy_data() && !policy_.has_policy_data_signature())) {
    Fail(NOT_FOUND);
    return;
  }
  if (!policy_.has_policy_data()) {
    Fail(OPERATION_FAILED);
    return;
  }
  if (!policy_.has_policy_data_signature()) {
    Fail(BAD_SIGNATURE);
    return;
  }
  std::vector<uint8> sig;
  const char* sig_ptr = policy_.policy_data_signature().c_str();
  sig.assign(sig_ptr, sig_ptr + policy_.policy_data_signature().length());
  service_->StartVerifyAttempt(policy_.policy_data(), sig, this);
}

void RetrievePolicyOp::PerformCallback(SignedSettings::ReturnCode code,
                                       const em::PolicyFetchResponse& value) {
  d_->OnSettingsOpCompleted(code, value);
}

}  // namespace chromeos
