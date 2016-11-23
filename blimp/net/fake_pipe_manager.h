// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_FAKE_PIPE_MANAGER_H_
#define BLIMP_NET_FAKE_PIPE_MANAGER_H_

#include <map>

#include "blimp/net/fake_blimp_message_processor.h"
#include "blimp/net/pipe_manager.h"

namespace blimp {

// Stores outgoing messages in memory, and allows incoming messages to be
// faked.
class BLIMP_NET_EXPORT FakePipeManager : public PipeManager {
 public:
  FakePipeManager();
  ~FakePipeManager() override;

  std::unique_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::FeatureCase feature_case,
      BlimpMessageProcessor* incoming_processor) override;

  // Send a fake incoming message as though it arrived from the network.
  void SendIncomingMessage(std::unique_ptr<BlimpMessage>);

  // Access the outgoing message processor for a given feature in order to
  // inspect sent messages.
  FakeBlimpMessageProcessor* GetOutgoingMessageProcessor(
      BlimpMessage::FeatureCase feature_case);

 private:
  std::map<BlimpMessage::FeatureCase, FakeBlimpMessageProcessor*>
      outgoing_processors_;

  std::map<BlimpMessage::FeatureCase, BlimpMessageProcessor*>
      incoming_processors_;

  DISALLOW_COPY_AND_ASSIGN(FakePipeManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_FAKE_PIPE_MANAGER_H_
