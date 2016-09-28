// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_MESSAGING_H_
#define CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_MESSAGING_H_

#include <stdint.h>

#include <string>

#include "base/strings/string_piece.h"

// Utility functions for parsing messages from the Cast Provider to
// CastRemotingConnector. These have been broken-out into this separate module
// to allow for efficient building, linking and execution of these routines for
// fuzzer testing.
//
// Note: If any additions/changes are made here, please update
// cast_remoting_connector_fuzzertest.cc as well!
class CastRemotingConnectorMessaging {
 public:
  // Returns true if the given |message| from the Cast Provider matches the
  // given |format| and the session ID in the |message| is equal to the
  // |expected_session_id|.
  static bool IsMessageForSession(const std::string& message,
                                  const char* format,
                                  unsigned int expected_session_id);

  // Scans |message| for |specifier| and extracts the remoting stream ID that
  // follows the specifier. Returns a negative value on error.
  static int32_t GetStreamIdFromStartedMessage(base::StringPiece message,
                                               base::StringPiece specifier);

  // Simple command messages sent from/to the CastRemotingConnector to/from the
  // Media Router Cast Provider to start/stop media remoting to a Cast device.
  //
  // Field separator (for tokenizing parts of messages).
  static const char kMessageFieldSeparator;
  // Message sent by CastRemotingConnector to Cast provider to start remoting.
  // Example:
  //   "START_CAST_REMOTING:session=1f"
  static const char kStartRemotingMessageFormat[];
  // Message sent by CastRemotingConnector to Cast provider to start the
  // remoting RTP stream(s). Example:
  //   "START_CAST_REMOTING_STREAMS:session=1f:audio=N:video=Y"
  static const char kStartStreamsMessageFormat[];
  // Start acknowledgement message sent by Cast provider to
  // CastRemotingConnector once remoting RTP streams have been set up. Examples:
  //   "STARTED_CAST_REMOTING_STREAMS:session=1f:audio_stream_id=2e:"
  //                                                        "video_stream_id=3d"
  //   "STARTED_CAST_REMOTING_STREAMS:session=1f:video_stream_id=b33f"
  static const char kStartedStreamsMessageFormatPartial[];
  static const char kStartedStreamsMessageAudioIdSpecifier[];
  static const char kStartedStreamsMessageVideoIdSpecifier[];
  // Stop message sent by CastRemotingConnector to Cast provider. Example:
  //   "STOP_CAST_REMOTING:session=1f"
  static const char kStopRemotingMessageFormat[];
  // Stop acknowledgement message sent by Cast provider to CastRemotingConnector
  // once remoting is available again after the last session ended. Example:
  //   "STOPPED_CAST_REMOTING:session=1f"
  static const char kStoppedMessageFormat[];
  // Failure message sent by Cast provider to CastRemotingConnector any time
  // there was a fatal error (e.g., the Cast provider failed to set up the RTP
  // streams, or there was some unexpected external event). Example:
  //   "FAILED_CAST_REMOTING:session=1f"
  static const char kFailedMessageFormat[];
};

#endif  // CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_MESSAGING_H_
