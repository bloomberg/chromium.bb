// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MESSAGE_FILTER_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MESSAGE_FILTER_H_

#include <string>

#include "base/macros.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace safe_json {

// A Handler for the ParseJSON IPC message that does the actual JSON parsing
// in the sandboxed process. Modelled after IPC::MessageFilter but does not
// directly implement it as it will be used by an adapter class under
// //chrome/utility.
class SafeJsonParserMessageFilter {
 public:
  SafeJsonParserMessageFilter();
  ~SafeJsonParserMessageFilter();
  // Returns true if it receives a message it could handle (currently
  // only SafeJsonParserMsg_ParseJSON), false otherwise.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  void OnParseJSON(const std::string& json);

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserMessageFilter);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MESSAGE_FILTER_H_
