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
      : set_owner_key_callback_(NULL) {
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

  bool SetOwnerKey(const std::vector<uint8>& public_key_der,
                   Delegate<bool>* callback) {
    DCHECK(callback) << "must provide a callback to SetOwnerKey()";
    set_owner_key_callback_ =  callback;
    return chromeos::SetOwnerKey(public_key_der);
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
      case WhitelistOpFailure:
      case PropertyOpSuccess:
      case PropertyOpFailure:
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED();
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

  chromeos::SessionConnection session_connection_;

  Delegate<bool>* set_owner_key_callback_;

  DISALLOW_COPY_AND_ASSIGN(LoginLibraryImpl);
};

class LoginLibraryStubImpl : public LoginLibrary {
 public:
  LoginLibraryStubImpl() {}
  virtual ~LoginLibraryStubImpl() {}

  bool EmitLoginPromptReady() { return true; }
  bool SetOwnerKey(const std::vector<uint8>& public_key_der,
                   Delegate<bool>* callback) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&SetOwnerKeyStubCallback, callback));
    return true;
  }
  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */) { return true; }
  bool StopSession(const std::string& unique_id /* unused */) { return true; }
  bool RestartJob(int pid, const std::string& command_line) { return true; }

 private:
  static void SetOwnerKeyStubCallback(Delegate<bool>* callback) {
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
