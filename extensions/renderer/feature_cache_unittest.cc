// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/feature_cache.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/scoped_web_frame.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

namespace {

// TODO(devlin): We should really just make ExtensionBuilder better to avoid
// having so many of these methods lying around.
scoped_refptr<Extension> CreateExtension(
    const std::string& name,
    const std::vector<std::string>& permissions) {
  DictionaryBuilder manifest;
  manifest.Set("name", name);
  manifest.Set("manifest_version", 2);
  manifest.Set("version", "0.1");
  manifest.Set("description", "test extension");

  {
    ListBuilder permissions_builder;
    for (const std::string& permission : permissions)
      permissions_builder.Append(permission);
    manifest.Set("permissions", permissions_builder.Build());
  }

  return ExtensionBuilder()
      .SetManifest(manifest.Build())
      .SetLocation(Manifest::INTERNAL)
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

struct FakeContext {
  Feature::Context context_type;
  const Extension* extension;
  const GURL url;
};

bool HasFeature(FeatureCache& cache,
                const FakeContext& context,
                const std::string& feature) {
  return base::ContainsValue(
      cache.GetAvailableFeatures(context.context_type, context.extension,
                                 context.url),
      feature);
}

}  // namespace

using FeatureCacheTest = testing::Test;

TEST_F(FeatureCacheTest, Basic) {
  FeatureCache cache;
  scoped_refptr<const Extension> extension_a = CreateExtension("a", {});
  scoped_refptr<const Extension> extension_b =
      CreateExtension("b", {"storage"});

  FakeContext context_a = {Feature::BLESSED_EXTENSION_CONTEXT,
                           extension_a.get(), extension_a->url()};
  FakeContext context_b = {Feature::BLESSED_EXTENSION_CONTEXT,
                           extension_b.get(), extension_b->url()};
  // To start, context a should not have access to storage, but context b
  // should.
  EXPECT_FALSE(HasFeature(cache, context_a, "storage"));
  EXPECT_TRUE(HasFeature(cache, context_b, "storage"));

  // Update extension b's permissions and invalidate the cache.
  extension_b->permissions_data()->SetPermissions(
      base::MakeUnique<PermissionSet>(), base::MakeUnique<PermissionSet>());
  cache.InvalidateExtension(extension_b->id());

  // Now, neither context should have storage access.
  EXPECT_FALSE(HasFeature(cache, context_a, "storage"));
  EXPECT_FALSE(HasFeature(cache, context_b, "storage"));
}

TEST_F(FeatureCacheTest, WebUIContexts) {
  FeatureCache cache;
  scoped_refptr<const Extension> extension = CreateExtension("a", {});

  // The chrome://extensions page is whitelisted for the management API.
  FakeContext webui_context = {Feature::WEBUI_CONTEXT, nullptr,
                               GURL("chrome://extensions")};
  // chrome://baz is not whitelisted, and should not have access.
  FakeContext webui_context_without_access = {Feature::WEBUI_CONTEXT, nullptr,
                                              GURL("chrome://baz")};

  EXPECT_TRUE(HasFeature(cache, webui_context, "management"));
  EXPECT_FALSE(HasFeature(cache, webui_context_without_access, "management"));
  // No webui context is whitelisted for, e.g., the idle API, so neither should
  // have access.
  EXPECT_FALSE(HasFeature(cache, webui_context, "idle"));
  EXPECT_FALSE(HasFeature(cache, webui_context_without_access, "idle"));
}

}  // namespace extensions
