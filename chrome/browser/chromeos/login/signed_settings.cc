// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
using content::BrowserThread;
using google::protobuf::RepeatedPtrField;
using std::string;

// static
const char SignedSettings::kDevicePolicyType[] = "google/chromeos/device";

SignedSettings::Relay::Relay(SignedSettings* s) : settings_(s) {
}

SignedSettings::Relay::~Relay() {}

void SignedSettings::Relay::OnSettingsOpCompleted(
    SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& value) {
  if (code == SignedSettings::SUCCESS) {
    settings_->Execute();
    return;
  }
  settings_->Fail(code);
}

SignedSettings::SignedSettings()
    : service_(OwnershipService::GetSharedInstance()),
      relay_(NULL),
      polfetcher_(NULL) {
}

SignedSettings::~SignedSettings() {}

void SignedSettings::TryToFetchPolicyAndCallBack() {
  relay_.reset(new Relay(this));
  polfetcher_ = SignedSettings::CreateRetrievePolicyOp(relay_.get());
  polfetcher_->set_service(service_);
  polfetcher_->Execute();
}

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

// static
bool SignedSettings::EnumerateWhitelist(std::vector<std::string>* whitelisted) {
  OwnershipService* service = OwnershipService::GetSharedInstance();
  if (!service->has_cached_policy())
    return false;
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(service->cached_policy().policy_value());
  if (!pol.has_user_whitelist())
    return false;

  const RepeatedPtrField<std::string>& whitelist =
      pol.user_whitelist().user_whitelist();
  for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
       it != whitelist.end();
       ++it) {
    whitelisted->push_back(*it);
  }
  return true;
}

class CheckWhitelistOp : public SignedSettings {
 public:
  CheckWhitelistOp(const std::string& email,
                   SignedSettings::Delegate<bool>* d);
  virtual ~CheckWhitelistOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(bool value);
  // Implementation of OwnerManager::Delegate
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  bool LookUpInPolicy(const std::string& email);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code, bool value);

  const std::string email_;
  SignedSettings::Delegate<bool>* d_;
};

class WhitelistOp : public SignedSettings,
                    public SignedSettings::Delegate<bool> {
 public:
  WhitelistOp(const std::string& email,
              bool add_to_whitelist,
              SignedSettings::Delegate<bool>* d);
  virtual ~WhitelistOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(bool value);
  // Implementation of OwnerManager::Delegate
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);
  // Implementation of SignedSettings::Delegate
  void OnSettingsOpCompleted(ReturnCode code, bool value);

 private:
  void ModifyWhitelist(const std::string& email,
                       bool add_to_whitelist,
                       em::UserWhitelistProto* whitelist_proto);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code, bool value);

  const std::string email_;
  const bool add_to_whitelist_;
  SignedSettings::Delegate<bool>* d_;
  em::PolicyFetchResponse to_store_;
  scoped_refptr<SignedSettings> store_op_;
};

class StorePropertyOp : public SignedSettings,
                        public SignedSettings::Delegate<bool> {
 public:
  StorePropertyOp(const std::string& name,
                  const base::Value& value,
                  SignedSettings::Delegate<bool>* d);
  virtual ~StorePropertyOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(bool value);
  // Implementation of OwnerManager::Delegate
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);
  // Implementation of SignedSettings::Delegate
  void OnSettingsOpCompleted(ReturnCode code, bool value);

 private:
  void SetInPolicy(const std::string& prop,
                   const base::Value& value,
                   em::PolicyData* poldata);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code, bool value);

  std::string name_;
  scoped_ptr<base::Value> value_;
  SignedSettings::Delegate<bool>* d_;
  em::PolicyFetchResponse to_store_;
  scoped_refptr<SignedSettings> store_op_;
};

