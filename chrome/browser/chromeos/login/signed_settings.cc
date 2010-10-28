// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"

namespace chromeos {

SignedSettings::SignedSettings()
    : service_(OwnershipService::GetSharedInstance()) {
}

SignedSettings::~SignedSettings() {}

class CheckWhitelistOp : public SignedSettings {
 public:
  CheckWhitelistOp(const std::string& email,
                   SignedSettings::Delegate<bool>* d);
  virtual ~CheckWhitelistOp();
  bool Execute();
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
  bool Execute();
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
  bool Execute();
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
  bool Execute();
  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload);

 private:
  std::string name_;
  std::string value_;
  SignedSettings::Delegate<std::string>* d_;
};

// static
SignedSettings* SignedSettings::CreateCheckWhitelistOp(
    const std::string& email,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new CheckWhitelistOp(email, d);
}

// static
SignedSettings* SignedSettings::CreateWhitelistOp(
    const std::string& email,
    bool add_to_whitelist,
    SignedSettings::Delegate<bool>* d) {
  DCHECK(d != NULL);
  return new WhitelistOp(email, add_to_whitelist, d);
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

CheckWhitelistOp::CheckWhitelistOp(const std::string& email,
                                   SignedSettings::Delegate<bool>* d)
    : email_(email),
      d_(d) {
}

CheckWhitelistOp::~CheckWhitelistOp() {}

bool CheckWhitelistOp::Execute() {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
  std::vector<uint8> sig;
  if (!CrosLibrary::Get()->GetLoginLibrary()->CheckWhitelist(email_, &sig))
    return false;

  // Posts a task to the FILE thread to verify |sig|.
  service_->StartVerifyAttempt(email_, sig, this);
  return true;
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
  if (return_code == OwnerManager::SUCCESS)
    d_->OnSettingsOpSucceeded(true);
  else
    d_->OnSettingsOpFailed();
}

WhitelistOp::WhitelistOp(const std::string& email,
                         bool add_to_whitelist,
                         SignedSettings::Delegate<bool>* d)
    : email_(email),
      add_to_whitelist_(add_to_whitelist),
      d_(d) {
}

WhitelistOp::~WhitelistOp() {}

bool WhitelistOp::Execute() {
  // Posts a task to the FILE thread to sign |email_|.
  service_->StartSigningAttempt(email_, this);
  return true;
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
  bool success = false;
  if (return_code == OwnerManager::SUCCESS) {
    // OnComplete() will be called when this is done.
    success = InitiateWhitelistOp(payload);
  }
  if (!success)
    d_->OnSettingsOpFailed();
}

void WhitelistOp::OnComplete(bool value) {
  if (value)
    d_->OnSettingsOpSucceeded(value);
  else
    d_->OnSettingsOpFailed();
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

bool StorePropertyOp::Execute() {
  if (!service_->IsAlreadyOwned()) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Store(name_, value_,
                                         g_browser_process->local_state())) {
      d_->OnSettingsOpSucceeded(true);
      return true;
    }
  }
  // Posts a task to the FILE thread to sign |name_|=|value_|.
  std::string to_sign = base::StringPrintf("%s=%s",
                                           name_.c_str(),
                                           value_.c_str());
  service_->StartSigningAttempt(to_sign, this);
  return true;
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
  // Now, sure we're on the UI thread.
  bool success = false;
  if (return_code == OwnerManager::SUCCESS) {
    // OnComplete() will be called when this is done.
    success = CrosLibrary::Get()->GetLoginLibrary()->StorePropertyAsync(name_,
                                                                        value_,
                                                                        payload,
                                                                        this);
  }
  if (!success)
    d_->OnSettingsOpFailed();
}

void StorePropertyOp::OnComplete(bool value) {
  if (value)
    d_->OnSettingsOpSucceeded(value);
  else
    d_->OnSettingsOpFailed();
}

RetrievePropertyOp::RetrievePropertyOp(const std::string& name,
                                       SignedSettings::Delegate<std::string>* d)
    : name_(name),
      d_(d) {
}

RetrievePropertyOp::~RetrievePropertyOp() {}

bool RetrievePropertyOp::Execute() {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
  // TODO(dilmah): Fix the race:
  // At the moment when device becomes owned there is lapse of time after
  // device has been owned and before temp_storage settings are finally
  // persisted into signed settings.
  // In this lapse of time Retrieve loses access to those settings.
  if (!service_->IsAlreadyOwned()) {
    if (g_browser_process &&
        g_browser_process->local_state() &&
        SignedSettingsTempStorage::Retrieve(
            name_, &value_, g_browser_process->local_state())) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &RetrievePropertyOp::OnKeyOpComplete,
                            OwnerManager::SUCCESS, std::vector<uint8>()));
      return true;
    }
  }
  std::vector<uint8> sig;
  if (!CrosLibrary::Get()->GetLoginLibrary()->RetrieveProperty(name_,
                                                               &value_,
                                                               &sig)) {
    return false;
  }
  std::string to_verify = base::StringPrintf("%s=%s",
                                             name_.c_str(),
                                             value_.c_str());
  // Posts a task to the FILE thread to verify |sig|.
  service_->StartVerifyAttempt(to_verify, sig, this);
  return true;
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
    d_->OnSettingsOpSucceeded(value_);
  else
    d_->OnSettingsOpFailed();
}

}  // namespace chromeos
