// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_JSON_PARSER_HANDLER_H_
#define CHROME_UTILITY_SAFE_JSON_PARSER_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/utility/utility_message_handler.h"

namespace safe_json {
class SafeJsonParserMessageFilter;
}  // namespace safe_json

// Dispatches IPCs for out of process JSON parsing. This is an adapter class
// that delegates to safe_json::SafeJsonParserMessageFilter, since the
// SafeJsonParserMessageFilter in //components/safe_json can't directly
// depend on //chrome/utility.
class SafeJsonParserHandler : public UtilityMessageHandler {
 public:
  SafeJsonParserHandler();
  ~SafeJsonParserHandler() override;

  // UtilityMessageHandler
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  std::unique_ptr<safe_json::SafeJsonParserMessageFilter> handler_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserHandler);
};

#endif  // CHROME_UTILITY_SAFE_JSON_PARSER_HANDLER_H_
