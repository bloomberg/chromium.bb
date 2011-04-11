// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

SignedSettings::SignedSettings()
    : service_(OwnershipService::GetSharedInstance()) {
}

SignedSettings::~SignedSettings() {}

SignedSettings::ReturnCode SignedSettings::MapKeyOpCode(
    OwnerManager::KeyOpCode return_code) {
  return (return_code == OwnerManager::KEY_UNAVAILABLE ?
          KEY_UNAVAILABLE : OPERATION_FAILED);
}

void SignedSettings::OnBoolComplete(void* delegate, bool success) {
  SignedSettings::Delegate<bool>* d =
      static_cast< SignedSettings::Delegate<bool>* >(delegate);
  d->OnSettingsOpCompleted(success ? SUCCESS : NOT_FOUND, success);
}

class CheckWhitelistOp : public SignedSettings {
 public:
  CheckWhitelistOp(const std::string& email,
                   SignedSettings::Delegate<bool>* d);
  virtual ~CheckWhitelistOp();
  void Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  const std::string email_;
  SignedSettings::Delegate<bool>* d_;
};

class WhitelistOp : public SignedSettings,
                    public LoginLibrary::Delegate {
 public:
  WhitelistOp(const std::string& email,
              bool add_to_whitelist,
              SignedSettings::Delegate<bool>* d);
  virtual ~WhitelistOp();
  void Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);
  // Implementation of LoginLibrary::Delegate::OnComplete()
  void OnComplete(bool value);

 private:
  bool InitiateWhitelistOp(const std::vector<uint8>& signature);

  const std::string email_;
  const bool add_to_whitelist_;
  SignedSettings::Delegate<bool>* d_;
};

class StorePropertyOp : public SignedSettings,
                        public LoginLibrary::Delegate {
 public:
  StorePropertyOp(const std::string& name,
                  const std::string& value,
                  SignedSettings::Delegate<bool>* d);
  virtual ~StorePropertyOp();
  void Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);
  // Implementation of LoginLibrary::Delegate::OnComplete()
  void OnComplete(bool value);

 private:
  std::string name_;
  std::string value_;
  SignedSettings::Delegate<bool>* d_;
};

class RetrievePropertyOp : public SignedSettings {
 public:
  RetrievePropertyOp(const std::string& name,
                     SignedSettings::Delegate<std::string>* d);
  virtual ~RetrievePropertyOp();
  void Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  // RetrievePropertyCallback
  static void OnRetrievePropertyNotify(void* user_data,
                                       bool success,
                                       const Property* property);

  std::string name_;
  std::string value_;
  SignedSettings::Delegate<std::string>* d_;
};

class StorePolicyOp : public SignedSettings {
 public:
  StorePolicyOp(em::PolicyFetchResponse* policy,
                SignedSettings::Delegate<bool>* d);
  virtual ~StorePolicyOp();
  void Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
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
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  static void OnStringComplete(void* delegate,
                               const char* policy,
                               const unsigned int len);

  em::PolicyFetchResponse policy_;
  SignedSettings::Delegate<const em::PolicyFetchResponse&>* d_;

  void ProcessPolicy(const char* out, const unsigned int len);
};

// static
SignedSettings* SignedSettings::CreateCheckWhitelistOp(
    const std::string& email,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new CheckWhitelistOp(Authenticator::Canonicalize(email), d);
}

// static
SignedSettings* SignedSettings::CreateWhitelistOp(
    const std::string& email,
    bool add_to_whitelist,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new WhitelistOp(Authenticator::Canonicalize(email),
                         add_to_whitelist,
                         d);
}

// static
SignedSettings* SignedSettings::CreateStorePropertyOp(
    const std::string& name,
    const std::string& value,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new StorePropertyOp(name, value, d);
}

// static
SignedSettings* SignedSettings::CreateRetrievePropertyOp(
    const std::string& name,
    SignedSettings::Delegate<std::string>* d) {
  DCHECK(d != NULL);
  return new RetrievePropertyOp(name, d);
}

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

CheckWhitelistOp::CheckWhitelistOp(const std::string& email,
                                   SignedSettings::Delegate<bool>* d)
    : email_(email),
      d_(d) {
}

CheckWhitelistOp::~CheckWhitelistOp() {}

