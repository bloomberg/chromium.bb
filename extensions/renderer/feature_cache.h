// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_FEATURE_CACHE_H_
#define EXTENSIONS_RENDERER_FEATURE_CACHE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"

namespace extensions {
class ScriptContext;

// Caches features available to different extensions in different context types,
// and returns features available to a given context. Note: currently, this is
// only used for non-webpage contexts.
// TODO(devlin): Use it for all context types?
class FeatureCache {
 public:
  using FeatureNameVector = std::vector<std::string>;

  FeatureCache();
  ~FeatureCache();

  // Returns the names of features available to the given |context| in a
  // lexicographically sorted vector.
  FeatureNameVector GetAvailableFeatures(ScriptContext* context);

  // Invalidates the cache for the specified extension.
  void InvalidateExtension(const ExtensionId& extension_id);

 private:
  // Note: We use a key of ExtensionId, Feature::Context to maximize cache hits.
  // Unfortunately, this won't always be perfectly accurate, since some features
  // may have other context-dependent restrictions (such as URLs), but caching
  // by extension id + context + url would result in significantly fewer hits.
  using FeatureVector = std::vector<const Feature*>;
  using CacheMapKey = std::pair<ExtensionId, Feature::Context>;
  using CacheMap = std::map<CacheMapKey, FeatureVector>;

  // Returns the features available to the given context from the cache.
  const FeatureVector& GetFeaturesFromCache(ScriptContext* context);

  CacheMap feature_cache_;

  DISALLOW_COPY_AND_ASSIGN(FeatureCache);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FEATURE_CACHE_H_
