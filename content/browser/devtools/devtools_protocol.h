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

  class Response;

  class Message {
   public:
    virtual ~Message();

    std::string domain() { return domain_; }
    std::string method() { return method_; }
    base::DictionaryValue* params() { return params_.get(); }
    virtual std::string Serialize() = 0;

   protected:
    Message(const std::string& method,
            base::DictionaryValue* params);

    std::string domain_;
    std::string method_;
    scoped_ptr<base::DictionaryValue> params_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  class Command : public Message {
   public:
    virtual  ~Command();

    int id() { return id_; }

    virtual std::string Serialize() OVERRIDE;

    // Creates success response. Takes ownership of |result|.
    scoped_ptr<Response> SuccessResponse(base::DictionaryValue* result);

    // Creates error response. Caller takes ownership of the return value.
    scoped_ptr<Response> InternalErrorResponse(const std::string& message);

    // Creates error response. Caller takes ownership of the return value.
    scoped_ptr<Response> InvalidParamResponse(const std::string& param);

    // Creates error response. Caller takes ownership of the return value.
    scoped_ptr<Response> NoSuchMethodErrorResponse();

   private:
    friend class DevToolsProtocol;
    Command(int id, const std::string& method,
            base::DictionaryValue* params);

    int id_;

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

  class Notification : public Message {
   public:
    virtual ~Notification();

    virtual std::string Serialize() OVERRIDE;

   private:
    friend class DevToolsProtocol;

    // Takes ownership of |params|.
    Notification(const std::string& method,
                 base::DictionaryValue* params);

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

  static Notification* ParseNotification(const std::string& json);

  static Notification* CreateNotification(const std::string& method,
                                          base::DictionaryValue* params);

 private:
  static DictionaryValue* ParseMessage(const std::string& json,
                                       std::string* error_response);

  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
