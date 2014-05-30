// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/values.h"

// Utility class for processing DevTools remote debugging messages.
// This is a stripped down clone of content::DevTools which is not accessible
// from chrome component (see content/browser/devtools/devtools_protocol.*).
class DevToolsProtocol {
 public:
  class Response;

  class Message {
   public:
    virtual ~Message();

    std::string method() { return method_; }
    base::DictionaryValue* params() { return params_.get(); }

   protected:
    Message(const std::string& method, base::DictionaryValue* params);

    std::string method_;
    scoped_ptr<base::DictionaryValue> params_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  class Command : public Message {
   public:
    Command(int id, const std::string& method, base::DictionaryValue* params);
    virtual ~Command();

    int id() { return id_; }
    std::string Serialize();

    // Creates success response. Takes ownership of |result|.
    scoped_ptr<Response> SuccessResponse(base::DictionaryValue* result);

    // Creates error response.
    scoped_ptr<Response> InvalidParamResponse(const std::string& param);

   private:
    int id_;

    DISALLOW_COPY_AND_ASSIGN(Command);
  };

  class Response {
   public:
    virtual ~Response();

    int id() { return id_; }
    int error_code() { return error_code_; }

    // Result ownership is passed to the caller.
    base::DictionaryValue* Serialize();

   private:
    friend class DevToolsProtocol;

    Response(int id, int error_code, const std::string error_message);
    Response(int id, base::DictionaryValue* result);
    int id_;
    int error_code_;
    std::string error_message_;
    scoped_ptr<base::DictionaryValue> result_;

    DISALLOW_COPY_AND_ASSIGN(Response);
  };

  class Notification : public Message {
   public:
    virtual ~Notification();

   private:
    friend class DevToolsProtocol;

    // Takes ownership of |params|.
    Notification(const std::string& method,
                 base::DictionaryValue* params);

    DISALLOW_COPY_AND_ASSIGN(Notification);
  };

  // Result ownership is passed to the caller.
  static Command* ParseCommand(base::DictionaryValue* command_dict);

  // Result ownership is passed to the caller.
  static Notification* ParseNotification(const std::string& json);

  // Result ownership is passed to the caller.
  static Response* ParseResponse(const std::string& json);

 private:

  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
