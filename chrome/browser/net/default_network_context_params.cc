// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/default_network_context_params.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();

  network_context_params->enable_brotli =
      base::FeatureList::IsEnabled(features::kBrotliEncoding);

  network_context_params->user_agent = GetUserAgent();

  std::string quic_user_agent_id = chrome::GetChannelName();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(content::BuildOSCpuInfo());
  network_context_params->quic_user_agent_id = quic_user_agent_id;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // TODO(eroman): Figure out why this doesn't work in single-process mode,
  // or if it does work, now.
  // Should be possible now that a private isolate is used.
  // http://crbug.com/474654
  if (!command_line.HasSwitch(switches::kWinHttpProxyResolver)) {
    if (command_line.HasSwitch(switches::kSingleProcess)) {
      LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    } else {
      network_context_params->proxy_resolver_factory =
          ChromeMojoProxyResolverFactory::CreateWithStrongBinding()
              .PassInterface();
    }
  }

  bool http_09_on_non_default_ports_enabled = false;
  const base::Value* value =
      g_browser_process->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kHttp09OnNonDefaultPortsEnabled);
  if (value)
    value->GetAsBoolean(&http_09_on_non_default_ports_enabled);
  network_context_params->http_09_on_non_default_ports_enabled =
      http_09_on_non_default_ports_enabled;

  return network_context_params;
}
