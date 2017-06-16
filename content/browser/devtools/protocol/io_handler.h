// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_IO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_IO_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/io.h"

namespace content {
class DevToolsIOContext;

namespace protocol {

class IOHandler : public DevToolsDomainHandler,
                  public IO::Backend {
 public:
  explicit IOHandler(DevToolsIOContext* io_context);
  ~IOHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  // Protocol methods.
  void Read(
      const std::string& handle,
      Maybe<int> offset,
      Maybe<int> max_size,
      std::unique_ptr<ReadCallback> callback) override;
  Response Close(const std::string& handle) override;

 private:
  void ReadComplete(std::unique_ptr<ReadCallback> callback,
                    std::unique_ptr<std::string> data,
                    int status);

  std::unique_ptr<IO::Frontend> frontend_;
  DevToolsIOContext* io_context_;
  base::WeakPtrFactory<IOHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
