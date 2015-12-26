// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_

#include <stdint.h>
#include <algorithm>  // For |std::swap()|.
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"
#include "mojo/public/cpp/bindings/lib/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/interface_id.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"

namespace mojo {
namespace internal {

template <typename Interface>
class AssociatedInterfacePtrState {
 public:
  using GenericInterface = typename Interface::GenericInterface;

  AssociatedInterfacePtrState() : version_(0u) {}

  ~AssociatedInterfacePtrState() {
    endpoint_client_.reset();
    proxy_.reset();
  }

  Interface* instance() {
    // This will be null if the object is not bound.
    return proxy_.get();
  }

  uint32_t version() const { return version_; }

  void QueryVersion(const Callback<void(uint32_t)>& callback) {
    // It is safe to capture |this| because the callback won't be run after this
    // object goes away.
    auto callback_wrapper = [this, callback](uint32_t version) {
      this->version_ = version;
      callback.Run(version);
    };

    // Do a static cast in case the interface contains methods with the same
    // name.
    static_cast<ControlMessageProxy*>(proxy_.get())
        ->QueryVersion(callback_wrapper);
  }

  void RequireVersion(uint32_t version) {
    if (version <= version_)
      return;

    version_ = version;
    // Do a static cast in case the interface contains methods with the same
    // name.
    static_cast<ControlMessageProxy*>(proxy_.get())->RequireVersion(version);
  }

  void Swap(AssociatedInterfacePtrState* other) {
    using std::swap;
    swap(other->endpoint_client_, endpoint_client_);
    swap(other->proxy_, proxy_);
    swap(other->version_, version_);
  }

  void Bind(AssociatedInterfacePtrInfo<GenericInterface> info) {
    DCHECK(!endpoint_client_);
    DCHECK(!proxy_);
    DCHECK_EQ(0u, version_);
    DCHECK(info.is_valid());

    version_ = info.version();
    endpoint_client_.reset(new InterfaceEndpointClient(
        AssociatedInterfacePtrInfoHelper::PassHandle(&info), nullptr,
        make_scoped_ptr(new typename Interface::ResponseValidator_())));
    proxy_.reset(new Proxy(endpoint_client_.get()));
    proxy_->serialization_context()->router = endpoint_client_->router();
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  AssociatedInterfacePtrInfo<GenericInterface> PassInterface() {
    ScopedInterfaceEndpointHandle handle = endpoint_client_->PassHandle();
    endpoint_client_.reset();
    proxy_.reset();

    AssociatedInterfacePtrInfo<GenericInterface> result;
    result.set_version(version_);
    AssociatedInterfacePtrInfoHelper::SetHandle(&result, std::move(handle));
    return result.Pass();
  }

  bool is_bound() const { return !!endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  void set_connection_error_handler(const Closure& error_handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(error_handler);
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

 private:
  using Proxy = typename Interface::Proxy_;

  scoped_ptr<InterfaceEndpointClient> endpoint_client_;
  scoped_ptr<Proxy> proxy_;

  uint32_t version_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_
