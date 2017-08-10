// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/feature_cache.h"

#include <algorithm>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

FeatureCache::FeatureCache() {}
FeatureCache::~FeatureCache() = default;

FeatureCache::FeatureNameVector FeatureCache::GetAvailableFeatures(
    ScriptContext* context) {
  const FeatureVector& features = GetFeaturesFromCache(context);
  FeatureNameVector names;
  names.reserve(features.size());
  for (const Feature* feature : features) {
    // Since we only cache based on extension id and context type, instead of
    // all attributes of a context (like URL), we need to double-check if the
    // feature is actually available to the context. This is still a win, since
    // we only perform this check on the (much smaller) set of features that
    // *may* be available, rather than all known features.
    // TODO(devlin): Optimize this - we should be able to tell if a feature may
    // change based on additional context attributes.
    if (context->IsAnyFeatureAvailableToContext(
            *feature, CheckAliasStatus::NOT_ALLOWED)) {
      names.push_back(feature->name());
    }
  }
  return names;
}

void FeatureCache::InvalidateExtension(const ExtensionId& extension_id) {
  for (auto iter = feature_cache_.begin(); iter != feature_cache_.end();) {
    if (iter->first.first == extension_id)
      iter = feature_cache_.erase(iter);
    else
      ++iter;
  }
}

const FeatureCache::FeatureVector& FeatureCache::GetFeaturesFromCache(
    ScriptContext* context) {
  CacheMapKey key(context->GetExtensionID(), context->context_type());

  auto iter = feature_cache_.find(key);
  if (iter != feature_cache_.end())
    return iter->second;

  FeatureVector features;
  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  GURL empty_url;
  const Extension* extension = context->extension();
  for (const auto& map_entry : api_feature_provider->GetAllFeatures()) {
    Feature* feature = map_entry.second.get();
    // Exclude internal APIs.
    if (feature->IsInternal())
      continue;

    // Exclude child features (like events or specific functions).
    // TODO(devlin): Optimize this - instead of skipping child features and then
    // checking IsAnyFeatureAvailableToContext() (which checks child features),
    // we should just check all features directly.
    if (api_feature_provider->GetParent(*feature) != nullptr)
      continue;

    // Skip chrome.test if this isn't a test.
    if (map_entry.first == "test" &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kTestType)) {
      continue;
    }

    if (!ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
            *feature, extension, context->context_type(), empty_url,
            CheckAliasStatus::NOT_ALLOWED)) {
      continue;
    }

    features.push_back(feature);
  }

  std::sort(
      features.begin(), features.end(),
      [](const Feature* a, const Feature* b) { return a->name() < b->name(); });
  DCHECK(std::unique(features.begin(), features.end()) == features.end());

  return feature_cache_.emplace(key, std::move(features)).first->second;
}

}  // namespace extensions
