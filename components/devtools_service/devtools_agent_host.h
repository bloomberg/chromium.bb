// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_AGENT_HOST_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/callback.h"

namespace devtools_service {

// DevToolsAgentHost represents a DevTools agent at the service side.
class DevToolsAgentHost : public DevToolsAgentClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                                         const std::string& message) = 0;

    // Notifies the delegate that |agent_host| is going away.
    virtual void OnAgentHostClosed(DevToolsAgentHost* agent_host) = 0;
  };

  DevToolsAgentHost(const std::string& id, DevToolsAgentPtr agent);

  ~DevToolsAgentHost() override;

  void set_agent_connection_error_handler(const mojo::Closure& handler) {
    agent_.set_connection_error_handler(handler);
  }

  std::string id() const { return id_; }

  // Doesn't take ownership of |delegate|. If |delegate| dies before this
  // object, a new delegate or nullptr must be set so that this object doesn't
  // hold an invalid pointer.
  void SetDelegate(Delegate* delegate);

  bool IsAttached() const { return !!delegate_; }

  void SendProtocolMessageToAgent(const std::string& message);

 private:
  // DevToolsAgentClient implementation.
  void DispatchProtocolMessage(int32_t call_id,
                               const mojo::String& message,
                               const mojo::String& state) override;

  const std::string id_;

  DevToolsAgentPtr agent_;

  mojo::Binding<DevToolsAgentClient> binding_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentHost);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_AGENT_HOST_H_
