// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/values.h"

namespace content {

// Utility classes for processing DevTools remote debugging messages.
// https://developers.google.com/chrome-developer-tools/docs/debugger-protocol
class DevToolsProtocol {
 public:
  typedef base::Callback<void(const std::string& message)> Notifier;

  // JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
  enum Error {
    kErrorParseError = -32700,
    kErrorInvalidRequest = -32600,
    kErrorNoSuchMethod = -32601,
    kErrorInvalidParams = -32602,
    kErrorInternalError = -32603
  };

  class Response;

  class Command {
   public:
    ~Command();

    int id() { return id_; }
    std::string domain() { return domain_; }
    std::string method() { return method_; }
    base::DictionaryValue* params() { return params_.get(); }

    // Creates success response. Takes ownership of |result|.
    scoped_ptr<Response> SuccessResponse(base::DictionaryValue* result);

    // Creates error response. Caller takes ownership of the return value.
    scoped_ptr<Response> ErrorResponse(int error_code,
                                       const std::string& error_message);

    // Creates error response. Caller takes ownership of the return value.
    scoped_ptr<Response> NoSuchMethodErrorResponse();

   private:
    friend class DevToolsProtocol;
    Command(int id, const std::string& domain, const std::string& method,
            base::DictionaryValue* params);

    int id_;
    std::string domain_;
    std::string method_;
    scoped_ptr<base::DictionaryValue> params_;

    DISALLOW_COPY_AND_ASSIGN(Command);
  };

  class Response {
   public:
    ~Response();

    std::string Serialize();

   private:
    friend class Command;
    friend class DevToolsProtocol;

    Response(int id, base::DictionaryValue* result);
    Response(int id, int error_code, const std::string& error_message);

    int id_;
    scoped_ptr<base::DictionaryValue> result_;
    int error_code_;
    std::string error_message_;

    DISALLOW_COPY_AND_ASSIGN(Response);
  };

  class Notification {
   public:
    // Takes ownership of |params|.
    Notification(const std::string& method, base::DictionaryValue* params);
    ~Notification();

    std::string Serialize();

   private:
    std::string method_;
    scoped_ptr<base::DictionaryValue> params_;

    DISALLOW_COPY_AND_ASSIGN(Notification);
  };

  class Handler {
   public:
    typedef base::Callback<scoped_ptr<DevToolsProtocol::Response>(
        DevToolsProtocol::Command* command)> CommandHandler;

    virtual ~Handler();

    virtual scoped_ptr<DevToolsProtocol::Response> HandleCommand(
        DevToolsProtocol::Command* command);

    void SetNotifier(const Notifier& notifier);

   protected:
    Handler();

    void RegisterCommandHandler(const std::string& command,
                                const CommandHandler& handler);

    // Sends notification to the client. Takes ownership of |params|.
    void SendNotification(const std::string& method,
                          base::DictionaryValue* params);

   private:
    typedef std::map<std::string, CommandHandler> CommandHandlers;

    Notifier notifier_;
    CommandHandlers command_handlers_;

    DISALLOW_COPY_AND_ASSIGN(Handler);
  };

  static Command* ParseCommand(const std::string& json,
                               std::string* error_response);

 private:
  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