class RetrievePropertyOp : public SignedSettings {
 public:
  RetrievePropertyOp(const std::string& name,
                     SignedSettings::Delegate<const base::Value*>* d);
  virtual ~RetrievePropertyOp();
  void Execute();
  void Fail(SignedSettings::ReturnCode code);
  void Succeed(const base::Value* value);
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  base::Value* LookUpInPolicy(const std::string& prop);
  // Always call d_->OnSettingOpCompleted() via this call.
  // It guarantees that the callback will not be triggered until _after_
  // Execute() returns, which is implicitly assumed by SignedSettingsHelper
  // in some cases.
  void PerformCallback(SignedSettings::ReturnCode code,
                       const base::Value* value);

  std::string name_;
  SignedSettings::Delegate<const base::Value*>* d_;
};

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
    const base::Value& value,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new StorePropertyOp(name, value, d);
}

// static
SignedSettings* SignedSettings::CreateRetrievePropertyOp(
    const std::string& name,
    SignedSettings::Delegate<const base::Value*>* d) {
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
  std::vector<uint8> sig;
  std::string email_to_check = email_;
  if (!service_->has_cached_policy()) {
    TryToFetchPolicyAndCallBack();
    return;
  }
  if (LookUpInPolicy(email_to_check)) {
    VLOG(2) << "Whitelist check was successful for " << email_to_check;
    Succeed(true);
    return;
  }
  // If the exact match was not found try to match against a wildcard entry
  // where the domain only matches (e.g. *@example.com). In theory we should
  // always have correctly formated mail address here but a little precaution
  // does no harm.
  if (email_.find('@') != std::string::npos) {
    email_to_check = std::string("*").append(email_.substr(email_.find('@')));
    if (LookUpInPolicy(email_to_check)) {
      VLOG(2) << "Whitelist check was successful for " << email_to_check;
      Succeed(true);
      return;
    }
  }
  Fail(NOT_FOUND);
  return;
}

void CheckWhitelistOp::Fail(SignedSettings::ReturnCode code) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckWhitelistOp::PerformCallback, this, code, false));
}

void CheckWhitelistOp::Succeed(bool value) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckWhitelistOp::PerformCallback, this, SUCCESS, value));
}

void CheckWhitelistOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& payload) {
  NOTREACHED();
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CheckWhitelistOp::OnKeyOpComplete, this, return_code,
                   payload));
    return;
  }
  if (return_code == OwnerManager::SUCCESS) {
    VLOG(2) << "Whitelist check was successful.";
    Succeed(true);
  } else {
    VLOG(2) << "Whitelist check failed.";
    Fail(SignedSettings::MapKeyOpCode(return_code));
  }
}

bool CheckWhitelistOp::LookUpInPolicy(const std::string& email) {
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(service_->cached_policy().policy_value());
  if (!pol.has_user_whitelist())
    return false;

  const RepeatedPtrField<std::string>& whitelist =
      pol.user_whitelist().user_whitelist();
  for (RepeatedPtrField<std::string>::const_iterator it = whitelist.begin();
       it != whitelist.end();
       ++it) {
    if (email == *it)
      return true;
  }
  return false;
}

void CheckWhitelistOp::PerformCallback(SignedSettings::ReturnCode code,
                                       bool value) {
  d_->OnSettingsOpCompleted(code, value);
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
  if (!service_->has_cached_policy()) {
    TryToFetchPolicyAndCallBack();
    return;
  }
  em::PolicyData to_sign;
  to_sign.CheckTypeAndMergeFrom(service_->cached_policy());
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(to_sign.policy_value());
  em::UserWhitelistProto* whitelist_proto = pol.mutable_user_whitelist();
  ModifyWhitelist(email_, add_to_whitelist_, whitelist_proto);
  to_sign.set_policy_value(pol.SerializeAsString());
  to_store_.set_policy_data(to_sign.SerializeAsString());
  service_->StartSigningAttempt(to_store_.policy_data(), this);
}

void WhitelistOp::Fail(SignedSettings::ReturnCode code) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WhitelistOp::PerformCallback, this, code, false));
}

void WhitelistOp::Succeed(bool value) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WhitelistOp::PerformCallback, this, SUCCESS, value));
}

void WhitelistOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                  const std::vector<uint8>& sig) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WhitelistOp::OnKeyOpComplete, this, return_code, sig));
    return;
  }
  VLOG(2) << "WhitelistOp::OnKeyOpComplete return_code = " << return_code;
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS) {
    to_store_.set_policy_data_signature(
        std::string(reinterpret_cast<const char*>(&sig[0]), sig.size()));
    store_op_ = CreateStorePolicyOp(&to_store_, this);
    // d_->OnSettingsOpCompleted() will be called by this call.
    store_op_->Execute();
  } else {
    Fail(SignedSettings::MapKeyOpCode(return_code));
  }
}

void WhitelistOp::OnSettingsOpCompleted(ReturnCode code, bool value) {
  if (value && to_store_.has_policy_data()) {
    em::PolicyData poldata;
    poldata.ParseFromString(to_store_.policy_data());
    service_->set_cached_policy(poldata);
    Succeed(value);
    return;
  }
  Fail(NOT_FOUND);
}

void WhitelistOp::ModifyWhitelist(const std::string& email,
                                  bool add_to_whitelist,
                                  em::UserWhitelistProto* whitelist_proto) {
  int i = 0;
  const RepeatedPtrField<string>& whitelist = whitelist_proto->user_whitelist();
  for (RepeatedPtrField<string>::const_iterator it = whitelist.begin();
       it != whitelist.end();
       ++it, ++i) {
    if (email == *it)
      break;
  }
  // |i| contains the index of |email|, if it is in |whitelist|.
  if (add_to_whitelist) {
    if (i >= whitelist.size())  // |email| was not in |whitelist|, we must add.
      whitelist_proto->add_user_whitelist(email);
    return;
  } else {
    if (i < whitelist.size()) {  // |email| was in |whitelist|, we must remove.
      RepeatedPtrField<string>* change_list =
          whitelist_proto->mutable_user_whitelist();
      change_list->SwapElements(i, whitelist.size() - 1);  // Move to end.
      change_list->RemoveLast();
    }
    return;
  }
  LOG(WARNING) << "Whitelist modification no-op: " << email;
}

void WhitelistOp::PerformCallback(SignedSettings::ReturnCode code, bool value) {
  d_->OnSettingsOpCompleted(code, value);
}

StorePropertyOp::StorePropertyOp(const std::string& name,
                                 const base::Value& value,
                                 SignedSettings::Delegate<bool>* d)
    : name_(name),
      value_(value.DeepCopy()),
      d_(d),
      store_op_(NULL) {
}

StorePropertyOp::~StorePropertyOp() {}

void StorePropertyOp::Execute() {
  if (service_->GetStatus(true) != OwnershipService::OWNERSHIP_TAKEN) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Store(name_, *value_,
                                         g_browser_process->local_state())) {
      Succeed(true);
      return;
    }
  }
  if (!service_->has_cached_policy()) {
    TryToFetchPolicyAndCallBack();
    return;
  }
  // Posts a task to the FILE thread to sign policy.
  em::PolicyData to_sign;
  to_sign.CheckTypeAndMergeFrom(service_->cached_policy());
  SetInPolicy(name_, *value_, &to_sign);
  to_store_.set_policy_data(to_sign.SerializeAsString());
  service_->StartSigningAttempt(to_store_.policy_data(), this);
}

void StorePropertyOp::Fail(SignedSettings::ReturnCode code) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StorePropertyOp::PerformCallback, this, code, false));
}

void StorePropertyOp::Succeed(bool value) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StorePropertyOp::PerformCallback, this, SUCCESS, value));
}

void StorePropertyOp::OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                                      const std::vector<uint8>& sig) {
  // Ensure we're on the UI thread, due to the need to send DBus traffic.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&StorePropertyOp::OnKeyOpComplete, this, return_code, sig));
    return;
  }
  VLOG(2) << "StorePropertyOp::OnKeyOpComplete return_code = " << return_code;
  // Now, sure we're on the UI thread.
  if (return_code == OwnerManager::SUCCESS) {
    to_store_.set_policy_data_signature(
        std::string(reinterpret_cast<const char*>(&sig[0]), sig.size()));
    store_op_ = CreateStorePolicyOp(&to_store_, this);
    // d_->OnSettingsOpCompleted() will be called by this call.
    store_op_->Execute();
  } else {
    Fail(SignedSettings::MapKeyOpCode(return_code));
  }
}

