// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_TEST_MOCK_CAST_MESSAGE_HANDLER_H_
#define CAST_COMMON_CHANNEL_TEST_MOCK_CAST_MESSAGE_HANDLER_H_

#include "cast/common/channel/cast_message_handler.h"
#include "gmock/gmock.h"

namespace cast {
namespace channel {

class MockCastMessageHandler final : public CastMessageHandler {
 public:
  MOCK_METHOD(void,
              OnMessage,
              (VirtualConnectionRouter * router,
               CastSocket* socket,
               CastMessage&& message),
              (override));
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_TEST_MOCK_CAST_MESSAGE_HANDLER_H_
