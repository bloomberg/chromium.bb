// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/login_library.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace em = enterprise_management;
namespace chromeos {

class LoginLibraryImpl : public LoginLibrary {
 public:
  LoginLibraryImpl()
      : job_restart_request_(NULL),
        set_owner_key_callback_(NULL),
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
    CryptoBlob* sig = NULL;
    if (chromeos::CheckWhitelistSafe(email.c_str(), &sig)) {
      OUT_signature->assign(sig->data, sig->data + sig->length);
      chromeos::FreeCryptoBlob(sig);
      return true;
    }
    return false;
  }

  void RequestRetrievePolicy(RetrievePolicyCallback callback, void* delegate) {
    DCHECK(callback) << "must provide a callback to RequestRetrievePolicy()";
    chromeos::RetrievePolicy(callback, delegate);
  }

  void RequestRetrieveProperty(const std::string& name,
                               RetrievePropertyCallback callback,
                               void* user_data) {
    DCHECK(callback) << "must provide a callback to RequestRetrieveProperty()";
    chromeos::RequestRetrieveProperty(name.c_str(), callback, user_data);
  }

  void RequestStorePolicy(const std::string& policy,
                          StorePolicyCallback callback,
                          void* delegate) {
    DCHECK(callback) << "must provide a callback to StorePolicy()";
    chromeos::StorePolicy(policy.c_str(), policy.length(), callback, delegate);
  }

  bool StorePropertyAsync(const std::string& name,
                          const std::string& value,
                          const std::vector<uint8>& signature,
                          Delegate* callback) {
    DCHECK(callback) << "must provide a callback to StorePropertyAsync()";
    if (property_op_callback_)
      return false;
    property_op_callback_ = callback;
    Property* prop = chromeos::CreateProperty(name.c_str(),
                                              value.c_str(),
                                              &signature[0],
                                              signature.size());
    bool rv = chromeos::StorePropertySafe(prop);
    chromeos::FreeProperty(prop);
    return rv;
  }

  bool UnwhitelistAsync(const std::string& email,
                        const std::vector<uint8>& signature,
                        Delegate* callback) {
    DCHECK(callback) << "must provide a callback to UnwhitelistAsync()";
    if (whitelist_op_callback_)
      return false;
    whitelist_op_callback_ =  callback;
    CryptoBlob* sig = chromeos::CreateCryptoBlob(&signature[0],
                                                 signature.size());
    bool rv = chromeos::UnwhitelistSafe(email.c_str(), sig);
    chromeos::FreeCryptoBlob(sig);
    return rv;
  }

  bool WhitelistAsync(const std::string& email,
                      const std::vector<uint8>& signature,
                      Delegate* callback) {
    DCHECK(callback) << "must provide a callback to WhitelistAsync()";
    if (whitelist_op_callback_)
      return false;
    whitelist_op_callback_ =  callback;
    CryptoBlob* sig = chromeos::CreateCryptoBlob(&signature[0],
                                                 signature.size());
    bool rv = chromeos::WhitelistSafe(email.c_str(), sig);
    chromeos::FreeCryptoBlob(sig);
    return rv;
  }

