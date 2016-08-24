// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/binding_state.h"

namespace mojo {
namespace internal {

SimpleBindingState::SimpleBindingState() = default;

SimpleBindingState::~SimpleBindingState() = default;

void SimpleBindingState::AddFilter(std::unique_ptr<MessageReceiver> filter) {
  DCHECK(router_);
  router_->AddFilter(std::move(filter));
}

void SimpleBindingState::PauseIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->PauseIncomingMethodCallProcessing();
}
void SimpleBindingState::ResumeIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->ResumeIncomingMethodCallProcessing();
}

bool SimpleBindingState::WaitForIncomingMethodCall(MojoDeadline deadline) {
  DCHECK(router_);
  return router_->WaitForIncomingMessage(deadline);
}

void SimpleBindingState::Close() {
  if (!router_)
    return;

  router_->CloseMessagePipe();
  DestroyRouter();
}

void SimpleBindingState::EnableTestingMode() {
  DCHECK(is_bound());
  router_->EnableTestingMode();
}

void SimpleBindingState::BindInternal(
    ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const char* interface_name,
    std::unique_ptr<MessageReceiver> request_validator,
    bool has_sync_methods,
    MessageReceiverWithResponderStatus* stub) {
  FilterChain filters;
  filters.Append<MessageHeaderValidator>(interface_name);
  filters.Append(std::move(request_validator));

  router_ = new internal::Router(std::move(handle), std::move(filters),
                                 has_sync_methods, std::move(runner));
  router_->set_incoming_receiver(stub);
  router_->set_connection_error_handler(base::Bind(
      &SimpleBindingState::RunConnectionErrorHandler, base::Unretained(this)));
}

void SimpleBindingState::DestroyRouter() {
  router_->set_connection_error_handler(base::Closure());
  delete router_;
  router_ = nullptr;
  connection_error_handler_.Reset();
}

void SimpleBindingState::RunConnectionErrorHandler() {
  if (!connection_error_handler_.is_null())
    connection_error_handler_.Run();
}

// -----------------------------------------------------------------------------

MultiplexedBindingState::MultiplexedBindingState() = default;

MultiplexedBindingState::~MultiplexedBindingState() = default;

bool MultiplexedBindingState::HasAssociatedInterfaces() const {
  return router_ ? router_->HasAssociatedEndpoints() : false;
}

void MultiplexedBindingState::PauseIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->PauseIncomingMethodCallProcessing();
}
void MultiplexedBindingState::ResumeIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->ResumeIncomingMethodCallProcessing();
}

bool MultiplexedBindingState::WaitForIncomingMethodCall(MojoDeadline deadline) {
  DCHECK(router_);
  return router_->WaitForIncomingMessage(deadline);
}

void MultiplexedBindingState::Close() {
  if (!router_)
    return;

  endpoint_client_.reset();
  router_->CloseMessagePipe();
  router_ = nullptr;
  connection_error_handler_.Reset();
}

void MultiplexedBindingState::EnableTestingMode() {
  DCHECK(is_bound());
  router_->EnableTestingMode();
}

void MultiplexedBindingState::BindInternal(
    ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const char* interface_name,
    std::unique_ptr<MessageReceiver> request_validator,
    bool has_sync_methods,
    MessageReceiverWithResponderStatus* stub) {
  DCHECK(!router_);

  router_ = new internal::MultiplexRouter(false, std::move(handle), runner);
  router_->SetMasterInterfaceName(interface_name);

  endpoint_client_.reset(new InterfaceEndpointClient(
      router_->CreateLocalEndpointHandle(kMasterInterfaceId), stub,
      std::move(request_validator), has_sync_methods, std::move(runner)));

  endpoint_client_->set_connection_error_handler(
      base::Bind(&MultiplexedBindingState::RunConnectionErrorHandler,
                 base::Unretained(this)));
}

void MultiplexedBindingState::RunConnectionErrorHandler() {
  if (!connection_error_handler_.is_null())
    connection_error_handler_.Run();
}

}  // namesapce internal
}  // namespace mojo
