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
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace content {

// Utility classes for processing DevTools remote debugging messages.
// https://developers.google.com/chrome-developer-tools/docs/debugger-protocol
class DevToolsProtocol {
 public:
  typedef base::Callback<void(const std::string& message)> Notifier;

  class Response;

  class Message : public base::RefCountedThreadSafe<Message> {
   public:
    std::string domain() { return domain_; }
    std::string method() { return method_; }
    base::DictionaryValue* params() { return params_.get(); }
    virtual std::string Serialize() = 0;

   protected:
    friend class base::RefCountedThreadSafe<Message>;
    virtual ~Message();
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
    int id() { return id_; }

    virtual std::string Serialize() OVERRIDE;

    // Creates success response. Takes ownership of |result|.
    scoped_refptr<Response> SuccessResponse(base::DictionaryValue* result);

    // Creates error response.
    scoped_refptr<Response> InternalErrorResponse(const std::string& message);

    // Creates error response.
    scoped_refptr<Response> InvalidParamResponse(const std::string& param);

    // Creates error response.
    scoped_refptr<Response> NoSuchMethodErrorResponse();

    // Creates async response promise.
    scoped_refptr<Response> AsyncResponsePromise();

   protected:
    virtual  ~Command();

   private:
    friend class DevToolsProtocol;
    Command(int id, const std::string& method,
            base::DictionaryValue* params);

    int id_;

    DISALLOW_COPY_AND_ASSIGN(Command);
  };

  class Response : public base::RefCountedThreadSafe<Response> {
   public:
    std::string Serialize();

    bool is_async_promise() { return is_async_promise_; }

   private:
    friend class base::RefCountedThreadSafe<Response>;
    friend class Command;
    friend class DevToolsProtocol;
    virtual  ~Response();

    Response(int id, base::DictionaryValue* result);
    Response(int id, int error_code, const std::string& error_message);

    int id_;
    scoped_ptr<base::DictionaryValue> result_;
    int error_code_;
    std::string error_message_;
    bool is_async_promise_;

    DISALLOW_COPY_AND_ASSIGN(Response);
  };

  class Notification : public Message {
   public:

    virtual std::string Serialize() OVERRIDE;

   private:
    friend class DevToolsProtocol;
    virtual ~Notification();

    // Takes ownership of |params|.
    Notification(const std::string& method,
                 base::DictionaryValue* params);

    DISALLOW_COPY_AND_ASSIGN(Notification);
  };

  class Handler {
   public:
    typedef base::Callback<scoped_refptr<DevToolsProtocol::Response>(
        scoped_refptr<DevToolsProtocol::Command> command)> CommandHandler;

    virtual ~Handler();

    virtual scoped_refptr<DevToolsProtocol::Response> HandleCommand(
        scoped_refptr<DevToolsProtocol::Command> command);

    void SetNotifier(const Notifier& notifier);

   protected:
    Handler();

    void RegisterCommandHandler(const std::string& command,
                                const CommandHandler& handler);

    // Sends notification to the client. Takes ownership of |params|.
    void SendNotification(const std::string& method,
                          base::DictionaryValue* params);

    // Sends message to client, the caller is presumed to properly
    // format the message.
    void SendRawMessage(const std::string& message);

   private:
    typedef std::map<std::string, CommandHandler> CommandHandlers;

    Notifier notifier_;
    CommandHandlers command_handlers_;

    DISALLOW_COPY_AND_ASSIGN(Handler);
  };

  static scoped_refptr<Command> ParseCommand(const std::string& json,
                                             std::string* error_response);

  static scoped_refptr<Notification> ParseNotification(
      const std::string& json);

  static scoped_refptr<Notification> CreateNotification(
      const std::string& method, base::DictionaryValue* params);

 private:
  static base::DictionaryValue* ParseMessage(const std::string& json,
                                             std::string* error_response);

  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
