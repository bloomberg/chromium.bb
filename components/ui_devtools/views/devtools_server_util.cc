// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/devtools_server_util.h"

#include <memory>

#include "base/command_line.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/switches.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "components/ui_devtools/views/overlay_agent_aura.h"

namespace ui_devtools {

namespace {

// Returns the port number specified by the command line flag. If a number is
// not specified as a command line argument, returns the default port (9223).
int GetPort() {
  constexpr int kUiDevToolsDefaultPort = 9223;
  auto value = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kEnableUiDevTools);
  int port = 0;
  return base::StringToInt(value, &port) ? port : kUiDevToolsDefaultPort;
}

}  // namespace

std::unique_ptr<UiDevToolsServer> CreateUiDevToolsServerForViews(
    network::mojom::NetworkContext* network_context,
    aura::Env* env) {
  auto server = UiDevToolsServer::CreateForViews(network_context, GetPort());
  DCHECK(server);
  auto client =
      std::make_unique<UiDevToolsClient>("UiDevToolsClient", server.get());
  auto dom_backend = std::make_unique<DOMAgentAura>(env);
  auto* dom_backend_ptr = dom_backend.get();
  client->AddAgent(std::move(dom_backend));
  client->AddAgent(std::make_unique<CSSAgent>(dom_backend_ptr));
  client->AddAgent(std::make_unique<OverlayAgentAura>(dom_backend_ptr, env));
  server->AttachClient(std::move(client));
  return server;
}

}  // namespace ui_devtools
