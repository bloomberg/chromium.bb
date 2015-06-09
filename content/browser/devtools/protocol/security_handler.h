// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_

#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/devtools_protocol_client.h"

namespace content {
namespace devtools {
namespace security {

class SecurityHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  SecurityHandler();
  virtual ~SecurityHandler();

  void SetClient(scoped_ptr<DevToolsProtocolClient> client);

  Response Enable();
  Response Disable();

 private:

  DISALLOW_COPY_AND_ASSIGN(SecurityHandler);
};

}  // namespace security
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
