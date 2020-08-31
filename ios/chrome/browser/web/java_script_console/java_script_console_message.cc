// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"

JavaScriptConsoleMessage::JavaScriptConsoleMessage() {}

JavaScriptConsoleMessage::JavaScriptConsoleMessage(
    const JavaScriptConsoleMessage& other)
    : url(other.url),
      level(other.level),
      message(base::Value::ToUniquePtrValue(other.message->Clone())) {}

JavaScriptConsoleMessage& JavaScriptConsoleMessage::operator=(
    JavaScriptConsoleMessage other) {
  url = other.url;
  level = other.level;
  message = std::move(other.message);
  return *this;
}

JavaScriptConsoleMessage::~JavaScriptConsoleMessage() {}
