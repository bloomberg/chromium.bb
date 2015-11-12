// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_demultiplexer.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace {

std::string BlimpMessageToDebugString(const BlimpMessage& message) {
  return base::StringPrintf("<message type=%d>", message.type());
}

}  // namespace

BlimpMessageDemultiplexer::BlimpMessageDemultiplexer() {}

BlimpMessageDemultiplexer::~BlimpMessageDemultiplexer() {}

void BlimpMessageDemultiplexer::AddProcessor(BlimpMessage::Type type,
                                             BlimpMessageProcessor* receiver) {
  DCHECK(receiver);
  if (feature_receiver_map_.find(type) == feature_receiver_map_.end()) {
    feature_receiver_map_.insert(std::make_pair(type, receiver));
  } else {
    DLOG(FATAL) << "Handler already registered for type=" << type << ".";
  }
}

void BlimpMessageDemultiplexer::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  auto receiver_iter = feature_receiver_map_.find(message->type());
  if (receiver_iter == feature_receiver_map_.end()) {
    DLOG(FATAL) << "No registered receiver for "
                << BlimpMessageToDebugString(*message) << ".";
    if (!callback.is_null()) {
      callback.Run(net::ERR_NOT_IMPLEMENTED);
    }
  }

  receiver_iter->second->ProcessMessage(message.Pass(), callback);
}

}  // namespace blimp