void StorePropertyOp::OnSettingsOpCompleted(ReturnCode code, bool value) {
  if (value && to_store_.has_policy_data()) {
    em::PolicyData poldata;
    poldata.ParseFromString(to_store_.policy_data());
    service_->set_cached_policy(poldata);
    Succeed(value);
    return;
  }
  Fail(NOT_FOUND);
}

void StorePropertyOp::SetInPolicy(const std::string& prop,
                                  const base::Value& value,
                                  em::PolicyData* poldata) {
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(poldata->policy_value());
  if (prop == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = pol.mutable_allow_new_users();
    bool allow_value;
    if (value.GetAsBoolean(&allow_value))
      allow->set_allow_new_users(allow_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = pol.mutable_guest_mode_enabled();
    bool guest_value;
    if (value.GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = pol.mutable_show_user_names();
    bool show_value;
    if (value.GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (prop == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = pol.mutable_data_roaming_enabled();
    bool roaming_value;
    if (value.GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
  } else if (prop == kSettingProxyEverywhere) {
    // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
    std::string proxy_value;
    if (value.GetAsString(&proxy_value)) {
      bool success =
          pol.mutable_device_proxy_settings()->ParseFromString(proxy_value);
      DCHECK(success);
    } else {
      NOTREACHED();
    }
  } else if (prop == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel = pol.mutable_release_channel();
    std::string channel_value;
    if (value.GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (prop == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = pol.mutable_metrics_enabled();
    bool metrics_value;
    if (value.GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
    else
      NOTREACHED();
  } else if (prop == kAccountsPrefUsers) {
    em::UserWhitelistProto* whitelist_proto = pol.mutable_user_whitelist();
    whitelist_proto->clear_user_whitelist();
    const base::ListValue& users = static_cast<const base::ListValue&>(value);
    for (base::ListValue::const_iterator i = users.begin();
         i != users.end(); ++i) {
      std::string email;
      if ((*i)->GetAsString(&email))
        whitelist_proto->add_user_whitelist(email.c_str());
    }
  } else {
    NOTREACHED();
  }
  poldata->set_policy_value(pol.SerializeAsString());
}

void StorePropertyOp::PerformCallback(SignedSettings::ReturnCode code,
                                      bool value) {
  d_->OnSettingsOpCompleted(code, value);
}

RetrievePropertyOp::RetrievePropertyOp(
    const std::string& name,
    SignedSettings::Delegate<const base::Value*>* d)
    : name_(name),
      d_(d) {
}

RetrievePropertyOp::~RetrievePropertyOp() {}

void RetrievePropertyOp::Execute() {
  base::Value* value;
  // TODO(dilmah): Fix the race:
  // At the moment when device becomes owned there is lapse of time after
  // device has been owned and before temp_storage settings are finally
  // persisted into signed settings.
  // In this lapse of time Retrieve loses access to those settings.
  if (service_->GetStatus(true) != OwnershipService::OWNERSHIP_TAKEN) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Retrieve(
            name_, &value, g_browser_process->local_state())) {
      Succeed(value->DeepCopy());
      return;
    }
  }

  if (!service_->has_cached_policy()) {
    TryToFetchPolicyAndCallBack();
    return;
  }
  value = LookUpInPolicy(name_);
  if (!value)
    Fail(NOT_FOUND);
  else
    Succeed(value);
}

void RetrievePropertyOp::Fail(SignedSettings::ReturnCode code) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RetrievePropertyOp::PerformCallback, this,
                 code, static_cast<const base::Value*>(NULL)));
}

void RetrievePropertyOp::Succeed(const base::Value* value) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RetrievePropertyOp::PerformCallback, this,
                 SUCCESS, base::Owned(value)));
}

// DEPRECATED.
void RetrievePropertyOp::OnKeyOpComplete(
    const OwnerManager::KeyOpCode return_code,
    const std::vector<uint8>& sig) {
  NOTREACHED();
}

