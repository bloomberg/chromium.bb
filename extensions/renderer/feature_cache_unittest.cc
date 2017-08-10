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

}  // namespace

using FeatureCacheTest = APIBindingTest;

TEST_F(FeatureCacheTest, Basic) {
  FeatureCache cache;
  scoped_refptr<const Extension> extension_a = CreateExtension("a", {});
  scoped_refptr<const Extension> extension_b =
      CreateExtension("b", {"storage"});

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> v8_context_a = MainContext();
  v8::Local<v8::Context> v8_context_b = AddContext();

  auto script_context_a = base::MakeUnique<ScriptContext>(
      v8_context_a, nullptr, extension_a.get(),
      Feature::BLESSED_EXTENSION_CONTEXT, extension_a.get(),
      Feature::BLESSED_EXTENSION_CONTEXT);
  auto script_context_b = base::MakeUnique<ScriptContext>(
      v8_context_b, nullptr, extension_b.get(),
      Feature::BLESSED_EXTENSION_CONTEXT, extension_b.get(),
      Feature::BLESSED_EXTENSION_CONTEXT);

  auto has_feature = [&cache](const std::unique_ptr<ScriptContext>& context,
                              const std::string& feature) {
    return base::ContainsValue(cache.GetAvailableFeatures(context.get()),
                               feature);
  };

  // To start, context a should not have access to storage, but context b
  // should.
  EXPECT_FALSE(has_feature(script_context_a, "storage"));
  EXPECT_TRUE(has_feature(script_context_b, "storage"));

  // Update extension b's permissions and invalidate the cache.
  extension_b->permissions_data()->SetPermissions(
      base::MakeUnique<PermissionSet>(), base::MakeUnique<PermissionSet>());
  cache.InvalidateExtension(extension_b->id());

  // Now, neither context should have storage access.
  EXPECT_FALSE(has_feature(script_context_a, "storage"));
  EXPECT_FALSE(has_feature(script_context_b, "storage"));

  script_context_a->Invalidate();
  script_context_b->Invalidate();
}

// TODO(devlin): It'd be nice to test that the FeatureCache properly handles
// features that are restricted to certain URLs; unfortunately, for that we'd
// need a stubbed out web frame with a given URL, and there's no good way to do
// that outside Blink.

}  // namespace extensions
