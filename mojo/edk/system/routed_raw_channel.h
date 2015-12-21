// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_
#define MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {
class RawChannel;

// This class wraps a RawChannel and adds routing on top of it.
// Any RawChannel::Delegate can call here, indirectly through the Broker
// interface, to get notified about messages with its route_id.
class RoutedRawChannel : public RawChannel::Delegate {
 public:
  RoutedRawChannel(
      ScopedPlatformHandle handle,
      const base::Callback<void(RoutedRawChannel*)>& destruct_callback);

  // These methods should all be called on the IO thread only.

  // Connect the given |delegate| with the |route_id|.
  void AddRoute(uint64_t route_id, RawChannel::Delegate* delegate);

  // Called when the route listener is going away.
  void RemoveRoute(uint64_t route_id);

  RawChannel* channel() { return channel_; }

 private:
  friend class base::DeleteHelper<RoutedRawChannel>;
  ~RoutedRawChannel() override;

  // RawChannel::Delegate implementation:
  void OnReadMessage(
        const MessageInTransit::View& message_view,
        ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  RawChannel* channel_;

  base::hash_map<uint64_t, RawChannel::Delegate*> routes_;

  // If we got messages before the route was added (due to race conditions
  // between different channels), this is used to buffer them.
  struct PendingMessage {
    PendingMessage();
    ~PendingMessage();
    std::vector<char> message;
    ScopedPlatformHandleVectorPtr handles;
  };
  std::vector<scoped_ptr<PendingMessage>> pending_messages_;

  // If we got a ROUTE_CLOSED message for a route before it registered with us,
  // we need to hold on to this information so that we can tell it that the
  // connetion is closed when it does connect.
  base::hash_set<uint64_t> close_routes_;

  base::Callback<void(RoutedRawChannel*)> destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(RoutedRawChannel);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_ROUTED_RAW_CHANNEL_H_
