// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "net/socket/stream_listen_socket.h"

namespace content {

class DevToolsTarget;

class DevToolsHttpHandlerDelegate {
 public:
  virtual ~DevToolsHttpHandlerDelegate() {}

  // Should return discovery page HTML that should list available tabs
  // and provide attach links.
  virtual std::string GetDiscoveryPageHTML() = 0;

  // Returns true if and only if frontend resources are bundled.
  virtual bool BundlesFrontendResources() = 0;

  // Returns path to the front-end files on the local filesystem for debugging.
  virtual base::FilePath GetDebugFrontendDir() = 0;

  // Creates named socket for reversed tethering implementation (used with
  // remote debugging, primarily for mobile).
  virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(
      net::StreamListenSocket::Delegate* delegate,
      std::string* name) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
