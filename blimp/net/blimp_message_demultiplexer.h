// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_DEMULTIPLEXER_H_
#define BLIMP_NET_BLIMP_MESSAGE_DEMULTIPLEXER_H_

#include <map>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Multiplexing BlimpMessageProcessor which routes BlimpMessages to message
// processors based on |message.type|.
class BLIMP_NET_EXPORT BlimpMessageDemultiplexer
    : public BlimpMessageProcessor {
 public:
  BlimpMessageDemultiplexer();
  ~BlimpMessageDemultiplexer() override;

  // Registers a message processor which will receive all messages
  // of the |type| specified.
  // Only one handler may be added per type.
  //
  // |handler| must be valid when ProcessMessage() is called.
  void AddProcessor(BlimpMessage::Type type, BlimpMessageProcessor* handler);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  base::SmallMap<std::map<BlimpMessage::Type, BlimpMessageProcessor*>>
      feature_receiver_map_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageDemultiplexer);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_DEMULTIPLEXER_H_
