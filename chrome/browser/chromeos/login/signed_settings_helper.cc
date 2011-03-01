// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_helper.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

namespace {

class OpContext {
 public:
  class Delegate {
   public:
    virtual void OnOpCreated(OpContext* context) = 0;
    virtual void OnOpStarted(OpContext* context) = 0;
    virtual void OnOpCompleted(OpContext* context) = 0;
  };

  virtual ~OpContext() {}

  // Creates and execute op.
  void Execute() {
    CreateOp();
    CHECK(op_.get());
    if (delegate_)
      delegate_->OnOpCreated(this);

    // Note that the context could be released when op_->Execute() returns.
    // So keep a local copy of delegate and executing flag to use after
    // the call.
    Delegate* delegate = delegate_;
    executing_ = true;
    op_->Execute();
    if (delegate)
      delegate->OnOpStarted(this);
  }

  // Cancels the callback.
  void CancelCallback() {
    callback_ = NULL;
  }

  // Cancels the callback and cancels the op if it is not executing.
  void Cancel() {
    CancelCallback();

   if (!executing_)
      OnOpCompleted();
  }

  // Accessors.
  SignedSettings* op() const {
    return op_.get();
  }

  SignedSettingsHelper::Callback* callback() const {
    return callback_;
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  OpContext(SignedSettingsHelper::Callback* callback,
            Delegate* delegate)
      : executing_(false),
        delegate_(delegate),
        callback_(callback) {
  }

  // Creates the op to execute.
  virtual void CreateOp() = 0;

  // Callback on op completion.
  virtual void OnOpCompleted() {
    if (delegate_)
      delegate_->OnOpCompleted(this);

    delete this;
  }

  bool executing_;
  Delegate* delegate_;

  scoped_refptr<SignedSettings> op_;
  SignedSettingsHelper::Callback* callback_;
};

class WhitelistOpContext : public SignedSettings::Delegate<bool>,
                           public OpContext {
 public:
  enum Type {
    CHECK,
    ADD,
    REMOVE,
  };

  WhitelistOpContext(Type type,
                     const std::string& email,
                     SignedSettingsHelper::Callback* callback,
                     Delegate* delegate)
      : OpContext(callback, delegate),
        type_(type),
        email_(email) {
  }

  // chromeos::SignedSettings::Delegate implementation
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     bool value) {
    if (callback_) {
      switch (type_) {
        case CHECK:
          callback_->OnCheckWhitelistCompleted(code, email_);
          break;
        case ADD:
          callback_->OnWhitelistCompleted(code, email_);
          break;
        case REMOVE:
          callback_->OnUnwhitelistCompleted(code, email_);
          break;
        default:
          LOG(ERROR) << "Unknown WhitelistOpContext type " << type_;
          break;
      }
    }
    OnOpCompleted();
  }

 protected:
  // OpContext implemenetation
  virtual void CreateOp() {
    switch (type_) {
      case CHECK:
        op_ = SignedSettings::CreateCheckWhitelistOp(email_, this);
        break;
      case ADD:
        op_ = SignedSettings::CreateWhitelistOp(email_, true, this);
        break;
      case REMOVE:
        op_ = SignedSettings::CreateWhitelistOp(email_, false, this);
        break;
      default:
        LOG(ERROR) << "Unknown WhitelistOpContext type " << type_;
        break;
    }
  }

 private:
  Type type_;
  std::string email_;

  DISALLOW_COPY_AND_ASSIGN(WhitelistOpContext);
};

class StorePropertyOpContext : public SignedSettings::Delegate<bool>,
                               public OpContext {
 public:
  StorePropertyOpContext(const std::string& name,
                         const std::string& value,
                         SignedSettingsHelper::Callback* callback,
                         Delegate* delegate)
      : OpContext(callback, delegate),
        name_(name),
        value_(value) {
  }

  // chromeos::SignedSettings::Delegate implementation
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     bool unused) {
    VLOG(2) << "OnSettingsOpCompleted, code = " << code;
    if (callback_)
      callback_->OnStorePropertyCompleted(code, name_, value_);
    OnOpCompleted();
  }

 protected:
  // OpContext implemenetation
  virtual void CreateOp() {
    op_ = SignedSettings::CreateStorePropertyOp(name_, value_, this);
  }

 private:
  std::string name_;
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(StorePropertyOpContext);
};

class RetrievePropertyOpContext
    : public SignedSettings::Delegate<std::string>,
      public OpContext {
 public:
  RetrievePropertyOpContext(const std::string& name,
                            SignedSettingsHelper::Callback* callback,
                            Delegate* delegate)
      : OpContext(callback, delegate),
        name_(name) {
  }

  // chromeos::SignedSettings::Delegate implementation
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     std::string value) {
    if (callback_)
      callback_->OnRetrievePropertyCompleted(code, name_, value);

    OnOpCompleted();
  }

 protected:
  // OpContext implemenetation
  virtual void CreateOp() {
    op_ = SignedSettings::CreateRetrievePropertyOp(name_, this);
  }

 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(RetrievePropertyOpContext);
};

}  // namespace