base::Value* RetrievePropertyOp::LookUpInPolicy(const std::string& prop) {
  if (prop == kDeviceOwner) {
    const em::PolicyData& data = service_->cached_policy();
    if (data.has_username() && !data.has_request_token())
      return base::Value::CreateStringValue(data.username());
  }
  VLOG(2) << "Looking up " << prop;
  em::ChromeDeviceSettingsProto pol;
  pol.ParseFromString(service_->cached_policy().policy_value());
  if (prop == kAccountsPrefAllowNewUser) {
    if (pol.has_allow_new_users() &&
        pol.allow_new_users().has_allow_new_users() &&
        pol.allow_new_users().allow_new_users()) {
      // New users allowed, user_whitelist() ignored.
      return base::Value::CreateBooleanValue(true);
    }
    // If we have the allow_new_users bool, and it is true, we honor that above.
    // In all other cases (don't have it, have it and it is set to false, etc),
    // We will honor the user_whitelist() if it is there and populated.
    // Otherwise we default to allowing new users.
    if (!pol.has_user_whitelist())
      return base::Value::CreateBooleanValue(true);
    return base::Value::CreateBooleanValue(
               pol.user_whitelist().user_whitelist_size() == 0);

  } else if (prop == kAccountsPrefAllowGuest) {
    if (!pol.has_guest_mode_enabled() ||
        !pol.guest_mode_enabled().has_guest_mode_enabled()) {
      // Default to allowing guests;
      return base::Value::CreateBooleanValue(true);
    }
    return base::Value::CreateBooleanValue(
               pol.guest_mode_enabled().guest_mode_enabled());

  } else if (prop == kAccountsPrefShowUserNamesOnSignIn) {
    if (!pol.has_show_user_names() ||
        !pol.show_user_names().has_show_user_names()) {
      // Default to showing pods on the login screen;
      return base::Value::CreateBooleanValue(true);
    }
    return base::Value::CreateBooleanValue(
               pol.show_user_names().show_user_names());

  } else if (prop == kSignedDataRoamingEnabled) {
    if (!pol.has_data_roaming_enabled() ||
        !pol.data_roaming_enabled().has_data_roaming_enabled()) {
      // Default to disabling cellular data roaming;
      return base::Value::CreateBooleanValue(false);
    }
    return base::Value::CreateBooleanValue(
               pol.data_roaming_enabled().data_roaming_enabled());

  } else if (prop == kSettingProxyEverywhere) {
    // TODO(cmasone): NOTIMPLEMENTED() once http://crosbug.com/13052 is fixed.
    std::string serialized;
    if (pol.has_device_proxy_settings() &&
        pol.device_proxy_settings().SerializeToString(&serialized)) {
      return base::Value::CreateStringValue(serialized);
    }

  } else if (prop == kReleaseChannel) {
    if (!pol.has_release_channel() ||
        !pol.release_channel().has_release_channel()) {
      // Default to an invalid channel (will be ignored).
      return base::Value::CreateStringValue("");
    }
    return base::Value::CreateStringValue(
               pol.release_channel().release_channel());

  } else if (prop == kStatsReportingPref) {
    if (pol.has_metrics_enabled()) {
      return base::Value::CreateBooleanValue(
                 pol.metrics_enabled().metrics_enabled());
    }
  } else if (prop == kAccountsPrefUsers) {
    base::ListValue* list = new base::ListValue();
    const em::UserWhitelistProto& whitelist_proto = pol.user_whitelist();
    const RepeatedPtrField<string>& whitelist =
        whitelist_proto.user_whitelist();
    for (RepeatedPtrField<string>::const_iterator it = whitelist.begin();
         it != whitelist.end(); ++it) {
      list->Append(base::Value::CreateStringValue(*it));
    }
    return list;
  }
  return NULL;
}

void RetrievePropertyOp::PerformCallback(SignedSettings::ReturnCode code,
                                         const base::Value* value) {
  d_->OnSettingsOpCompleted(code, value);
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
    service_->set_cached_policy(poldata);
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
    service_->set_cached_policy(poldata);
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
