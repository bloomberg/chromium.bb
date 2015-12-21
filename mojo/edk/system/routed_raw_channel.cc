// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/routed_raw_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/edk/embedder/embedder_internal.h"

namespace mojo {
namespace edk {

namespace {
const uint64_t kInternalRouteId = 0;
}

RoutedRawChannel::PendingMessage::PendingMessage() {
}

RoutedRawChannel::PendingMessage::~PendingMessage() {
}

RoutedRawChannel::RoutedRawChannel(
    ScopedPlatformHandle handle,
    const base::Callback<void(RoutedRawChannel*)>& destruct_callback)
    : channel_(RawChannel::Create(std::move(handle))),
      destruct_callback_(destruct_callback) {
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RawChannel::Init, base::Unretained(channel_), this));
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&RawChannel::EnsureLazyInitialized,
                  base::Unretained(channel_)));
}

void RoutedRawChannel::AddRoute(uint64_t route_id,
                                RawChannel::Delegate* delegate) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  CHECK_NE(route_id, kInternalRouteId) << kInternalRouteId << " is reserved";
  CHECK(routes_.find(route_id) == routes_.end());
  routes_[route_id] = delegate;

  for (size_t i = 0; i < pending_messages_.size();) {
    MessageInTransit::View view(pending_messages_[i]->message.size(),
                                &pending_messages_[i]->message[0]);
    if (view.route_id() == route_id) {
      delegate->OnReadMessage(view, std::move(pending_messages_[i]->handles));
      pending_messages_.erase(pending_messages_.begin() + i);
    } else {
      ++i;
    }
  }

  if (close_routes_.find(route_id) != close_routes_.end())
    delegate->OnError(ERROR_READ_SHUTDOWN);
}

void RoutedRawChannel::RemoveRoute(uint64_t route_id) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  // We don't check that routes_ contains route_id because it's possible for it
  // to not have been added yet (i.e. it's waiting to be added later down the
  // call stack).
  routes_.erase(route_id);

  // Only send a message to the other side to close the route if we hadn't
  // received a close route message. Otherwise they would keep going back and
  // forth.
  if (close_routes_.find(route_id) != close_routes_.end()) {
    close_routes_.erase(route_id);
  } else if (channel_) {
    // Default route id of 0 to reach the other side's RoutedRawChannel.
    char message_data[sizeof(uint64_t)];
    memcpy(&message_data[0], &route_id, sizeof(uint64_t));
    scoped_ptr<MessageInTransit> message(new MessageInTransit(
        MessageInTransit::Type::MESSAGE, arraysize(message_data),
          message_data));
    message->set_route_id(kInternalRouteId);
    channel_->WriteMessage(std::move(message));
  }

  if (!channel_ && routes_.empty()) {
    // PostTask to avoid reentrancy since the broker might be calling us.
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

RoutedRawChannel::~RoutedRawChannel() {
  DCHECK(!channel_);
  destruct_callback_.Run(this);
}

void RoutedRawChannel::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  // Note: normally, when a message arrives here we should find a corresponding
  // entry for the RawChannel::Delegate with the given route_id. However it is
  // possible that they just connected, and due to race conditions one side has
  // connected and sent a message (and even closed) before the other side had a
  // chance to register with this RoutedRawChannel. In that case, we must buffer
  // all messages.
  uint64_t route_id = message_view.route_id();
  if (route_id == kInternalRouteId) {
    if (message_view.num_bytes() !=  sizeof(uint64_t)) {
      NOTREACHED() << "Invalid internal message in RoutedRawChannel." ;
      return;
    }
    uint64_t closed_route = *static_cast<const uint64_t*>(message_view.bytes());
    if (close_routes_.find(closed_route) != close_routes_.end()) {
      NOTREACHED() << "Should only receive one ROUTE_CLOSED per route.";
      return;
    }
    close_routes_.insert(closed_route);
    if (routes_.find(closed_route) == routes_.end())
      return;  // This side hasn't connected yet.

    routes_[closed_route]->OnError(ERROR_READ_SHUTDOWN);
    return;
  }

  if (routes_.find(route_id) != routes_.end()) {
    routes_[route_id]->OnReadMessage(message_view, std::move(platform_handles));
  } else {
    scoped_ptr<PendingMessage> msg(new PendingMessage);
    msg->message.resize(message_view.total_size());
    memcpy(&msg->message[0], message_view.main_buffer(),
           message_view.total_size());
    msg->handles = std::move(platform_handles);
    pending_messages_.push_back(std::move(msg));
  }
}

void RoutedRawChannel::OnError(Error error) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());

  // Note: we must ensure we don't call RawChannel::Shutdown until after we've
  // called OnError on each route's delegate.
  for (auto it = routes_.begin(); it != routes_.end();) {
    // The delegate might call RemoveRoute in their OnError implementation which
    // would invalidate |it|. So increment it first.
    auto cur_it = it++;
    cur_it->second->OnError(error);
  }

  if (routes_.empty()) {
    channel_->Shutdown();
    channel_ = nullptr;
    delete this;
    return;
  }
}

}  // namespace edk
}  // namespace mojo
