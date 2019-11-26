// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CAST_MESSAGE_HANDLER_H_
#define CAST_COMMON_CHANNEL_CAST_MESSAGE_HANDLER_H_

namespace cast {
namespace channel {

class CastSocket;
class CastMessage;
class VirtualConnectionRouter;

class CastMessageHandler {
 public:
  virtual ~CastMessageHandler() = default;

  virtual void OnMessage(VirtualConnectionRouter* router,
                         CastSocket* socket,
                         CastMessage&& message) = 0;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_CAST_MESSAGE_HANDLER_H_