  // DEPRECATED.
  bool EnumerateWhitelisted(std::vector<std::string>* whitelisted) {
    NOTREACHED();
    UserList* list = NULL;
    if (chromeos::EnumerateWhitelistedSafe(&list)) {
      for (int i = 0; i < list->num_users; i++)
        whitelisted->push_back(std::string(list->users[i]));
      chromeos::FreeUserList(list);
      return true;
    }
    return false;
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

  bool RestartEntd() {
    return chromeos::RestartEntd();
  }

  bool RestartJob(int pid, const std::string& command_line) {
    if (job_restart_request_) {
      NOTREACHED();
      return false;
    }
    job_restart_request_ = new JobRestartRequest(pid, command_line);
    return true;
  }

 private:
  class JobRestartRequest
      : public base::RefCountedThreadSafe<JobRestartRequest> {
   public:
    JobRestartRequest(int pid, const std::string& command_line)
        : pid_(pid),
          command_line_(command_line),
          local_state_(g_browser_process->local_state()) {
      AddRef();
      if (local_state_) {
        // XXX: normally this call must not be needed, however RestartJob
        // just kills us so settings may be lost. See http://crosbug.com/13102
        local_state_->CommitPendingWrite();
        timer_.Start(
            base::TimeDelta::FromSeconds(3), this,
            &JobRestartRequest::RestartJob);
        // Post task on file thread thus it occurs last on task queue, so it
        // would be executed after committing pending write on file thread.
        BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            NewRunnableMethod(this, &JobRestartRequest::RestartJob));
      } else {
        RestartJob();
      }
    }

   private:
    void RestartJob() {
      if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
        if (!chromeos::RestartJob(pid_, command_line_.c_str()))
          NOTREACHED();
      } else {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableMethod(this, &JobRestartRequest::RestartJob));
        MessageLoop::current()->AssertIdle();
      }
    }

    int pid_;
    std::string command_line_;
    PrefService* local_state_;
    base::OneShotTimer<JobRestartRequest> timer_;
  };

  class StubDelegate
      : public SignedSettings::Delegate<const em::PolicyFetchResponse&> {
   public:
    StubDelegate() : polfetcher_(NULL) {}
    virtual ~StubDelegate() {}
    void set_fetcher(SignedSettings* s) { polfetcher_ = s; }
    SignedSettings* fetcher() { return polfetcher_.get(); }
    // Implementation of SignedSettings::Delegate
    virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                       const em::PolicyFetchResponse& value) {
      VLOG(2) << "Done Fetching Policy";
      delete this;
    }
   private:
    scoped_refptr<SignedSettings> polfetcher_;
    DISALLOW_COPY_AND_ASSIGN(StubDelegate);
  };

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

  void CompleteSetOwnerKey(bool value) {
    VLOG(1) << "Owner key generation: " << (value ? "success" : "fail");
    NotificationType result =
        NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;
    if (!value)
      result = NotificationType::OWNER_KEY_FETCH_ATTEMPT_FAILED;

    // Whether we exported the public key or not, send a notification indicating
    // that we're done with this attempt.
    NotificationService::current()->Notify(result,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());

    // We stored some settings in transient storage before owner was assigned.
    // Now owner is assigned and key is generated and we should persist
    // those settings into signed storage.
    if (g_browser_process && g_browser_process->local_state()) {
      SignedSettingsTempStorage::Finalize(g_browser_process->local_state());
    }
  }

  void CompleteWhitelistOp(bool result) {
    if (whitelist_op_callback_) {
      whitelist_op_callback_->OnComplete(result);
      whitelist_op_callback_ = NULL;
    }
  }

  void CompletePropertyOp(bool result) {
    if (result) {
      StubDelegate* stub = new StubDelegate();  // Manages its own lifetime.
      stub->set_fetcher(SignedSettings::CreateRetrievePolicyOp(stub));
      stub->fetcher()->Execute();
    }
  }

  chromeos::SessionConnection session_connection_;
  JobRestartRequest* job_restart_request_;

  Delegate* set_owner_key_callback_;
  Delegate* whitelist_op_callback_;
  Delegate* property_op_callback_;

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
  void RequestRetrievePolicy(RetrievePolicyCallback callback, void* delegate) {
    callback(delegate, "", 0);
  }
  void RequestRetrieveProperty(const std::string& name,
                               RetrievePropertyCallback callback,
                               void* user_data) {
    uint8 sig_bytes[] = { 0, 0 };
    CryptoBlob sig = { sig_bytes, arraysize(sig_bytes) };
    Property prop = {
      "prop_name",
      "stub",
      &sig,
    };

    callback(user_data, true, &prop);
  }
  void RequestStorePolicy(const std::string& policy,
                          StorePolicyCallback callback,
                          void* delegate) {
    callback(delegate, true);
  }
  bool StorePropertyAsync(const std::string& name,
                          const std::string& value,
                          const std::vector<uint8>& signature,
                          Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool UnwhitelistAsync(const std::string& email,
                        const std::vector<uint8>& signature,
                        Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&DoStubCallback, callback));
    return true;
  }
  bool WhitelistAsync(const std::string& email,
                      const std::vector<uint8>& signature,
                      Delegate* callback) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
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
  bool RestartEntd() { return true; }

 private:
  static void DoStubCallback(Delegate* callback) {
    callback->OnComplete(true);
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
