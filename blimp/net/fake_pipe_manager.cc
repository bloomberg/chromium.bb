// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/fake_pipe_manager.h"

#include "base/memory/ptr_util.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {

FakePipeManager::FakePipeManager() = default;

FakePipeManager::~FakePipeManager() = default;

std::unique_ptr<BlimpMessageProcessor> FakePipeManager::RegisterFeature(
      BlimpMessage::FeatureCase feature_case,
      BlimpMessageProcessor* incoming_processor) {
  incoming_processors_.insert(
      std::pair<BlimpMessage::FeatureCase, BlimpMessageProcessor*>(
          feature_case, incoming_processor));

  std::unique_ptr<FakeBlimpMessageProcessor> outgoing_processor =
      base::MakeUnique<FakeBlimpMessageProcessor>();

  outgoing_processors_.insert(
      std::pair<BlimpMessage::FeatureCase, FakeBlimpMessageProcessor*>(
          feature_case, outgoing_processor.get()));

  return std::move(outgoing_processor);
}

FakeBlimpMessageProcessor* FakePipeManager::GetOutgoingMessageProcessor(
    BlimpMessage::FeatureCase feature_case) {
  return outgoing_processors_.at(feature_case);
}

}  // namespace blimp
