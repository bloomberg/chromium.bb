// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_

#include "base/callback.h"
#include "base/values.h"

namespace content {

using DevToolsCommandId = int;
class DevToolsProtocolHandler;
class DevToolsProtocolDispatcher;

class DevToolsProtocolClient {
 public:
  typedef base::Callback<void(const std::string& message)>
      RawMessageCallback;
  static const DevToolsCommandId kNoId;

  struct Response {
   public:
    static Response FallThrough();
    static Response OK();
    static Response InvalidParams(const std::string& param);
    static Response InternalError(const std::string& message);
    static Response ServerError(const std::string& message);

    int status() const;
    const std::string& message() const;

    bool IsFallThrough() const;

   private:
    friend class DevToolsProtocolHandler;

    explicit Response(int status);
    Response(int status, const std::string& message);

    int status_;
    std::string message_;
  };

  bool SendError(DevToolsCommandId command_id,
                 const Response& response);

  // Sends message to client, the caller is presumed to properly
  // format the message. Do not use unless you must.
  void SendRawMessage(const std::string& message);

  explicit DevToolsProtocolClient(
      const RawMessageCallback& raw_message_callback);
  virtual ~DevToolsProtocolClient();

 protected:
  void SendSuccess(DevToolsCommandId command_id,
                   scoped_ptr<base::DictionaryValue> params);
  void SendNotification(const std::string& method,
                        scoped_ptr<base::DictionaryValue> params);

 private:
  friend class DevToolsProtocolDispatcher;

  void SendMessage(const base::DictionaryValue& message);

  RawMessageCallback raw_message_callback_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
