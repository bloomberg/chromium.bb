// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/memory.h"

namespace content {
namespace protocol {

class MemoryHandler : public DevToolsDomainHandler,
                      public Memory::Backend {
 public:
  MemoryHandler();
  ~MemoryHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  Response SetPressureNotificationsSuppressed(bool suppressed) override;
  Response SimulatePressureNotification(const std::string& level) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEMORY_HANDLER_H_
