// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_

namespace mojo {

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_state.h"
#include "mojo/public/cpp/bindings/message.h"

struct ThreadSafeInterfacePtrDeleter;

// ThreadSafeInterfacePtr is a version of InterfacePtr that lets caller invoke
// interface methods from any threads. Callbacks are called on the thread that
// performed the interface call.
// To create a ThreadSafeInterfacePtr, create first a regular InterfacePtr that
// you then provide to ThreadSafeInterfacePtr::Create.
// You can then call methods on the ThreadSafeInterfacePtr from any thread.
//
// Ex:
// frob::FrobinatorPtr frobinator;
// frob::FrobinatorImpl impl(GetProxy(&frobinator));
// scoped_refptr<frob::ThreadSafeFrobinatorPtr> thread_safe_frobinator =
//     frob::ThreadSafeFrobinatorPtr::Create(std::move(frobinator));
// (*thread_safe_frobinator)->FrobinateToTheMax();

template <typename Interface>
class ThreadSafeInterfacePtr : public MessageReceiverWithResponder,
    public base::RefCountedThreadSafe<ThreadSafeInterfacePtr<Interface>,
                                      ThreadSafeInterfacePtrDeleter> {
 public:
  using ProxyType = typename Interface::Proxy_;

  using AcceptCallback = base::Callback<void(Message)>;
  using AcceptWithResponderCallback =
      base::Callback<void(Message, std::unique_ptr<MessageReceiver>)>;

  Interface* get() { return &proxy_; }
  Interface* operator->() { return get(); }
  Interface& operator*() { return *get(); }

  static scoped_refptr<ThreadSafeInterfacePtr<Interface>> Create(
      InterfacePtr<Interface> interface_ptr) {
    if (!interface_ptr.is_bound()) {
      LOG(ERROR) << "Attempting to create a ThreadSafeInterfacePtr from an "
          "unbound InterfacePtr.";
      return nullptr;
    }
    return new ThreadSafeInterfacePtr(std::move(interface_ptr),
                                      base::ThreadTaskRunnerHandle::Get());
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeInterfacePtr<Interface>>;
  friend struct ThreadSafeInterfacePtrDeleter;

  ThreadSafeInterfacePtr(
      InterfacePtr<Interface> interface_ptr,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : interface_ptr_task_runner_(task_runner),
        proxy_(this),
        interface_ptr_(std::move(interface_ptr)) {
    // Note that it's important we do get the callback after interface_ptr_ has
    // been set, as they would become invalid if interface_ptr_ is copied.
    accept_callback_ = interface_ptr_.internal_state()->
        GetThreadSafePtrAcceptCallback();
    accept_with_responder_callback_ = interface_ptr_.internal_state()->
        GetThreadSafePtrAcceptWithResponderCallback();
  }

  void DeleteOnCorrectThread() const {
    if (!interface_ptr_task_runner_->BelongsToCurrentThread() &&
        interface_ptr_task_runner_->DeleteSoon(FROM_HERE, this)) {
      return;
    }
    delete this;
  }

  // MessageReceiverWithResponder implementation:
  bool Accept(Message* message) override {
    interface_ptr_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(accept_callback_, base::Passed(std::move(*message))));
    return true;
  }

  bool AcceptWithResponder(Message* message,
                           MessageReceiver* responder) override {
    auto forward_responder = base::MakeUnique<ForwardToCallingThread>(
        base::WrapUnique(responder));
    interface_ptr_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(accept_with_responder_callback_,
                   base::Passed(std::move(*message)),
                   base::Passed(std::move(forward_responder))));
    return true;
  }

  class ForwardToCallingThread : public MessageReceiver {
   public:
    explicit ForwardToCallingThread(std::unique_ptr<MessageReceiver> responder)
        : responder_(std::move(responder)),
          caller_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    }

   private:
    bool Accept(Message* message) {
      // The current instance will be deleted when this method returns, so we
      // have to relinquish the responder's ownership so it does not get
      // deleted.
      caller_task_runner_->PostTask(FROM_HERE,
          base::Bind(&ForwardToCallingThread::CallAcceptAndDeleteResponder,
                     base::Passed(std::move(responder_)),
                     base::Passed(std::move(*message))));
      return true;
    }

    static void CallAcceptAndDeleteResponder(
        std::unique_ptr<MessageReceiver> responder,
        Message message) {
      ignore_result(responder->Accept(&message));
    }

    std::unique_ptr<MessageReceiver> responder_;
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  };

  scoped_refptr<base::SingleThreadTaskRunner> interface_ptr_task_runner_;
  ProxyType proxy_;
  AcceptCallback accept_callback_;
  AcceptWithResponderCallback accept_with_responder_callback_;
  InterfacePtr<Interface> interface_ptr_;
};

struct ThreadSafeInterfacePtrDeleter {
  template <typename Interface>
  static void Destruct(const ThreadSafeInterfacePtr<Interface>* interface_ptr) {
    interface_ptr->DeleteOnCorrectThread();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
