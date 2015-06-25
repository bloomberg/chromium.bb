// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_json_parser_handler.h"

#include "components/safe_json/safe_json_parser_message_filter.h"

SafeJsonParserHandler::SafeJsonParserHandler()
    : handler_(new safe_json::SafeJsonParserMessageFilter) {
}

SafeJsonParserHandler::~SafeJsonParserHandler() {
}

bool SafeJsonParserHandler::OnMessageReceived(const IPC::Message& message) {
  return handler_->OnMessageReceived(message);
}
