// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_PIPE_MANAGER_H_
#define BLIMP_NET_PIPE_MANAGER_H_

#include "base/macros.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlimpMessageProcessor;

class BLIMP_NET_EXPORT PipeManager {
 public:
  virtual ~PipeManager() {}

  // Registers a message processor |incoming_processor| which will receive all
  // messages of the |feature_case| specified. Returns a BlimpMessageProcessor
  // object for sending messages of the given feature.
  virtual std::unique_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::FeatureCase feature_case,
      BlimpMessageProcessor* incoming_processor) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_PIPE_MANAGER_H_
