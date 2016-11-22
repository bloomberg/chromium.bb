// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

namespace protocol {
class IOHandler;
class MemoryHandler;
class SystemInfoHandler;
class TetheringHandler;
class TracingHandler;
}  // namespace protocol

class BrowserDevToolsAgentHost : public DevToolsAgentHostImpl {
 private:
  friend class DevToolsAgentHost;
  BrowserDevToolsAgentHost(
      scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
      const CreateServerSocketCallback& socket_callback);
  ~BrowserDevToolsAgentHost() override;

  // DevToolsAgentHostImpl implementation.
  void Attach() override;
  void Detach() override;

  // DevToolsAgentHost implementation.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  bool DispatchProtocolMessage(const std::string& message) override;

  scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner_;
  CreateServerSocketCallback socket_callback_;

  std::unique_ptr<protocol::IOHandler> io_handler_;
  std::unique_ptr<protocol::MemoryHandler> memory_handler_;
  std::unique_ptr<protocol::SystemInfoHandler> system_info_handler_;
  std::unique_ptr<protocol::TetheringHandler> tethering_handler_;
  std::unique_ptr<protocol::TracingHandler> tracing_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_BROWSER_DEVTOOLS_AGENT_HOST_H_
