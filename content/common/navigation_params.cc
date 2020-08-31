// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"

namespace content {

mojom::InitiatorCSPInfoPtr CreateInitiatorCSPInfo() {
  return mojom::InitiatorCSPInfo::New(
      network::mojom::CSPDisposition::CHECK,
      std::vector<network::mojom::ContentSecurityPolicyPtr>() /* empty */,
      nullptr /* initiator_self_source */
  );
}

mojom::CommonNavigationParamsPtr CreateCommonNavigationParams() {
  auto common_params = mojom::CommonNavigationParams::New();
  common_params->referrer = blink::mojom::Referrer::New();
  common_params->navigation_start = base::TimeTicks::Now();
  common_params->initiator_csp_info = CreateInitiatorCSPInfo();
  common_params->source_location = network::mojom::SourceLocation::New();

  return common_params;
}

mojom::CommitNavigationParamsPtr CreateCommitNavigationParams() {
  auto commit_params = mojom::CommitNavigationParams::New();
  commit_params->navigation_token = base::UnguessableToken::Create();
  commit_params->navigation_timing = mojom::NavigationTiming::New();

  return commit_params;
}

}  // namespace content
