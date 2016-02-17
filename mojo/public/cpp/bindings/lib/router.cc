// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/router.h"

#include <stdint.h>
#include <utility>

#include "base/logging.h"

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

namespace {

class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(const base::WeakPtr<Router>& router)
      : router_(router), accept_was_invoked_(false) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Mojo application handled a message that was expecting a response
      // but did not send a response.
      if (router_) {
        // We raise an error to signal the calling application that an error
        // condition occurred. Without this the calling application would have
        // no way of knowing it should stop waiting for a response.
        router_->RaiseError();
      }
    }
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(kMessageIsResponse));

    bool result = false;

    if (router_)
      result = router_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsValid() override {
    return router_ && !router_->encountered_error() && router_->is_valid();
  }

 private:
  base::WeakPtr<Router> router_;
  bool accept_was_invoked_;
};

}  // namespace

// ----------------------------------------------------------------------------

Router::HandleIncomingMessageThunk::HandleIncomingMessageThunk(Router* router)
    : router_(router) {
}

Router::HandleIncomingMessageThunk::~HandleIncomingMessageThunk() {
}

bool Router::HandleIncomingMessageThunk::Accept(Message* message) {
  return router_->HandleIncomingMessage(message);
}

// ----------------------------------------------------------------------------

Router::Router(ScopedMessagePipeHandle message_pipe,
               FilterChain filters,
               const MojoAsyncWaiter* waiter)
    : thunk_(this),
      filters_(std::move(filters)),
      connector_(std::move(message_pipe),
                 Connector::SINGLE_THREADED_SEND,
                 waiter),
      incoming_receiver_(nullptr),
      next_request_id_(0),
      testing_mode_(false),
      weak_factory_(this) {
  filters_.SetSink(&thunk_);
  connector_.set_incoming_receiver(filters_.GetHead());
}

Router::~Router() {
  weak_factory_.InvalidateWeakPtrs();

  for (auto& pair : async_responders_)
    delete pair.second;
  for (auto& pair : sync_responders_)
    delete pair.second;
}

bool Router::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!message->has_flag(kMessageExpectsResponse));
  return connector_.Accept(message);
}

bool Router::AcceptWithResponder(Message* message, MessageReceiver* responder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message->has_flag(kMessageExpectsResponse));

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  message->set_request_id(request_id);
  if (!connector_.Accept(message))
    return false;

  if (!message->has_flag(kMessageIsSync)) {
    // We assume ownership of |responder|.
    async_responders_[request_id] = responder;
    return true;
  }

  sync_responders_[request_id] = responder;
  base::WeakPtr<Router> weak_self = weak_factory_.GetWeakPtr();
  for (;;) {
    // TODO(yzshen): Here we should allow incoming sync requests to re-enter and
    // block async messages.
    bool result = WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
    // The message pipe has disconnected.
    if (!result)
      break;

    // This instance has been destroyed.
    if (!weak_self)
      break;

    // The corresponding response message has arrived.
    if (sync_responders_.find(request_id) == sync_responders_.end())
      break;
  }

  // Return true means that we take ownership of |responder|.
  return true;
}

void Router::EnableTestingMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool Router::HandleIncomingMessage(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (message->has_flag(kMessageExpectsResponse)) {
    if (!incoming_receiver_)
      return false;

    MessageReceiverWithStatus* responder =
        new ResponderThunk(weak_factory_.GetWeakPtr());
    bool ok = incoming_receiver_->AcceptWithResponder(message, responder);
    if (!ok)
      delete responder;
    return ok;

  } else if (message->has_flag(kMessageIsResponse)) {
    ResponderMap& responder_map = message->has_flag(kMessageIsSync)
                                      ? sync_responders_
                                      : async_responders_;
    uint64_t request_id = message->request_id();
    ResponderMap::iterator it = responder_map.find(request_id);
    if (it == responder_map.end()) {
      DCHECK(testing_mode_);
      return false;
    }
    MessageReceiver* responder = it->second;
    responder_map.erase(it);
    bool ok = responder->Accept(message);
    delete responder;
    return ok;
  } else {
    if (!incoming_receiver_)
      return false;

    return incoming_receiver_->Accept(message);
  }
}

// ----------------------------------------------------------------------------

}  // namespace internal
}  // namespace mojo
