// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
#define CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// This struct holds feature policy whitelist data that needs to be replicated
// between a RenderFrame and any of its associated RenderFrameProxies. A list of
// these form a ParsedFeaturePolicyHeader.
// NOTE: These types are used for replication frame state between processes.
// They exist only because we can't transfer WebVectors directly over IPC.
struct CONTENT_EXPORT ParsedFeaturePolicyDeclaration {
  ParsedFeaturePolicyDeclaration();
  ParsedFeaturePolicyDeclaration(std::string feature_name,
                                 bool matches_all_origins,
                                 std::vector<url::Origin> origins);
  ParsedFeaturePolicyDeclaration(const ParsedFeaturePolicyDeclaration& rhs);
  ~ParsedFeaturePolicyDeclaration();

  std::string feature_name;
  bool matches_all_origins;
  std::vector<url::Origin> origins;
};

using ParsedFeaturePolicyHeader = std::vector<ParsedFeaturePolicyDeclaration>;

}  // namespace content

#endif  // CONTENT_COMMON_FEATURE_POLICY_FEATURE_POLICY_H_
