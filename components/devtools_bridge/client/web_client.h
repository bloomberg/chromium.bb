// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_CLIENT_WEB_CLIENT_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_CLIENT_WEB_CLIENT_H_

#include "base/memory/scoped_ptr.h"

namespace content {
class BrowserContext;
}

namespace devtools_bridge {

/**
 * Client for DevTools Bridge for desktop Chrome. Uses WebContents to host
 * JavaScript implementation (therefore lives on the UI thread and must be
 * destroyed before |context|). WebContents works as a sandbox for WebRTC
 * related code.
 */
class WebClient {
 public:
  class Delegate {
   public:

    // TODO(serya): implement
  };

  virtual ~WebClient() {}

  static scoped_ptr<WebClient> CreateInstance(
      content::BrowserContext* context, Delegate* delegate);

  // TODO(serya): Implement.

 protected:
  WebClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebClient);
};

}  // namespace devtools_bridge

#endif // COMPONENTS_DEVTOOLS_BRIDGE_CLIENT_WEB_CLIENT_H_
