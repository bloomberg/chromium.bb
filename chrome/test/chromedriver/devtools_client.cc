// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_client.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"
#include "googleurl/src/gurl.h"

DevToolsClient::DevToolsClient(
    URLRequestContextGetter* context_getter,
    const std::string& url)
    : context_getter_(context_getter),
      url_(url),
      socket_(new SyncWebSocket(context_getter)),
      connected_(false) {}

DevToolsClient::~DevToolsClient() {}

Status DevToolsClient::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  if (!connected_) {
    if (!socket_->Connect(GURL(url_)))
      return Status(kUnknownError, "unable to connect to renderer");
    connected_ = true;
  }
  base::DictionaryValue command;
  command.SetInteger("id", 1);
  command.SetString("method", method);
  command.Set("params", params.DeepCopy());
  std::string message;
  base::JSONWriter::Write(&command, &message);
  if (!socket_->Send(message))
    return Status(kUnknownError, "unable to send message to renderer");
  std::string response;
  if (!socket_->ReceiveNextMessage(&response))
    return Status(kUnknownError, "unable to receive message from renderer");
  return Status(kOk);
}
