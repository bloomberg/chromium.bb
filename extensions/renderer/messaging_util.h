// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MESSAGING_UTIL_H_
#define EXTENSIONS_RENDERER_MESSAGING_UTIL_H_

#include <memory>
#include <string>

#include "v8/include/v8.h"

namespace extensions {
struct Message;

namespace messaging_util {

// The channel names for the sendMessage and sendRequest calls.
extern const char kSendMessageChannel[];
extern const char kSendRequestChannel[];

// Messaging-related events.
extern const char kOnMessageEvent[];
extern const char kOnMessageExternalEvent[];
extern const char kOnRequestEvent[];
extern const char kOnRequestExternalEvent[];
extern const char kOnConnectEvent[];
extern const char kOnConnectExternalEvent[];

extern const int kNoFrameId;

// Parses the message from a v8 value, returning null on failure.
std::unique_ptr<Message> MessageFromV8(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value);

// Converts a message to a v8 value. This is expected not to fail, since it
// should only be used for messages that have been validated.
v8::Local<v8::Value> MessageToV8(v8::Local<v8::Context> context,
                                 const Message& message);

// Extracts an integer id from |value|, including accounting for -0 (which is a
// valid integer, but is stored in V8 as a number). This will DCHECK that
// |value| is either an int32 or -0.
int ExtractIntegerId(v8::Local<v8::Value> value);

// The result of the call to ParseMessageOptions().
enum ParseOptionsResult {
  TYPE_ERROR,  // The arguments were invalid.
  THROWN,      // The script threw an error during parsing.
  SUCCESS,     // Parsing succeeded.
};

// Flags for ParseMessageOptions().
enum ParseOptionsFlags {
  NO_FLAGS = 0,
  PARSE_CHANNEL_NAME = 1,
  PARSE_FRAME_ID = 1 << 1,
  PARSE_INCLUDE_TLS_CHANNEL_ID = 1 << 2,
};

struct MessageOptions {
  std::string channel_name;
  int frame_id = kNoFrameId;
  bool include_tls_channel_id = false;
};

// Parses the parameters sent to sendMessage or connect, returning the result of
// the attempted parse. If |check_for_channel_name| is true, also checks for a
// provided channel name (this is only true for connect() calls). Populates the
// result in |params_out| or |error_out| (depending on the success of the
// parse).
ParseOptionsResult ParseMessageOptions(v8::Local<v8::Context> context,
                                       v8::Local<v8::Object> v8_options,
                                       int flags,
                                       MessageOptions* options_out,
                                       std::string* error_out);

}  // namespace messaging_util
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MESSAGING_UTIL_H_
