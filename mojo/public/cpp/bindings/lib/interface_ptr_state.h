// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <algorithm>  // For |std::swap()|.
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/filter_chain.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfacePtrState {
 public:
  InterfacePtrState() : version_(0u) {}

  ~InterfacePtrState() {
    endpoint_client_.reset();
    proxy_.reset();
    if (router_)
      router_->CloseMessagePipe();
  }

  Interface* instance() {
    ConfigureProxyIfNecessary();

    // This will be null if the object is not bound.
    return proxy_.get();
  }

  uint32_t version() const { return version_; }

  void QueryVersion(const base::Callback<void(uint32_t)>& callback) {
    ConfigureProxyIfNecessary();

    // It is safe to capture |this| because the callback won't be run after this
    // object goes away.
    endpoint_client_->QueryVersion(base::Bind(
        &InterfacePtrState::OnQueryVersion, base::Unretained(this), callback));
  }

  void RequireVersion(uint32_t version) {
    ConfigureProxyIfNecessary();

    if (version <= version_)
      return;

    version_ = version;
    endpoint_client_->RequireVersion(version);
  }

  void FlushForTesting() {
    ConfigureProxyIfNecessary();
    endpoint_client_->FlushForTesting();
  }

  void CloseWithReason(uint32_t custom_reason, const std::string& description) {
    ConfigureProxyIfNecessary();
    endpoint_client_->CloseWithReason(custom_reason, description);
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->router_, router_);
    swap(other->endpoint_client_, endpoint_client_);
    swap(other->proxy_, proxy_);
    handle_.swap(other->handle_);
    runner_.swap(other->runner_);
    swap(other->version_, version_);
  }

  void Bind(InterfacePtrInfo<Interface> info,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!router_);
    DCHECK(!endpoint_client_);
    DCHECK(!proxy_);
    DCHECK(!handle_.is_valid());
    DCHECK_EQ(0u, version_);
    DCHECK(info.is_valid());

    handle_ = info.PassHandle();
    version_ = info.version();
    runner_ = std::move(runner);
  }

  bool HasAssociatedInterfaces() const {
    return router_ ? router_->HasAssociatedEndpoints() : false;
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  InterfacePtrInfo<Interface> PassInterface() {
    endpoint_client_.reset();
    proxy_.reset();
    return InterfacePtrInfo<Interface>(
        router_ ? router_->PassMessagePipe() : std::move(handle_), version_);
  }

  bool is_bound() const { return handle_.is_valid() || endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  AssociatedGroup* associated_group() {
    ConfigureProxyIfNecessary();
    return endpoint_client_->associated_group();
  }

  void EnableTestingMode() {
    ConfigureProxyIfNecessary();
    router_->EnableTestingMode();
  }

  void ForwardMessage(Message message) {
    ConfigureProxyIfNecessary();
    endpoint_client_->Accept(&message);
  }

  void ForwardMessageWithResponder(Message message,
                                   std::unique_ptr<MessageReceiver> responder) {
    ConfigureProxyIfNecessary();
    endpoint_client_->AcceptWithResponder(&message, std::move(responder));
  }

 private:
  using Proxy = typename Interface::Proxy_;

  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      DCHECK(router_);
      DCHECK(endpoint_client_);
      return;
    }
    // The object hasn't been bound.
    if (!handle_.is_valid())
      return;

    MultiplexRouter::Config config =
        Interface::PassesAssociatedKinds_
            ? MultiplexRouter::MULTI_INTERFACE
            : (Interface::HasSyncMethods_
                   ? MultiplexRouter::SINGLE_INTERFACE_WITH_SYNC_METHODS
                   : MultiplexRouter::SINGLE_INTERFACE);
    router_ = new MultiplexRouter(std::move(handle_), config, true, runner_);
    router_->SetMasterInterfaceName(Interface::Name_);
    endpoint_client_.reset(new InterfaceEndpointClient(
        router_->CreateLocalEndpointHandle(kMasterInterfaceId), nullptr,
        base::WrapUnique(new typename Interface::ResponseValidator_()), false,
        std::move(runner_),
        // The version is only queried from the client so the value passed here
        // will not be used.
        0u));
    proxy_.reset(new Proxy(endpoint_client_.get()));
  }

  void OnQueryVersion(const base::Callback<void(uint32_t)>& callback,
                      uint32_t version) {
    version_ = version;
    callback.Run(version);
  }

  scoped_refptr<MultiplexRouter> router_;

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
  std::unique_ptr<Proxy> proxy_;

  // |router_| (as well as other members above) is not initialized until
  // read/write with the message pipe handle is needed. |handle_| is valid
  // between the Bind() call and the initialization of |router_|.
  ScopedMessagePipeHandle handle_;
  scoped_refptr<base::SequencedTaskRunner> runner_;

  uint32_t version_;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
