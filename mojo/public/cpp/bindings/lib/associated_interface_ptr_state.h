// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <algorithm>  // For |std::swap()|.
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace internal {

template <typename Interface>
class AssociatedInterfacePtrState {
 public:
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

  void QueryVersion(const base::Callback<void(uint32_t)>& callback) {
    // It is safe to capture |this| because the callback won't be run after this
    // object goes away.
    endpoint_client_->QueryVersion(
        base::Bind(&AssociatedInterfacePtrState::OnQueryVersion,
                   base::Unretained(this), callback));
  }

  void RequireVersion(uint32_t version) {
    if (version <= version_)
      return;

    version_ = version;
    endpoint_client_->RequireVersion(version);
  }

  void FlushForTesting() { endpoint_client_->FlushForTesting(); }

  void CloseWithReason(uint32_t custom_reason, const std::string& description) {
    endpoint_client_->CloseWithReason(custom_reason, description);
  }

  void Swap(AssociatedInterfacePtrState* other) {
    using std::swap;
    swap(other->endpoint_client_, endpoint_client_);
    swap(other->proxy_, proxy_);
    swap(other->version_, version_);
  }

  void Bind(AssociatedInterfacePtrInfo<Interface> info,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!endpoint_client_);
    DCHECK(!proxy_);
    DCHECK_EQ(0u, version_);
    DCHECK(info.is_valid());

    version_ = info.version();
    // The version is only queried from the client so the value passed here
    // will not be used.
    endpoint_client_.reset(new InterfaceEndpointClient(
        info.PassHandle(), nullptr,
        base::WrapUnique(new typename Interface::ResponseValidator_()), false,
        std::move(runner), 0u));
    proxy_.reset(new Proxy(endpoint_client_.get()));
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  AssociatedInterfacePtrInfo<Interface> PassInterface() {
    ScopedInterfaceEndpointHandle handle = endpoint_client_->PassHandle();
    endpoint_client_.reset();
    proxy_.reset();
    return AssociatedInterfacePtrInfo<Interface>(std::move(handle), version_);
  }

  bool is_bound() const { return !!endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

  void ForwardMessage(Message message) { endpoint_client_->Accept(&message); }

  void ForwardMessageWithResponder(Message message,
                                   std::unique_ptr<MessageReceiver> responder) {
    endpoint_client_->AcceptWithResponder(&message, std::move(responder));
  }

 private:
  using Proxy = typename Interface::Proxy_;

  void OnQueryVersion(const base::Callback<void(uint32_t)>& callback,
                      uint32_t version) {
    version_ = version;
    callback.Run(version);
  }

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
  std::unique_ptr<Proxy> proxy_;

  uint32_t version_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_
