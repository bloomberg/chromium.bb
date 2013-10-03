// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_

#include <string>

#include "content/common/content_export.h"

class GURL;

namespace net {
class StreamListenSocketFactory;
class URLRequestContextGetter;
}

namespace content {

class DevToolsHttpHandlerDelegate;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:
  // Returns true if the given protocol version is supported.
  CONTENT_EXPORT static bool IsSupportedProtocolVersion(
      const std::string& version);

  // Returns frontend resource id for the given resource |name|.
  CONTENT_EXPORT static int GetFrontendResourceId(
      const std::string& name);

  // Takes ownership over |socket_factory| and |delegate|.
  CONTENT_EXPORT static DevToolsHttpHandler* Start(
      const net::StreamListenSocketFactory* socket_factory,
      const std::string& frontend_url,
      DevToolsHttpHandlerDelegate* delegate);

  // Called from the main thread in order to stop protocol handler.
  // Automatically destroys the handler instance.
  virtual void Stop() = 0;

  // Returns the URL for the address to debug |agent_host|.
  virtual GURL GetFrontendURL() = 0;

 protected:
  virtual ~DevToolsHttpHandler() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
