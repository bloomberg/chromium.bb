// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_helper.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

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

  // Cancels the callback and cancels the op if it is not executing.
  void Cancel() {
    if (!executing_)
      OnOpCompleted();
  }

  // Accessors.
  SignedSettings* op() const {
    return op_.get();
  }

  void set_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 protected:
  explicit OpContext(Delegate* delegate)
      : executing_(false),
        delegate_(delegate) {
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
};

class StorePolicyOpContext
    : public SignedSettings::Delegate<bool>,
      public OpContext {
 public:
  StorePolicyOpContext(const em::PolicyFetchResponse& policy,
                       SignedSettingsHelper::StorePolicyCallback callback,
                       OpContext::Delegate* delegate)
      : OpContext(delegate),
        callback_(callback),
        policy_(policy) {
  }

  // chromeos::SignedSettings::Delegate implementation
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     bool unused) OVERRIDE {
    VLOG(2) << "OnSettingsOpCompleted, code = " << code;
    callback_.Run(code);
    OnOpCompleted();
  }

 protected:
  // OpContext implementation
  virtual void CreateOp() OVERRIDE {
    op_ = SignedSettings::CreateStorePolicyOp(&policy_, this);
  }

 private:
  SignedSettingsHelper::StorePolicyCallback callback_;
  em::PolicyFetchResponse policy_;

  DISALLOW_COPY_AND_ASSIGN(StorePolicyOpContext);
};

class RetrievePolicyOpContext
    : public SignedSettings::Delegate<const em::PolicyFetchResponse&>,
      public OpContext {
 public:
  RetrievePolicyOpContext(SignedSettingsHelper::RetrievePolicyCallback callback,
                          OpContext::Delegate* delegate)
      : OpContext(delegate),
        callback_(callback){
  }

  // chromeos::SignedSettings::Delegate implementation
  virtual void OnSettingsOpCompleted(
      SignedSettings::ReturnCode code,
      const em::PolicyFetchResponse& policy) OVERRIDE {
    callback_.Run(code, policy);
    OnOpCompleted();
  }

 protected:
  // OpContext implementation
  virtual void CreateOp() OVERRIDE {
    op_ = SignedSettings::CreateRetrievePolicyOp(this);
  }

 private:
  SignedSettingsHelper::RetrievePolicyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(RetrievePolicyOpContext);
};

}  // namespace


class SignedSettingsHelperImpl : public SignedSettingsHelper,
                                 public OpContext::Delegate {
 public:
  // SignedSettingsHelper implementation
  virtual void StartStorePolicyOp(const em::PolicyFetchResponse& policy,
                                  StorePolicyCallback callback) OVERRIDE;
  virtual void StartRetrievePolicyOp(RetrievePolicyCallback callback) OVERRIDE;

  // OpContext::Delegate implementation
  virtual void OnOpCreated(OpContext* context);
  virtual void OnOpStarted(OpContext* context);
  virtual void OnOpCompleted(OpContext* context);

 private:
  SignedSettingsHelperImpl();
  virtual ~SignedSettingsHelperImpl();

  void AddOpContext(OpContext* context);
  void ClearAll();

  std::vector<OpContext*> pending_contexts_;

  friend struct base::DefaultLazyInstanceTraits<SignedSettingsHelperImpl>;
  DISALLOW_COPY_AND_ASSIGN(SignedSettingsHelperImpl);
};

static base::LazyInstance<SignedSettingsHelperImpl>
    g_signed_settings_helper_impl = LAZY_INSTANCE_INITIALIZER;

SignedSettingsHelperImpl::SignedSettingsHelperImpl() {
}

SignedSettingsHelperImpl::~SignedSettingsHelperImpl() {
  if (!pending_contexts_.empty()) {
    LOG(WARNING) << "SignedSettingsHelperImpl shutdown with pending ops, "
                 << "changes will be lost.";
    ClearAll();
  }
}

void SignedSettingsHelperImpl::StartStorePolicyOp(
    const em::PolicyFetchResponse& policy,
    StorePolicyCallback callback) {
  AddOpContext(new StorePolicyOpContext(policy, callback, this));
}

void SignedSettingsHelperImpl::StartRetrievePolicyOp(
    RetrievePolicyCallback callback) {
  AddOpContext(new RetrievePolicyOpContext(callback, this));
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