void CheckWhitelistOp::Execute() {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
  std::vector<uint8> sig;
  std::string email_to_check = email_;
  if (!CrosLibrary::Get()->GetLoginLibrary()->CheckWhitelist(
      email_to_check, &sig)) {
    // If the exact match was not found try to match agains a wildcard entry
    // where the domain only matches (e.g. *@example.com). In theory we should
    // always have correctly formated mail address here but a little precaution
    // does no harm.
    if (email_.find('@') != std::string::npos) {
      email_to_check = std::string("*").append(email_.substr(email_.find('@')));
      if (!CrosLibrary::Get()->GetLoginLibrary()->CheckWhitelist(
          email_to_check, &sig)) {
        d_->OnSettingsOpCompleted(NOT_FOUND, false);
        return;
      }
    } else {
      d_->OnSettingsOpCompleted(NOT_FOUND, false);
      return;
    }
  }
  // Posts a task to the FILE thread to verify |sig|.
  service_->StartVerifyAttempt(email_to_check, sig, this);
}

void CheckWhitelistOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& payload) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &CheckWhitelistOp::OnKeyOpComplete,
                          return_code, payload));
    return;
  }
  if (return_code == OwnerManager::SUCCESS) {
    VLOG(2) << "Whitelist check was successful.";
    d_->OnSettingsOpCompleted(SUCCESS, true);
  } else {
    VLOG(2) << "Whitelist check failed.";
    d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code), false);
  }
}

WhitelistOp::WhitelistOp(const std::string& email,
                         bool add_to_whitelist,
                         SignedSettings::Delegate<bool>* d)
    : email_(email),
      add_to_whitelist_(add_to_whitelist),
      d_(d) {
}

WhitelistOp::~WhitelistOp() {}

void WhitelistOp::Execute() {
  // Posts a task to the FILE thread to sign |email_|.
  service_->StartSigningAttempt(email_, this);
}

void WhitelistOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                  const std::vector<uint8>& payload) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &WhitelistOp::OnKeyOpComplete,
                          return_code, payload));
    return;
  }
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS) {
    // OnComplete() will be called by this call, if it succeeds.
    if (!InitiateWhitelistOp(payload))
      d_->OnSettingsOpCompleted(OPERATION_FAILED, false);
  } else {
    d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code), false);
  }
}

void WhitelistOp::OnComplete(bool value) {
  if (value)
    d_->OnSettingsOpCompleted(SUCCESS, value);
  else
    d_->OnSettingsOpCompleted(NOT_FOUND, false);
}

bool WhitelistOp::InitiateWhitelistOp(const std::vector<uint8>& signature) {
  LoginLibrary* library = CrosLibrary::Get()->GetLoginLibrary();
  if (add_to_whitelist_)
    return library->WhitelistAsync(email_, signature, this);
  return library->UnwhitelistAsync(email_, signature, this);
}

StorePropertyOp::StorePropertyOp(const std::string& name,
                                 const std::string& value,
                                 SignedSettings::Delegate<bool>* d)
    : name_(name),
      value_(value),
      d_(d) {
}

StorePropertyOp::~StorePropertyOp() {}

void StorePropertyOp::Execute() {
  if (service_->GetStatus(true) != OwnershipService::OWNERSHIP_TAKEN) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Store(name_, value_,
                                         g_browser_process->local_state())) {
      d_->OnSettingsOpCompleted(SUCCESS, true);
      return;
    }
  }
  // Posts a task to the FILE thread to sign |name_|=|value_|.
  std::string to_sign = base::StringPrintf("%s=%s",
                                           name_.c_str(),
                                           value_.c_str());
  service_->StartSigningAttempt(to_sign, this);
}

void StorePropertyOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                      const std::vector<uint8>& payload) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &StorePropertyOp::OnKeyOpComplete,
                          return_code, payload));
    return;
  }
  VLOG(2) << "StorePropertyOp::OnKeyOpComplete return_code = " << return_code;
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS) {
    // OnComplete() will be called by this call, if it succeeds.
    if (!CrosLibrary::Get()->GetLoginLibrary()->StorePropertyAsync(name_,
                                                                   value_,
                                                                   payload,
                                                                   this)) {
      d_->OnSettingsOpCompleted(OPERATION_FAILED, false);
    }
  } else {
    d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code), false);
  }
}

void StorePropertyOp::OnComplete(bool value) {
  if (value)
    d_->OnSettingsOpCompleted(SUCCESS, value);
  else
    d_->OnSettingsOpCompleted(NOT_FOUND, false);
}

RetrievePropertyOp::RetrievePropertyOp(const std::string& name,
                                       SignedSettings::Delegate<std::string>* d)
    : name_(name),
      d_(d) {
}

RetrievePropertyOp::~RetrievePropertyOp() {}

