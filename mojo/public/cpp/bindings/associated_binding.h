// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/lib/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"

namespace mojo {

// Represents the implementation side of an associated interface. It is similar
// to Binding, except that it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedBinding {
 public:
  // Constructs an incomplete associated binding that will use the
  // implementation |impl|. It may be completed with a subsequent call to the
  // |Bind| method. Does not take ownership of |impl|, which must outlive this
  // object.
  explicit AssociatedBinding(Interface* impl) : impl_(impl) {
    stub_.set_sink(impl_);
  }

  // Constructs a completed associated binding of |impl|. The output |ptr_info|
  // should be passed through the message pipe endpoint referred to by
  // |associated_group| to setup the corresponding asssociated interface
  // pointer. |impl| must outlive this object.
  AssociatedBinding(Interface* impl,
                    AssociatedInterfacePtrInfo<Interface>* ptr_info,
                    AssociatedGroup* associated_group)
      : AssociatedBinding(impl) {
    Bind(ptr_info, associated_group);
  }

  // Constructs a completed associated binding of |impl|. |impl| must outlive
  // the binding.
  AssociatedBinding(Interface* impl,
                    AssociatedInterfaceRequest<Interface> request)
      : AssociatedBinding(impl) {
    Bind(std::move(request));
  }

  ~AssociatedBinding() {}

  // Creates an associated inteface and sets up this object as the
  // implementation side. The output |ptr_info| should be passed through the
  // message pipe endpoint referred to by |associated_group| to setup the
  // corresponding asssociated interface pointer.
  void Bind(AssociatedInterfacePtrInfo<Interface>* ptr_info,
            AssociatedGroup* associated_group) {
    AssociatedInterfaceRequest<Interface> request;
    associated_group->CreateAssociatedInterface(AssociatedGroup::WILL_PASS_PTR,
                                                ptr_info, &request);
    Bind(std::move(request));
  }

  // Sets up this object as the implementation side of an associated interface.
  void Bind(AssociatedInterfaceRequest<Interface> request) {
    internal::ScopedInterfaceEndpointHandle handle =
        internal::AssociatedInterfaceRequestHelper::PassHandle(&request);

    DCHECK(handle.is_local())
        << "The AssociatedInterfaceRequest is supposed to be used at the "
        << "other side of the message pipe.";

    if (!handle.is_valid() || !handle.is_local()) {
      endpoint_client_.reset();
      return;
    }

    endpoint_client_.reset(new internal::InterfaceEndpointClient(
        std::move(handle), &stub_,
        base::WrapUnique(new typename Interface::RequestValidator_()),
        Interface::HasSyncMethods_));
    endpoint_client_->set_connection_error_handler(
        [this]() { connection_error_handler_.Run(); });

    stub_.serialization_context()->router = endpoint_client_->router();
  }

  // Closes the associated interface. Puts this object into a state where it can
  // be rebound.
  void Close() {
    DCHECK(endpoint_client_);
    endpoint_client_.reset();
    connection_error_handler_.reset();
  }

  // Unbinds and returns the associated interface request so it can be
  // used in another context, such as on another thread or with a different
  // implementation. Puts this object into a state where it can be rebound.
  AssociatedInterfaceRequest<Interface> Unbind() {
    DCHECK(endpoint_client_);

    AssociatedInterfaceRequest<Interface> request;
    internal::AssociatedInterfaceRequestHelper::SetHandle(
        &request, endpoint_client_->PassHandle());

    endpoint_client_.reset();
    connection_error_handler_.reset();

    return request;
  }

  // Sets an error handler that will be called if a connection error occurs.
  //
  // This method may only be called after this AssociatedBinding has been bound
  // to a message pipe. The error handler will be reset when this
  // AssociatedBinding is unbound or closed.
  void set_connection_error_handler(const Closure& error_handler) {
    DCHECK(is_bound());
    connection_error_handler_ = error_handler;
  }

  // Returns the interface implementation that was previously specified.
  Interface* impl() { return impl_; }

  // Indicates whether the associated binding has been completed.
  bool is_bound() const { return !!endpoint_client_; }

  // Returns the associated group that this object belongs to. Returns null if
  // the object is not bound.
  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

 private:
  std::unique_ptr<internal::InterfaceEndpointClient> endpoint_client_;

  typename Interface::Stub_ stub_;
  Interface* impl_;
  Closure connection_error_handler_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedBinding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
