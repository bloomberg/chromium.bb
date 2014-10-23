// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_client.h"

namespace content {

DevToolsProtocolClient::DevToolsProtocolClient(
    const RawMessageCallback& raw_message_callback)
    : raw_message_callback_(raw_message_callback) {
}

DevToolsProtocolClient::~DevToolsProtocolClient() {
}

void DevToolsProtocolClient::SendNotification(const std::string& method,
                                              base::DictionaryValue* params) {
  scoped_refptr<DevToolsProtocol::Notification> notification =
      new DevToolsProtocol::Notification(method, params);
  SendRawMessage(notification->Serialize());
}

void DevToolsProtocolClient::SendAsyncResponse(
    scoped_refptr<DevToolsProtocol::Response> response) {
  SendRawMessage(response->Serialize());
}

void DevToolsProtocolClient::SendRawMessage(const std::string& message) {
  raw_message_callback_.Run(message);
}

void DevToolsProtocolClient::SendInvalidParamsResponse(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& message) {
  SendAsyncResponse(command->InvalidParamResponse(message));
}

void DevToolsProtocolClient::SendInternalErrorResponse(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& message) {
  SendAsyncResponse(command->InternalErrorResponse(message));
}

void DevToolsProtocolClient::SendServerErrorResponse(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& message) {
  SendAsyncResponse(command->ServerErrorResponse(message));
}

typedef DevToolsProtocolClient::Response Response;

Response Response::FallThrough() {
  Response response;
  response.status_ = ResponseStatus::RESPONSE_STATUS_FALLTHROUGH;
  return response;
}

Response Response::OK() {
  Response response;
  response.status_ = ResponseStatus::RESPONSE_STATUS_OK;
  return response;
}

Response Response::InvalidParams(const std::string& message) {
  Response response;
  response.status_ = ResponseStatus::RESPONSE_STATUS_INVALID_PARAMS;
  response.message_ = message;
  return response;
}

Response Response::InternalError(const std::string& message) {
  Response response;
  response.status_ = ResponseStatus::RESPONSE_STATUS_INTERNAL_ERROR;
  response.message_ = message;
  return response;
}

Response Response::ServerError(const std::string& message) {
  Response response;
  response.status_ = ResponseStatus::RESPONSE_STATUS_SERVER_ERROR;
  response.message_ = message;
  return response;
}

DevToolsProtocolClient::ResponseStatus Response::status() const {
  return status_;
}

const std::string& Response::message() const {
  return message_;
}

Response::Response() {
}

}  // namespace content
