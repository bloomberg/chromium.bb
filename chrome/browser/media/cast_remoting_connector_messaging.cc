// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector_messaging.h"

#include <stdio.h>

#include <limits>

#include "base/strings/string_number_conversions.h"

const char CastRemotingConnectorMessaging::kMessageFieldSeparator = ':';
const char CastRemotingConnectorMessaging::kStartRemotingMessageFormat[] =
    "START_CAST_REMOTING:session=%x";
const char CastRemotingConnectorMessaging::kStartStreamsMessageFormat[] =
    "START_CAST_REMOTING_STREAMS:session=%x:audio=%c:video=%c";
const char
CastRemotingConnectorMessaging::kStartedStreamsMessageFormatPartial[] =
    "STARTED_CAST_REMOTING_STREAMS:session=%x";
const char
CastRemotingConnectorMessaging::kStartedStreamsMessageAudioIdSpecifier[] =
    ":audio_stream_id=";
const char
CastRemotingConnectorMessaging::kStartedStreamsMessageVideoIdSpecifier[] =
    ":video_stream_id=";
const char CastRemotingConnectorMessaging::kStopRemotingMessageFormat[] =
    "STOP_CAST_REMOTING:session=%x";
const char CastRemotingConnectorMessaging::kStoppedMessageFormat[] =
    "STOPPED_CAST_REMOTING:session=%x";
const char CastRemotingConnectorMessaging::kFailedMessageFormat[] =
    "FAILED_CAST_REMOTING:session=%x";

// static
bool CastRemotingConnectorMessaging::IsMessageForSession(
    const std::string& message, const char* format,
    unsigned int expected_session_id) {
  unsigned int session_id;
  if (sscanf(message.c_str(), format, &session_id) == 1)
    return session_id == expected_session_id;
  return false;
}

// static
int32_t CastRemotingConnectorMessaging::GetStreamIdFromStartedMessage(
    base::StringPiece message, base::StringPiece specifier) {
  auto start = message.find(specifier);
  if (start == std::string::npos)
    return -1;
  start += specifier.size();
  if (start + 1 >= message.size())
    return -1; // Must be at least one hex digit following the specifier.
  const auto length = message.find(kMessageFieldSeparator, start) - start;
  int parsed_value;
  if (!base::HexStringToInt(message.substr(start, length), &parsed_value) ||
      parsed_value < 0 ||
      parsed_value > std::numeric_limits<int32_t>::max()) {
    return -1; // Non-hex digits, or outside valid range.
  }
  return static_cast<int32_t>(parsed_value);
}
