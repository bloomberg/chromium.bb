// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_

#include "content/browser/devtools/devtools_protocol.h"

namespace content {

class DevToolsProtocolClient {
 public:
  typedef base::Callback<void(const std::string& event,
                              base::DictionaryValue* params)> EventCallback;

  typedef base::Callback<void(scoped_refptr<DevToolsProtocol::Response>)>
      ResponseCallback;

  enum ResponseStatus {
    RESPONSE_STATUS_FALLTHROUGH,
    RESPONSE_STATUS_OK,
    RESPONSE_STATUS_INVALID_PARAMS,
    RESPONSE_STATUS_INTERNAL_ERROR,
    RESPONSE_STATUS_SERVER_ERROR,
  };

  struct Response {
   public:
    static Response FallThrough();
    static Response OK();
    static Response InvalidParams(const std::string& message);
    static Response InternalError(const std::string& message);
    static Response ServerError(const std::string& message);

    ResponseStatus status() const;
    const std::string& message() const;

   private:
    Response();

    ResponseStatus status_;
    std::string message_;
  };

  void SendInvalidParamsResponse(
      scoped_refptr<DevToolsProtocol::Command> command,
      const std::string& message);
  void SendInternalErrorResponse(
      scoped_refptr<DevToolsProtocol::Command> command,
      const std::string& message);
  void SendServerErrorResponse(
      scoped_refptr<DevToolsProtocol::Command> command,
      const std::string& message);

 protected:
  DevToolsProtocolClient(const EventCallback& event_callback,
                         const ResponseCallback& response_callback);

  virtual ~DevToolsProtocolClient();

  void SendNotification(const std::string& method,
                        base::DictionaryValue* params);

  void SendAsyncResponse(scoped_refptr<DevToolsProtocol::Response> response);

 private:
  EventCallback event_callback_;
  ResponseCallback response_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
