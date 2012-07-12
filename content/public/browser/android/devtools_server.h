// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_SERVER_H_

#include <string>

#include "base/callback_forward.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class DevToolsHttpHandlerDelegate;

// This class controls Developer Tools remote debugging server.
class DevToolsServer {
 public:
  // A callback being called each time the server restarts.
  // The delegate instance is deleted on server stop, so the creator must
  // provide the new instance on each call.
  typedef base::Callback<content::DevToolsHttpHandlerDelegate*(void)>
      DelegateCreator;

  // Returns the singleton instance (and initializes it if necessary).
  static DevToolsServer* GetInstance();

  // Initializes the server. Must be called prior to the first call of |Start|.
  virtual void Init(const std::string& frontend_version,
                    net::URLRequestContextGetter* url_request_context,
                    const DelegateCreator& delegate_creator) = 0;

  // Opens linux abstract socket to be ready for remote debugging.
  virtual void Start() = 0;

  // Closes debugging socket, stops debugging.
  virtual void Stop() = 0;

  virtual bool IsInitialized() const = 0;

  virtual bool IsStarted() const = 0;

 protected:
  virtual ~DevToolsServer() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
