// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#pragma once

#include <string>

#include "content/common/content_export.h"

namespace net {
class StreamListenSocketFactory;
class URLRequestContextGetter;
}

namespace content {

class DevToolsHttpHandlerDelegate;
class RenderViewHost;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:
  // Interface responsible for mapping RenderViewHost instances to/from string
  // identifiers.
  class RenderViewHostBinding {
   public:
    virtual ~RenderViewHostBinding() {}

    // Returns the mapping of RenderViewHost to identifier.
    virtual std::string GetIdentifier(RenderViewHost* rvh) = 0;

    // Returns the mapping of identifier to RenderViewHost.
    virtual RenderViewHost* ForIdentifier(const std::string& identifier) = 0;
  };

  // Returns frontend resource id for the given resource |name|.
  CONTENT_EXPORT static int GetFrontendResourceId(
      const std::string& name);

  // Takes ownership over |socket_factory| and |delegate|.
  CONTENT_EXPORT static DevToolsHttpHandler* Start(
      const net::StreamListenSocketFactory* socket_factory,
      const std::string& frontend_url,
      net::URLRequestContextGetter* request_context_getter,
      DevToolsHttpHandlerDelegate* delegate);

  // Called from the main thread in order to stop protocol handler.
  // Automatically destroys the handler instance.
  virtual void Stop() = 0;

  // Set the RenderViewHostBinding instance. If no instance is provided the
  // default implementation will be used.
  virtual void SetRenderViewHostBinding(RenderViewHostBinding* binding) = 0;

 protected:
  virtual ~DevToolsHttpHandler() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
