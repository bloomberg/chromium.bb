// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/interface_endpoint_client.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

namespace {

// When receiving an incoming message which expects a repsonse,
// InterfaceEndpointClient creates a ResponderThunk object and passes it to the
// incoming message receiver. When the receiver finishes processing the message,
// it can provide a response using this object.
class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(
      const base::WeakPtr<InterfaceEndpointClient>& endpoint_client)
      : endpoint_client_(endpoint_client), accept_was_invoked_(false) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Mojo application handled a message that was expecting a response
      // but did not send a response.
      if (endpoint_client_) {
        // We raise an error to signal the calling application that an error
        // condition occurred. Without this the calling application would have
        // no way of knowing it should stop waiting for a response.
        //
        // We raise the error asynchronously and only if |endpoint_client_| is
        // still alive when the task is run. That way it won't break the case
        // where the user abandons the interface endpoint client soon after
        // he/she abandons the callback.
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&InterfaceEndpointClient::RaiseError, endpoint_client_));
      }
    }
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(kMessageIsResponse));

    bool result = false;

    if (endpoint_client_)
      result = endpoint_client_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsValid() override {
    return endpoint_client_ && !endpoint_client_->encountered_error();
  }

 private:
  base::WeakPtr<InterfaceEndpointClient> endpoint_client_;
  bool accept_was_invoked_;

  DISALLOW_COPY_AND_ASSIGN(ResponderThunk);
};

}  // namespace

// ----------------------------------------------------------------------------

InterfaceEndpointClient::HandleIncomingMessageThunk::HandleIncomingMessageThunk(
    InterfaceEndpointClient* owner)
    : owner_(owner) {}

InterfaceEndpointClient::HandleIncomingMessageThunk::
    ~HandleIncomingMessageThunk() {}

bool InterfaceEndpointClient::HandleIncomingMessageThunk::Accept(
    Message* message) {
  return owner_->HandleValidatedMessage(message);
}

// ----------------------------------------------------------------------------

InterfaceEndpointClient::InterfaceEndpointClient(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    scoped_ptr<MessageFilter> payload_validator)
    : handle_(std::move(handle)),
      incoming_receiver_(receiver),
      payload_validator_(std::move(payload_validator)),
      thunk_(this),
      next_request_id_(1),
      encountered_error_(false),
      weak_ptr_factory_(this) {
  DCHECK(handle_.is_valid());
  DCHECK(handle_.is_local());

  // TODO(yzshen): the way to use validator (or message filter in general)
  // directly is a little awkward.
  payload_validator_->set_sink(&thunk_);

  handle_.router()->AttachEndpointClient(handle_, this);
}

InterfaceEndpointClient::~InterfaceEndpointClient() {
  DCHECK(thread_checker_.CalledOnValidThread());

  STLDeleteValues(&responders_);

  handle_.router()->DetachEndpointClient(handle_);
}

AssociatedGroup* InterfaceEndpointClient::associated_group() {
  if (!associated_group_)
    associated_group_ = handle_.router()->CreateAssociatedGroup();
  return associated_group_.get();
}

ScopedInterfaceEndpointHandle InterfaceEndpointClient::PassHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!has_pending_responders());

  if (!handle_.is_valid())
    return ScopedInterfaceEndpointHandle();

  handle_.router()->DetachEndpointClient(handle_);

  return std::move(handle_);
}

void InterfaceEndpointClient::RaiseError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  handle_.router()->RaiseError();
}

bool InterfaceEndpointClient::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!message->has_flag(kMessageExpectsResponse));

  if (encountered_error_)
    return false;

  return handle_.router()->SendMessage(handle_, message);
}

bool InterfaceEndpointClient::AcceptWithResponder(Message* message,
                                                  MessageReceiver* responder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message->has_flag(kMessageExpectsResponse));

  if (encountered_error_)
    return false;

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  message->set_request_id(request_id);

  if (!handle_.router()->SendMessage(handle_, message))
    return false;

  // We assume ownership of |responder|.
  responders_[request_id] = responder;
  return true;
}

bool InterfaceEndpointClient::HandleIncomingMessage(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return payload_validator_->Accept(message);
}

void InterfaceEndpointClient::NotifyError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (encountered_error_)
    return;
  encountered_error_ = true;
  error_handler_.Run();
}

bool InterfaceEndpointClient::HandleValidatedMessage(Message* message) {
  DCHECK_EQ(handle_.id(), message->interface_id());

  if (message->has_flag(kMessageExpectsResponse)) {
    if (!incoming_receiver_)
      return false;

    MessageReceiverWithStatus* responder =
        new ResponderThunk(weak_ptr_factory_.GetWeakPtr());
    bool ok = incoming_receiver_->AcceptWithResponder(message, responder);
    if (!ok)
      delete responder;
    return ok;
  } else if (message->has_flag(kMessageIsResponse)) {
    uint64_t request_id = message->request_id();
    ResponderMap::iterator it = responders_.find(request_id);
    if (it == responders_.end())
      return false;
    MessageReceiver* responder = it->second;
    responders_.erase(it);
    bool ok = responder->Accept(message);
    delete responder;
    return ok;
  } else {
    if (!incoming_receiver_)
      return false;

    return incoming_receiver_->Accept(message);
  }
}

}  // namespace internal
}  // namespace mojo
