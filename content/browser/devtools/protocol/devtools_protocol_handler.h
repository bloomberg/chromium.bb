// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {

class DevToolsProtocolHandler {
 public:
  using Response = DevToolsProtocolClient::Response;
  using Notifier = base::Callback<void(const std::string& message)>;

  explicit DevToolsProtocolHandler(const Notifier& notifier);
  virtual ~DevToolsProtocolHandler();

  scoped_ptr<base::DictionaryValue> ParseCommand(const std::string& message);
  void HandleCommand(scoped_ptr<base::DictionaryValue> command);
  bool HandleOptionalCommand(scoped_ptr<base::DictionaryValue> command);

  DevToolsProtocolDispatcher* dispatcher() { return &dispatcher_; }

 private:
  DevToolsProtocolClient client_;
  DevToolsProtocolDispatcher dispatcher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_
