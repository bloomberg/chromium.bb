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
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "content/browser/devtools/devtools_protocol.h"

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
  DevToolsBrowserTarget(base::MessageLoopProxy* message_loop_proxy,
                        net::HttpServer* server,
                        int connection_id);
  ~DevToolsBrowserTarget();

  int connection_id() const { return connection_id_; }

  // Takes ownership of |handler|.
  void RegisterDomainHandler(const std::string& domain,
                             DevToolsProtocol::Handler* handler);

  std::string HandleMessage(const std::string& data);

 private:
  void OnNotification(const std::string& message);

  base::MessageLoopProxy* const message_loop_proxy_;
  net::HttpServer* const http_server_;
  const int connection_id_;

  typedef std::map<std::string, DevToolsProtocol::Handler*> DomainHandlerMap;
  DomainHandlerMap handlers_;
  STLValueDeleter<DomainHandlerMap> handlers_deleter_;
  base::WeakPtrFactory<DevToolsBrowserTarget> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBrowserTarget);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BROWSER_TARGET_H_
