// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_

#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/binding_state.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

class MessageReceiver;

// Represents the binding of an interface implementation to a message pipe.
// When the |Binding| object is destroyed, the binding between the message pipe
// and the interface is torn down and the message pipe is closed, leaving the
// interface implementation in an unbound state.
//
// Example:
//
//   #include "foo.mojom.h"
//
//   class FooImpl : public Foo {
//    public:
//     explicit FooImpl(InterfaceRequest<Foo> request)
//         : binding_(this, std::move(request)) {}
//
//     // Foo implementation here.
//
//    private:
//     Binding<Foo> binding_;
//   };
//
//   class MyFooFactory : public InterfaceFactory<Foo> {
//    public:
//     void Create(..., InterfaceRequest<Foo> request) override {
//       auto f = new FooImpl(std::move(request));
//       // Do something to manage the lifetime of |f|. Use StrongBinding<> to
//       // delete FooImpl on connection errors.
//     }
//   };
//
// This class is thread hostile while bound to a message pipe. All calls to this
// class must be from the thread that bound it. The interface implementation's
// methods will be called from the thread that bound this. If a Binding is not
// bound to a message pipe, it may be bound or destroyed on any thread.
//
// When you bind this class to a message pipe, optionally you can specify a
// base::SingleThreadTaskRunner. This task runner must belong to the same
// thread. It will be used to dispatch incoming method calls and connection
// error notification. It is useful when you attach multiple task runners to a
// single thread for the purposes of task scheduling. Please note that incoming
// synchrounous method calls may not be run from this task runner, when they
// reenter outgoing synchrounous calls on the same thread.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class Binding {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an incomplete binding that will use the implementation |impl|.
  // The binding may be completed with a subsequent call to the |Bind| method.
  // Does not take ownership of |impl|, which must outlive the binding.
  explicit Binding(ImplPointerType impl) : internal_state_(std::move(impl)) {}

  // Constructs a completed binding of |impl| to the message pipe endpoint in
  // |request|, taking ownership of the endpoint. Does not take ownership of
  // |impl|, which must outlive the binding.
  Binding(ImplPointerType impl,
          InterfaceRequest<Interface> request,
          scoped_refptr<base::SingleThreadTaskRunner> runner =
              base::ThreadTaskRunnerHandle::Get())
      : Binding(std::move(impl)) {
    Bind(std::move(request), std::move(runner));
  }

  // Tears down the binding, closing the message pipe and leaving the interface
  // implementation unbound.
  ~Binding() {}

  // Returns an InterfacePtr bound to one end of a pipe whose other end is
  // bound to |this|.
  InterfacePtr<Interface> CreateInterfacePtrAndBind(
      scoped_refptr<base::SingleThreadTaskRunner> runner =
          base::ThreadTaskRunnerHandle::Get()) {
    InterfacePtr<Interface> interface_ptr;
    Bind(MakeRequest(&interface_ptr), std::move(runner));
    return interface_ptr;
  }

  // Completes a binding that was constructed with only an interface
  // implementation by removing the message pipe endpoint from |request| and
  // binding it to the previously specified implementation.
  void Bind(InterfaceRequest<Interface> request,
            scoped_refptr<base::SingleThreadTaskRunner> runner =
                base::ThreadTaskRunnerHandle::Get()) {
    internal_state_.Bind(request.PassMessagePipe(), std::move(runner));
  }

  // Adds a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed.
  void AddFilter(std::unique_ptr<MessageReceiver> filter) {
    DCHECK(is_bound());
    internal_state_.AddFilter(std::move(filter));
  }

  // Whether there are any associated interfaces running on the pipe currently.
  bool HasAssociatedInterfaces() const {
    return internal_state_.HasAssociatedInterfaces();
  }

  // Stops processing incoming messages until
  // ResumeIncomingMethodCallProcessing(), or WaitForIncomingMethodCall().
  // Outgoing messages are still sent.
  //
  // No errors are detected on the message pipe while paused.
  //
  // This method may only be called if the object has been bound to a message
  // pipe and there are no associated interfaces running.
  void PauseIncomingMethodCallProcessing() {
    CHECK(!HasAssociatedInterfaces());
    internal_state_.PauseIncomingMethodCallProcessing();
  }
  void ResumeIncomingMethodCallProcessing() {
    internal_state_.ResumeIncomingMethodCallProcessing();
  }

  // Blocks the calling thread until either a call arrives on the previously
  // bound message pipe, the deadline is exceeded, or an error occurs. Returns
  // true if a method was successfully read and dispatched.
  //
  // This method may only be called if the object has been bound to a message
  // pipe. This returns once a message is received either on the master
  // interface or any associated interfaces.
  bool WaitForIncomingMethodCall(
      MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE) {
    return internal_state_.WaitForIncomingMethodCall(deadline);
  }

  // Closes the message pipe that was previously bound. Put this object into a
  // state where it can be rebound to a new pipe.
  void Close() { internal_state_.Close(); }

  // Similar to the method above, but also specifies a disconnect reason.
  void CloseWithReason(uint32_t custom_reason, const std::string& description) {
    internal_state_.CloseWithReason(custom_reason, description);
  }

  // Unbinds the underlying pipe from this binding and returns it so it can be
  // used in another context, such as on another thread or with a different
  // implementation. Put this object into a state where it can be rebound to a
  // new pipe.
  //
  // This method may only be called if the object has been bound to a message
  // pipe and there are no associated interfaces running.
  //
  // TODO(yzshen): For now, users need to make sure there is no one holding
  // on to associated interface endpoint handles at both sides of the
  // message pipe in order to call this method. We need a way to forcefully
  // invalidate associated interface endpoint handles.
  InterfaceRequest<Interface> Unbind() {
    CHECK(!HasAssociatedInterfaces());
    return internal_state_.Unbind();
  }

  // Sets an error handler that will be called if a connection error occurs on
  // the bound message pipe.
  //
  // This method may only be called after this Binding has been bound to a
  // message pipe. The error handler will be reset when this Binding is unbound
  // or closed.
  void set_connection_error_handler(const base::Closure& error_handler) {
    DCHECK(is_bound());
    internal_state_.set_connection_error_handler(error_handler);
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    DCHECK(is_bound());
    internal_state_.set_connection_error_with_reason_handler(error_handler);
  }

  // Returns the interface implementation that was previously specified. Caller
  // does not take ownership.
  Interface* impl() { return internal_state_.impl(); }

  // Indicates whether the binding has been completed (i.e., whether a message
  // pipe has been bound to the implementation).
  bool is_bound() const { return internal_state_.is_bound(); }

  // Returns the value of the handle currently bound to this Binding which can
  // be used to make explicit Wait/WaitMany calls. Requires that the Binding be
  // bound. Ownership of the handle is retained by the Binding, it is not
  // transferred to the caller.
  MessagePipeHandle handle() const { return internal_state_.handle(); }

  // Sends a no-op message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Exposed for testing, should not generally be used.
  void EnableTestingMode() { internal_state_.EnableTestingMode(); }

 private:
  internal::BindingState<Interface, ImplRefTraits> internal_state_;

  DISALLOW_COPY_AND_ASSIGN(Binding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_H_
