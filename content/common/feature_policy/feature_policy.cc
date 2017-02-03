// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/feature_policy/feature_policy.h"

namespace content {

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration()
    : matches_all_origins(false) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    std::string feature_name,
    bool matches_all_origins,
    std::vector<url::Origin> origins)
    : feature_name(feature_name),
      matches_all_origins(matches_all_origins),
      origins(origins) {}

ParsedFeaturePolicyDeclaration::ParsedFeaturePolicyDeclaration(
    const ParsedFeaturePolicyDeclaration& rhs) = default;

ParsedFeaturePolicyDeclaration::~ParsedFeaturePolicyDeclaration() {}

}  // namespace content
