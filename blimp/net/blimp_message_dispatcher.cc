// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_dispatcher.h"

#include <string>

#include "base/strings/stringprintf.h"

namespace blimp {
namespace {

std::string BlimpMessageToDebugString(const BlimpMessage& message) {
  return base::StringPrintf("<message type=%d>", message.type());
}

}  // namespace

BlimpMessageDispatcher::BlimpMessageDispatcher() {}

BlimpMessageDispatcher::~BlimpMessageDispatcher() {}

void BlimpMessageDispatcher::AddReceiver(BlimpMessage::Type type,
                                         BlimpMessageReceiver* receiver) {
  DCHECK(receiver);
  if (feature_receiver_map_.find(type) == feature_receiver_map_.end()) {
    feature_receiver_map_.insert(std::make_pair(type, receiver));
  } else {
    DLOG(FATAL) << "Handler already registered for type=" << type << ".";
  }
}

net::Error BlimpMessageDispatcher::OnBlimpMessage(const BlimpMessage& message) {
  auto receiver_iter = feature_receiver_map_.find(message.type());
  if (receiver_iter == feature_receiver_map_.end()) {
    DLOG(FATAL) << "No registered receiver for "
                << BlimpMessageToDebugString(message) << ".";
    return net::ERR_NOT_IMPLEMENTED;
  }

  return receiver_iter->second->OnBlimpMessage(message);
}

}  // namespace blimp
