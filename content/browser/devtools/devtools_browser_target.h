// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_

#include <map>
#include <string>

#include "base/basictypes.h"

namespace base {

class DictionaryValue;
class Value;

}  // namespace base

namespace content {

// This class handles the "Browser" target for remote debugging.
class DevToolsBrowserTarget {
 public:
  class Handler {
   public:
    virtual ~Handler() {}

    // Returns the domain name for this handler.
    virtual std::string Domain() = 0;

    // |return_value| and |error_message_out| ownership is transferred to the
    // caller.
    virtual base::Value* OnProtocolCommand(
        const std::string& method,
        const base::DictionaryValue* params,
        base::Value** error_message_out) = 0;
  };

  explicit DevToolsBrowserTarget(int connection_id);
  ~DevToolsBrowserTarget();

  int connection_id() const { return connection_id_; }

  // Takes ownership of |handler|.
  void RegisterHandler(Handler* handler);

  std::string HandleMessage(const std::string& data);

 private:
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
