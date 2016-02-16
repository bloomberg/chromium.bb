// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/host_desktop.h"

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

class RemoteDebuggingServer {
 public:
  static void EnableTetheringForDebug();

  RemoteDebuggingServer(const std::string& ip, uint16_t port);

  virtual ~RemoteDebuggingServer();

 private:
  scoped_ptr<devtools_http_handler::DevToolsHttpHandler> devtools_http_handler_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
