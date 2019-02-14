// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"

namespace extensions {
namespace declarative_net_request {

// static
RulesetSource RulesetSource::Create(const Extension& extension) {
  RulesetSource source;

  const ExtensionResource* extension_resource =
      declarative_net_request::DNRManifestData::GetRulesetResource(&extension);
  if (!extension_resource)
    return source;

  source.json_path = extension_resource->GetFilePath();
  source.indexed_path = file_util::GetIndexedRulesetPath(extension.path());
  return source;
}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

RulesetSource RulesetSource::Clone() const {
  RulesetSource clone;
  clone.json_path = json_path;
  clone.indexed_path = indexed_path;
  return clone;
}

RulesetSource::RulesetSource() = default;

}  // namespace declarative_net_request
}  // namespace extensions
