// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_

#include "content/browser/devtools/protocol/forward.h"

namespace content {

class DevToolsSession;
class DevToolsAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class DevToolsDomainHandler {
 public:
  explicit DevToolsDomainHandler(const std::string& name);
  virtual ~DevToolsDomainHandler();

  virtual void SetRenderFrameHost(RenderFrameHostImpl* host);
  virtual void Wire(UberDispatcher* dispatcher);
  virtual Response Disable();

  const std::string& name() const { return name_; }

 protected:
  static DevToolsSession* GetFirstSession(DevToolsAgentHostImpl* host);

 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDomainHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOMAIN_HANDLER_H_
