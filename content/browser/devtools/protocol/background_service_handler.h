// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/devtools/protocol/background_service.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class DevToolsBackgroundServicesContext;
class RenderFrameHostImpl;

namespace protocol {

class BackgroundServiceHandler : public DevToolsDomainHandler,
                                 public BackgroundService::Backend {
 public:
  BackgroundServiceHandler();
  ~BackgroundServiceHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  Response Enable(const std::string& service) override;
  Response Disable(const std::string& service) override;
  Response SetRecording(bool should_record,
                        const std::string& service) override;

 private:
  std::unique_ptr<BackgroundService::Frontend> frontend_;

  // Owned by the storage partition.
  DevToolsBackgroundServicesContext* devtools_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundServiceHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
