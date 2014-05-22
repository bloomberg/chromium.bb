// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <map>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

namespace {

class Static {
 public:
  FeatureProvider* GetFeatures(const std::string& name) const {
    FeatureProviderMap::const_iterator it = feature_providers_.find(name);
    CHECK(it != feature_providers_.end());
    return it->second.get();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<Static>;

  Static() {
    ExtensionsClient* client = ExtensionsClient::Get();
    feature_providers_["api"] =
        make_linked_ptr(client->CreateFeatureProvider("api").release());
    feature_providers_["manifest"] =
        make_linked_ptr(client->CreateFeatureProvider("manifest").release());
    feature_providers_["permission"] =
        make_linked_ptr(client->CreateFeatureProvider("permission").release());
  }

  typedef std::map<std::string, linked_ptr<FeatureProvider> >
      FeatureProviderMap;

  FeatureProviderMap feature_providers_;
};

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const FeatureProvider* FeatureProvider::GetByName(const std::string& name) {
  return g_static.Get().GetFeatures(name);
}

// static
const FeatureProvider* FeatureProvider::GetAPIFeatures() {
  return GetByName("api");
}

// static
const FeatureProvider* FeatureProvider::GetManifestFeatures() {
  return GetByName("manifest");
}

// static
const FeatureProvider* FeatureProvider::GetPermissionFeatures() {
  return GetByName("permission");
}

}  // namespace extensions
