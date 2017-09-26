// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/default_network_context_params.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/user_agent.h"

content::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
  content::mojom::NetworkContextParamsPtr network_context_params =
      content::mojom::NetworkContextParams::New();

  network_context_params->enable_brotli =
      base::FeatureList::IsEnabled(features::kBrotliEncoding);

  std::string quic_user_agent_id = chrome::GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(content::BuildOSCpuInfo());
  network_context_params->quic_user_agent_id = quic_user_agent_id;

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
