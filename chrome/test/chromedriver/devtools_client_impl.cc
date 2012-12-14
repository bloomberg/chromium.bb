// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_client_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url)
    : socket_(factory.Run().Pass()),
      url_(url),
      connected_(false),
      next_id_(1) {}

DevToolsClientImpl::~DevToolsClientImpl() {}

Status DevToolsClientImpl::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  scoped_ptr<base::DictionaryValue> result;
  return SendCommandInternal(method, params, &result);
}

Status DevToolsClientImpl::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  scoped_ptr<base::DictionaryValue> intermediate_result;
  Status status = SendCommandInternal(method, params, &intermediate_result);
  if (status.IsError())
    return status;
  if (!intermediate_result)
    return Status(kUnknownError, "inspector response missing result");
  result->reset(intermediate_result.release());
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  if (!connected_) {
    if (!socket_->Connect(url_))
      return Status(kUnknownError, "unable to connect to renderer");
    connected_ = true;
  }

  base::DictionaryValue command;
  command.SetInteger("id", next_id_++);
  command.SetString("method", method);
  command.Set("params", params.DeepCopy());
  std::string message;
  base::JSONWriter::Write(&command, &message);
  if (!socket_->Send(message))
    return Status(kUnknownError, "unable to send message to renderer");

  std::string response;
  if (!socket_->ReceiveNextMessage(&response))
    return Status(kUnknownError, "unable to receive message from renderer");
  scoped_ptr<base::Value> response_value(base::JSONReader::Read(response));
  base::DictionaryValue* response_dict;
  if (!response_value || !response_value->GetAsDictionary(&response_dict))
    return Status(kUnknownError, "missing or invalid inspector response");

  int command_id;
  if (!response_dict->GetInteger("id", &command_id))
    return Status(kUnknownError, "inspector response must have integer 'id'");
  if (command_id != next_id_ - 1)
    return Status(kUnknownError, "inspector response ID does not match");
  base::DictionaryValue* unscoped_error;
  if (response_dict->GetDictionary("error", &unscoped_error)) {
    std::string error_string;
    base::JSONWriter::Write(unscoped_error, &error_string);
    return Status(kUnknownError, "inspector error: " + error_string);
  }
  base::DictionaryValue* unscoped_result;
  if (response_dict->GetDictionary("result", &unscoped_result))
    result->reset(unscoped_result->DeepCopy());
  return Status(kOk);
}