void RetrievePropertyOp::Execute() {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
  // TODO(dilmah): Fix the race:
  // At the moment when device becomes owned there is lapse of time after
  // device has been owned and before temp_storage settings are finally
  // persisted into signed settings.
  // In this lapse of time Retrieve loses access to those settings.
  if (service_->GetStatus(true) != OwnershipService::OWNERSHIP_TAKEN) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Retrieve(
            name_, &value_, g_browser_process->local_state())) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &RetrievePropertyOp::OnKeyOpComplete,
                            OwnerManager::SUCCESS, std::vector<uint8>()));
      return;
    }
  }

  CrosLibrary::Get()->GetLoginLibrary()->RequestRetrieveProperty(name_,
      &RetrievePropertyOp::OnRetrievePropertyNotify, this);
}

// static
void RetrievePropertyOp::OnRetrievePropertyNotify(void* user_data,
    bool success, const Property* property) {
  RetrievePropertyOp* self = static_cast<RetrievePropertyOp*>(user_data);
  if (!success) {
    self->d_->OnSettingsOpCompleted(NOT_FOUND, std::string());
    return;
  }

  self->value_ = property->value;

  std::vector<uint8> sig;
  sig.assign(property->signature->data,
             property->signature->data + property->signature->length);

  std::string to_verify = base::StringPrintf("%s=%s",
                                             self->name_.c_str(),
                                             self->value_.c_str());
  // Posts a task to the FILE thread to verify |sig|.
  self->service_->StartVerifyAttempt(to_verify, sig, self);
}

void RetrievePropertyOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& payload) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &RetrievePropertyOp::OnKeyOpComplete,
                          return_code, payload));
    return;
  }
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS)
    d_->OnSettingsOpCompleted(SUCCESS, value_);
  else
    d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code),
                              std::string());
}

StorePolicyOp::StorePolicyOp(em::PolicyFetchResponse* policy,
                             SignedSettings::Delegate<bool>* d)
    : policy_(policy),
      d_(d) {
}

StorePolicyOp::~StorePolicyOp() {}

void StorePolicyOp::Execute() {
  // get protobuf contents to sign
  if (!policy_->has_policy_data())
    d_->OnSettingsOpCompleted(OPERATION_FAILED, false);
  if (!policy_->has_policy_data_signature())
    service_->StartSigningAttempt(policy_->policy_data(), this);
  else
    RequestStorePolicy();
}

void StorePolicyOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                    const std::vector<uint8>& payload) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &StorePolicyOp::OnKeyOpComplete,
                          return_code, payload));
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
  d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code), false);
}

void StorePolicyOp::RequestStorePolicy() {
  std::string serialized;
  if (policy_->SerializeToString(&serialized)) {
    CrosLibrary::Get()->GetLoginLibrary()->RequestStorePolicy(
        serialized,
        &SignedSettings::OnBoolComplete,
        d_);
  } else {
    d_->OnSettingsOpCompleted(OPERATION_FAILED, false);
  }
}

RetrievePolicyOp::RetrievePolicyOp(
    SignedSettings::Delegate<const em::PolicyFetchResponse&>* d)
    : d_(d) {
}

RetrievePolicyOp::~RetrievePolicyOp() {}

void RetrievePolicyOp::Execute() {
  CrosLibrary::Get()->GetLoginLibrary()->RequestRetrievePolicy(
      &RetrievePolicyOp::OnStringComplete, this);
}

void RetrievePolicyOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& payload) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &RetrievePolicyOp::OnKeyOpComplete,
                          return_code, payload));
    return;
  }
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS)
    d_->OnSettingsOpCompleted(SUCCESS, policy_);
  else
    d_->OnSettingsOpCompleted(SignedSettings::MapKeyOpCode(return_code),
                              em::PolicyFetchResponse());
}

// static
void RetrievePolicyOp::OnStringComplete(void* delegate,
                                        const char* out,
                                        const unsigned int len) {
  RetrievePolicyOp* op = static_cast<RetrievePolicyOp*>(delegate);
  op->ProcessPolicy(out, len);
}

void RetrievePolicyOp::ProcessPolicy(const char* out, const unsigned int len) {
  if (!out || !policy_.ParseFromString(std::string(out, len)) ||
      (!policy_.has_policy_data() && !policy_.has_policy_data_signature())) {
    d_->OnSettingsOpCompleted(NOT_FOUND, policy_);
    return;
  }
  if (!policy_.has_policy_data() || !policy_.has_policy_data_signature()) {
    d_->OnSettingsOpCompleted(OPERATION_FAILED, em::PolicyFetchResponse());
    return;
  }
  std::vector<uint8> sig;
  const char* sig_ptr = policy_.policy_data_signature().c_str();
  sig.assign(sig_ptr, sig_ptr + policy_.policy_data_signature().length());
  service_->StartVerifyAttempt(policy_.policy_data(), sig, this);
}

}  // namespace chromeos
