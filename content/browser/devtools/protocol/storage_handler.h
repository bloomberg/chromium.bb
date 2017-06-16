// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class StorageHandler : public DevToolsDomainHandler,
                       public Storage::Backend {
 public:
  StorageHandler();
  ~StorageHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;

  Response ClearDataForOrigin(
      const std::string& origin,
      const std::string& storage_types) override;
  void GetUsageAndQuota(
      const String& origin,
      std::unique_ptr<GetUsageAndQuotaCallback> callback) override;

 private:
  RenderFrameHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
