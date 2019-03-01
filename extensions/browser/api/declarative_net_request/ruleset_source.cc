// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include <utility>

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"

namespace extensions {
namespace declarative_net_request {

// static
RulesetSource RulesetSource::Create(const Extension& extension) {
  return RulesetSource(
      declarative_net_request::DNRManifestData::GetRulesetPath(extension),
      file_util::GetIndexedRulesetPath(extension.path()));
}

RulesetSource::RulesetSource(base::FilePath json_path,
                             base::FilePath indexed_path)
    : json_path(std::move(json_path)), indexed_path(std::move(indexed_path)) {}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

RulesetSource RulesetSource::Clone() const {
  return RulesetSource(json_path, indexed_path);
}

}  // namespace declarative_net_request
}  // namespace extensions
