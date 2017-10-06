// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MESSAGING_UTIL_H_
#define EXTENSIONS_RENDERER_MESSAGING_UTIL_H_

#include <memory>

#include "v8/include/v8.h"

namespace extensions {
struct Message;

namespace messaging_util {

// Parses the message from a v8 value, returning null on failure.
std::unique_ptr<Message> MessageFromV8(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value);

// Converts a message to a v8 value. This is expected not to fail, since it
// should only be used for messages that have been validated.
v8::Local<v8::Value> MessageToV8(v8::Local<v8::Context> context,
                                 const Message& message);

}  // namespace messaging_util
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MESSAGING_UTIL_H_
