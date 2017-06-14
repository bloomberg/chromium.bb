// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
#define COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_

#include <string>

namespace cast_channel {

// Maps to enum ReadyState in cast_channel.idl
enum class ReadyState {
  NONE,
  CONNECTING,
  OPEN,
  CLOSING,
  CLOSED,
};

// Maps to enum ChannelError in cast_channel.idl
enum class ChannelError {
  NONE,
  CHANNEL_NOT_OPEN,
  AUTHENTICATION_ERROR,
  CONNECT_ERROR,
  CAST_SOCKET_ERROR,
  TRANSPORT_ERROR,
  INVALID_MESSAGE,
  INVALID_CHANNEL_ID,
  CONNECT_TIMEOUT,
  PING_TIMEOUT,
  UNKNOWN,
};

// Maps to enum ChannelAuth in cast_channel.idl
enum class ChannelAuthType {
  NONE,
  SSL_VERIFIED,
};

std::string ReadyStateToString(ReadyState ready_state);
std::string ChannelErrorToString(ChannelError channel_error);
std::string ChannelAuthTypeToString(ChannelAuthType channel_auth);

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
