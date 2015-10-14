// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_RECEIVER_H_
#define BLIMP_NET_BLIMP_MESSAGE_RECEIVER_H_

#include "net/base/net_errors.h"

namespace blimp {

class BlimpMessage;

// Interface implemented by components that process BlimpMessages.
class BlimpMessageReceiver {
 public:
  virtual ~BlimpMessageReceiver() {}

  // Processes the BlimpMessage, or returns a net::ERR_* on error.
  // If an error code is returned, callers should terminate the connection
  // from which |message| was received.
  virtual net::Error OnBlimpMessage(const BlimpMessage& message) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_RECEIVER_H_
