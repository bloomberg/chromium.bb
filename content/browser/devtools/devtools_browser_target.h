// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace base {

class DictionaryValue;
class MessageLoopProxy;
class Value;

}  // namespace base

namespace net {
class HttpServer;
}  // namespace net


namespace content {

// This class handles the "Browser" target for remote debugging.
class DevToolsBrowserTarget {
 public:
  // A thin interface to send notifications over WebSocket.
  class Notifier {
   public:
    virtual ~Notifier() {}

    virtual void Notify(const std::string& data) = 0;

   protected:
    Notifier() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Notifier);
  };

  class Handler {
   public:
    virtual ~Handler();

    // Returns the domain name for this handler.
    virtual std::string Domain() = 0;

    // |return_value| and |error_message_out| ownership is transferred to the
    // caller.
    virtual base::Value* OnProtocolCommand(
        const std::string& method,
        const base::DictionaryValue* params,
        base::Value** error_message_out) = 0;

    void set_notifier(Notifier* notifier);
    Notifier* notifier() const;

   protected:
    Handler();

   private:
    Notifier* notifier_;

    DISALLOW_COPY_AND_ASSIGN(Handler);
  };



  DevToolsBrowserTarget(base::MessageLoopProxy* message_loop_proxy,
                        net::HttpServer* server,
                        int connection_id);
  ~DevToolsBrowserTarget();

  int connection_id() const { return connection_id_; }

  // Takes ownership of |handler|.
  void RegisterHandler(Handler* handler);

  std::string HandleMessage(const std::string& data);

 private:
  scoped_ptr<Notifier> notifier_;
  const int connection_id_;

  typedef std::map<std::string, Handler*> HandlersMap;
  HandlersMap handlers_;

  // Takes ownership of |error_object|.
  std::string SerializeErrorResponse(int request_id, base::Value* error_object);

  base::Value* CreateErrorObject(int error_code, const std::string& message);

  std::string SerializeResponse(int request_id, base::Value* response);

  DISALLOW_COPY_AND_ASSIGN(DevToolsBrowserTarget);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
