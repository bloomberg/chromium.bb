// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/dom.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class DOMHandler : public DevToolsDomainHandler,
                   public DOM::Backend {
 public:
  explicit DOMHandler(bool allow_file_access);
  ~DOMHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  Response SetFileInputFiles(
      std::unique_ptr<protocol::Array<std::string>> files,
      Maybe<DOM::NodeId> node_id,
      Maybe<DOM::BackendNodeId> backend_node_id,
      Maybe<String> in_object_id) override;

 private:
  RenderFrameHostImpl* host_;
  bool allow_file_access_;
  DISALLOW_COPY_AND_ASSIGN(DOMHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_
