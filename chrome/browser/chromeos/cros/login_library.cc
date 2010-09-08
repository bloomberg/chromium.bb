// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/login_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class LoginLibraryImpl : public LoginLibrary {
 public:
  LoginLibraryImpl()
      : set_owner_key_callback_(NULL),
        whitelist_op_callback_(NULL),
        property_op_callback_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded())
      Init();
  }
  virtual ~LoginLibraryImpl() {
    if (session_connection_) {
      chromeos::DisconnectSession(session_connection_);
    }
  }

  bool EmitLoginPromptReady() {
    return chromeos::EmitLoginPromptReady();
  }

  bool CheckWhitelist(const std::string& email,
                      std::vector<uint8>* OUT_signature) {
    return chromeos::CheckWhitelist(email.c_str(), OUT_signature);
  }

  bool RetrieveProperty(const std::string& name,
                        std::string* OUT_value,
                        std::vector<uint8>* OUT_signature) {
    return chromeos::RetrieveProperty(name.c_str(), OUT_value, OUT_signature);
  }

  bool SetOwnerKeyAsync(const std::vector<uint8>& public_key_der,
                        Delegate<bool>* callback) {
    DCHECK(callback) << "must provide a callback to SetOwnerKeyAsync()";
    if (set_owner_key_callback_)
      return false;
    set_owner_key_callback_ =  callback;
    return chromeos::SetOwnerKey(public_key_der);
  }

  bool StorePropertyAsync(const std::string& name,
                          const std::string& value,
                          const std::vector<uint8>& signature,
                          Delegate<bool>* callback) {
    DCHECK(callback) << "must provide a callback to StorePropertyAsync()";
    if (property_op_callback_)
      return false;
    property_op_callback_ = callback;
    return chromeos::StoreProperty(name.c_str(), value.c_str(), signature);
  }

  bool UnwhitelistAsync(const std::string& email,
                        const std::vector<uint8>& signature,
                        Delegate<bool>* callback) {
    DCHECK(callback) << "must provide a callback to UnwhitelistAsync()";
    if (whitelist_op_callback_)
      return false;
    whitelist_op_callback_ =  callback;
    return chromeos::Unwhitelist(email.c_str(), signature);
  }

  bool WhitelistAsync(const std::string& email,
                      const std::vector<uint8>& signature,
                      Delegate<bool>* callback) {
    DCHECK(callback) << "must provide a callback to WhitelistAsync()";
    if (whitelist_op_callback_)
      return false;
    whitelist_op_callback_ =  callback;
    return chromeos::Whitelist(email.c_str(), signature);
  }

  bool EnumerateWhitelisted(std::vector<std::string>* whitelisted) {
    return chromeos::EnumerateWhitelisted(whitelisted);
  }

  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */) {
    // only pass unique_id through once we use it for something.
    return chromeos::StartSession(user_email.c_str(), "");
  }

  bool StopSession(const std::string& unique_id /* unused */) {
    // only pass unique_id through once we use it for something.
    return chromeos::StopSession("");
  }

  bool RestartJob(int pid, const std::string& command_line) {
    return chromeos::RestartJob(pid, command_line.c_str());
  }

 private:
  static void Handler(void* object, const OwnershipEvent& event) {
    LoginLibraryImpl* self = static_cast<LoginLibraryImpl*>(object);
    switch (event) {
      case SetKeySuccess:
        self->CompleteSetOwnerKey(true);
        break;
      case SetKeyFailure:
        self->CompleteSetOwnerKey(false);
        break;
      case WhitelistOpSuccess:
        self->CompleteWhitelistOp(true);
        break;
      case WhitelistOpFailure:
        self->CompleteWhitelistOp(false);
        break;
      case PropertyOpSuccess:
        self->CompletePropertyOp(true);
        break;
      case PropertyOpFailure:
        self->CompletePropertyOp(false);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void Init() {
    session_connection_ = chromeos::MonitorSession(&Handler, this);
  }

  void CompleteSetOwnerKey(bool result) {
    CHECK(set_owner_key_callback_) << "CompleteSetOwnerKey() called without "
                                      "a registered callback!";
    set_owner_key_callback_->Run(result);
    set_owner_key_callback_ = NULL;
  }

  void CompleteWhitelistOp(bool result) {
    CHECK(whitelist_op_callback_);
    whitelist_op_callback_->Run(result);
    whitelist_op_callback_ = NULL;
  }

  void CompletePropertyOp(bool result) {
    CHECK(property_op_callback_);
    property_op_callback_->Run(result);
    property_op_callback_ = NULL;
  }

  chromeos::SessionConnection session_connection_;

  Delegate<bool>* set_owner_key_callback_;
  Delegate<bool>* whitelist_op_callback_;
  Delegate<bool>* property_op_callback_;

  DISALLOW_COPY_AND_ASSIGN(LoginLibraryImpl);
};

class LoginLibraryStubImpl : public LoginLibrary {
 public:
  LoginLibraryStubImpl() {}
  virtual ~LoginLibraryStubImpl() {}

  bool EmitLoginPromptReady() { return true; }
  bool CheckWhitelist(const std::string& email,
                      std::vector<uint8>* OUT_signature) {
    OUT_signature->assign(2, 0);
    return true;
  }
  bool RetrieveProperty(const std::string& name,
                        std::string* OUT_value,
                        std::vector<uint8>* OUT_signature) {
    OUT_value->assign("stub");
    OUT_signature->assign(2, 0);
    return true;
  }
  bool SetOwnerKeyAsync(const std::vector<uint8>& public_key_der,
                        Delegate<bool>* callback) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool StorePropertyAsync(const std::string& name,
                          const std::string& value,
                          const std::vector<uint8>& signature,
                          Delegate<bool>* callback) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool UnwhitelistAsync(const std::string& email,
                        const std::vector<uint8>& signature,
                        Delegate<bool>* callback) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool WhitelistAsync(const std::string& email,
                      const std::vector<uint8>& signature,
                      Delegate<bool>* callback) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool EnumerateWhitelisted(std::vector<std::string>* whitelisted) {
    return true;
  }
  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */) { return true; }
  bool StopSession(const std::string& unique_id /* unused */) { return true; }
  bool RestartJob(int pid, const std::string& command_line) { return true; }

 private:
  static void DoStubCallback(Delegate<bool>* callback) {
    callback->Run(true);
  }

  DISALLOW_COPY_AND_ASSIGN(LoginLibraryStubImpl);
};

// static
LoginLibrary* LoginLibrary::GetImpl(bool stub) {
  if (stub)
    return new LoginLibraryStubImpl();
  else
    return new LoginLibraryImpl();
}

}  // namespace chromeos