class SignedSettingsHelperImpl : public SignedSettingsHelper,
                                 public OpContext::Delegate {
 public:
  // SignedSettingsHelper implementation
  virtual void StartCheckWhitelistOp(const std::string& email,
                                     Callback* callback);
  virtual void StartWhitelistOp(const std::string& email,
                                bool add_to_whitelist,
                                Callback* callback);
  virtual void StartStorePropertyOp(const std::string& name,
                                    const std::string& value,
                                    Callback* callback);
  virtual void StartRetrieveProperty(const std::string& name,
                                     Callback* callback);
  virtual void CancelCallback(Callback* callback);

  // OpContext::Delegate implementation
  virtual void OnOpCreated(OpContext* context);
  virtual void OnOpStarted(OpContext* context);
  virtual void OnOpCompleted(OpContext* context);

 private:
  SignedSettingsHelperImpl();
  ~SignedSettingsHelperImpl();

  void AddOpContext(OpContext* context);
  void ClearAll();

  std::vector<OpContext*> pending_contexts_;

  friend struct base::DefaultLazyInstanceTraits<SignedSettingsHelperImpl>;
  DISALLOW_COPY_AND_ASSIGN(SignedSettingsHelperImpl);
};

static base::LazyInstance<SignedSettingsHelperImpl>
    g_signed_settings_helper_impl(base::LINKER_INITIALIZED);

SignedSettingsHelperImpl::SignedSettingsHelperImpl() {
}

SignedSettingsHelperImpl::~SignedSettingsHelperImpl() {
  if (!pending_contexts_.empty()) {
    LOG(WARNING) << "SignedSettingsHelperImpl shutdown with pending ops, "
                 << "changes will be lost.";
    ClearAll();
  }
}

void SignedSettingsHelperImpl::StartCheckWhitelistOp(
    const std::string&email,
    SignedSettingsHelper::Callback* callback) {
  AddOpContext(new WhitelistOpContext(
      WhitelistOpContext::CHECK,
      email,
      callback,
      this));
}

void SignedSettingsHelperImpl::StartWhitelistOp(
    const std::string&email,
    bool add_to_whitelist,
    SignedSettingsHelper::Callback* callback) {
  AddOpContext(new WhitelistOpContext(
      add_to_whitelist ? WhitelistOpContext::ADD : WhitelistOpContext::REMOVE,
      email,
      callback,
      this));
}

void SignedSettingsHelperImpl::StartStorePropertyOp(
    const std::string& name,
    const std::string& value,
    SignedSettingsHelper::SignedSettingsHelper::Callback* callback) {
  AddOpContext(new StorePropertyOpContext(
      name,
      value,
      callback,
      this));
}

void SignedSettingsHelperImpl::StartRetrieveProperty(
    const std::string& name,
    SignedSettingsHelper::Callback* callback) {
  AddOpContext(new RetrievePropertyOpContext(
      name,
      callback,
      this));
}

void SignedSettingsHelperImpl::CancelCallback(
    SignedSettingsHelper::Callback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < pending_contexts_.size(); ++i) {
    if (pending_contexts_[i]->callback() == callback) {
      pending_contexts_[i]->CancelCallback();
    }
  }
}

void SignedSettingsHelperImpl::AddOpContext(OpContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(context);

  pending_contexts_.push_back(context);
  if (pending_contexts_.size() == 1)
    context->Execute();
}

void SignedSettingsHelperImpl::ClearAll() {
  for (size_t i = 0; i < pending_contexts_.size(); ++i) {
    pending_contexts_[i]->set_delegate(NULL);
    pending_contexts_[i]->Cancel();
  }
  pending_contexts_.clear();
}

void SignedSettingsHelperImpl::OnOpCreated(OpContext* context) {
  if (test_delegate_)
    test_delegate_->OnOpCreated(context->op());
}

void SignedSettingsHelperImpl::OnOpStarted(OpContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (test_delegate_)
    test_delegate_->OnOpStarted(context->op());
}

void SignedSettingsHelperImpl::OnOpCompleted(OpContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(pending_contexts_.front() == context);

  pending_contexts_.erase(pending_contexts_.begin());
  if (!pending_contexts_.empty())
    pending_contexts_.front()->Execute();

  if (test_delegate_)
    test_delegate_->OnOpCompleted(context->op());
}

SignedSettingsHelper* SignedSettingsHelper::Get() {
  return g_signed_settings_helper_impl.Pointer();
}

}  // namespace chromeos
