// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#pragma once

#include <string>

#include "content/common/content_export.h"

namespace content {

class DevToolsHttpHandlerDelegate;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:
  // Takes ownership over |delegate|.
  CONTENT_EXPORT static DevToolsHttpHandler* Start(
      const std::string& ip,
      int port,
      const std::string& frontend_url,
      DevToolsHttpHandlerDelegate* delegate);

  // Called from the main thread in order to stop protocol handler.
  virtual void Stop() = 0;

 protected:
  virtual ~DevToolsHttpHandler() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
