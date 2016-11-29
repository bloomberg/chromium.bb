// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

struct ThreadSafeInterfacePtrDeleter;

// ThreadSafeInterfacePtr and ThreadSafeAssociatedInterfacePtr are versions of
// InterfacePtr and AssociatedInterfacePtr that let callers invoke
// interface methods from any threads. Callbacks are received on the thread that
// performed the interface call.
//
// To create a ThreadSafeInterfacePtr/ThreadSafeAssociatedInterfacePtr, first
// create a regular InterfacePtr/AssociatedInterfacePtr that
// you then provide to ThreadSafeInterfacePtr/AssociatedInterfacePtr::Create.
// You can then call methods on the
// ThreadSafeInterfacePtr/AssociatedInterfacePtr instance from any thread.
//
// Ex for ThreadSafeInterfacePtr:
// frob::FrobinatorPtr frobinator;
// frob::FrobinatorImpl impl(GetProxy(&frobinator));
// scoped_refptr<frob::ThreadSafeFrobinatorPtr> thread_safe_frobinator =
//     frob::ThreadSafeFrobinatorPtr::Create(std::move(frobinator));
// (*thread_safe_frobinator)->FrobinateToTheMax();
//
// An alternate way is to create the ThreadSafeInterfacePtr unbound (not
// associated with an InterfacePtr) and call Bind() at a later time when the
// InterfacePtr becomes available. Note that you shouldn't call any interface
// methods on the ThreadSafeInterfacePtr before it is bound.

template <typename Interface, template <typename> class InterfacePtrType>
class ThreadSafeInterfacePtrBase
    : public MessageReceiverWithResponder,
      public base::RefCountedThreadSafe<
          ThreadSafeInterfacePtrBase<Interface, InterfacePtrType>,
          ThreadSafeInterfacePtrDeleter> {
 public:
  using ProxyType = typename Interface::Proxy_;

  static scoped_refptr<ThreadSafeInterfacePtrBase<Interface, InterfacePtrType>>
  Create(InterfacePtrType<Interface> interface_ptr) {
    scoped_refptr<ThreadSafeInterfacePtrBase> ptr(
        new ThreadSafeInterfacePtrBase());
    return ptr->Bind(std::move(interface_ptr)) ? ptr : nullptr;
  }

  // Creates a ThreadSafeInterfacePtrBase with no associated InterfacePtr.
  // Call Bind() with the InterfacePtr once available.
  static scoped_refptr<ThreadSafeInterfacePtrBase<Interface, InterfacePtrType>>
  CreateUnbound() {
    return new ThreadSafeInterfacePtrBase();
  }

  // Binds a ThreadSafeInterfacePtrBase previously created with CreateUnbound().
  // This must be called on the thread that |interface_ptr| should be used.
  bool Bind(InterfacePtrType<Interface> interface_ptr) {
    DCHECK(!interface_ptr_task_runner_);
    if (!interface_ptr.is_bound()) {
      LOG(ERROR) << "Attempting to bind a ThreadSafe[Associated]InterfacePtr "
                    "from an unbound InterfacePtr.";
      return false;
    }
    interface_ptr_ = std::move(interface_ptr);
    interface_ptr_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    return true;
  }

  ~ThreadSafeInterfacePtrBase() override {}

  Interface* get() { return &proxy_; }
  Interface* operator->() { return get(); }
  Interface& operator*() { return *get(); }

 private:
  friend class base::RefCountedThreadSafe<
      ThreadSafeInterfacePtrBase<Interface, InterfacePtrType>>;
  friend struct ThreadSafeInterfacePtrDeleter;

  ThreadSafeInterfacePtrBase() : proxy_(this), weak_ptr_factory_(this) {}

  void DeleteOnCorrectThread() const {
    if (interface_ptr_task_runner_ &&
        !interface_ptr_task_runner_->BelongsToCurrentThread() &&
        interface_ptr_task_runner_->DeleteSoon(FROM_HERE, this)) {
      return;
    }
    delete this;
  }

  // MessageReceiverWithResponder implementation:
  bool Accept(Message* message) override {
    interface_ptr_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ThreadSafeInterfacePtrBase::AcceptOnInterfacePtrThread,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(std::move(*message))));
    return true;
  }

  bool AcceptWithResponder(Message* message,
                           MessageReceiver* responder) override {
    auto forward_responder = base::MakeUnique<ForwardToCallingThread>(
        base::WrapUnique(responder));
    interface_ptr_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ThreadSafeInterfacePtrBase::
                                  AcceptWithResponderOnInterfacePtrThread,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Passed(std::move(*message)),
                              base::Passed(std::move(forward_responder))));
    return true;
  }

  void AcceptOnInterfacePtrThread(Message message) {
    interface_ptr_.internal_state()->ForwardMessage(std::move(message));
  }
  void AcceptWithResponderOnInterfacePtrThread(
      Message message,
      std::unique_ptr<MessageReceiver> responder) {
    interface_ptr_.internal_state()->ForwardMessageWithResponder(
        std::move(message), std::move(responder));
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
  InterfacePtrType<Interface> interface_ptr_;
  base::WeakPtrFactory<ThreadSafeInterfacePtrBase> weak_ptr_factory_;
};

struct ThreadSafeInterfacePtrDeleter {
  template <typename Interface, template <typename> class InterfacePtrType>
  static void Destruct(
      const ThreadSafeInterfacePtrBase<Interface, InterfacePtrType>*
          interface_ptr) {
    interface_ptr->DeleteOnCorrectThread();
  }
};

template <typename Interface>
using ThreadSafeAssociatedInterfacePtr =
    ThreadSafeInterfacePtrBase<Interface, AssociatedInterfacePtr>;

template <typename Interface>
using ThreadSafeInterfacePtr =
    ThreadSafeInterfacePtrBase<Interface, InterfacePtr>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
