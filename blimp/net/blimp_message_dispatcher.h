// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_DISPATCHER_H_
#define BLIMP_NET_BLIMP_MESSAGE_DISPATCHER_H_

#include <map>

#include "base/containers/small_map.h"
#include "base/threading/thread_checker.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_receiver.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Multiplexing BlimpMessageReceiver which routes BlimpMessages to other
// receivers based on the messages' feature type.
class BLIMP_NET_EXPORT BlimpMessageDispatcher : public BlimpMessageReceiver {
 public:
  BlimpMessageDispatcher();
  ~BlimpMessageDispatcher() override;

  // Registers a message receiver which will process all messages
  // with of the specified |type|.
  // Only one handler may be added per type.
  //
  // |handler| must be valid when OnBlimpMessage() is called.
  void AddReceiver(BlimpMessage::Type type, BlimpMessageReceiver* handler);

  // BlimpMessageReceiver implementation.
  net::Error OnBlimpMessage(const BlimpMessage& message) override;

 private:
  base::SmallMap<std::map<BlimpMessage::Type, BlimpMessageReceiver*>>
      feature_receiver_map_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageDispatcher);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_DISPATCHER_H_
