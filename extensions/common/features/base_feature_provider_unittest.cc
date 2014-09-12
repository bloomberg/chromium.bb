// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/base_feature_provider.h"

#include <set>
#include <string>

#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that a real manifest feature is available for the correct types of
// extensions and apps.
TEST(BaseFeatureProviderTest, ManifestFeatureTypes) {
  const FeatureProvider* provider = BaseFeatureProvider::GetByName("manifest");
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  SimpleFeature* feature =
      static_cast<SimpleFeature*>(provider->GetFeature("description"));
  ASSERT_TRUE(feature);
  std::set<Manifest::Type>* extension_types = feature->extension_types();
  EXPECT_EQ(6u, extension_types->size());
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_PLATFORM_APP));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_HOSTED_APP));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_THEME));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_SHARED_MODULE));
}

// Tests that real manifest features have the correct availability for an
// extension.
TEST(BaseFeatureProviderTest, ManifestFeatureAvailability) {
  const FeatureProvider* provider = BaseFeatureProvider::GetByName("manifest");

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "test extension")
                           .Set("version", "1")
                           .Set("description", "hello there"))
          .Build();
  ASSERT_TRUE(extension.get());

  Feature* feature = provider->GetFeature("description");
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature->IsAvailableToContext(extension.get(),
                                          Feature::UNSPECIFIED_CONTEXT,
                                          GURL()).result());

  // This is a generic extension, so an app-only feature isn't allowed.
  feature = provider->GetFeature("app.background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::INVALID_TYPE,
            feature->IsAvailableToContext(extension.get(),
                                          Feature::UNSPECIFIED_CONTEXT,
                                          GURL()).result());

  // A feature not listed in the manifest isn't allowed.
  feature = provider->GetFeature("background");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT,
            feature->IsAvailableToContext(extension.get(),
                                          Feature::UNSPECIFIED_CONTEXT,
                                          GURL()).result());
}

// Tests that a real permission feature is available for the correct types of
// extensions and apps.
TEST(BaseFeatureProviderTest, PermissionFeatureTypes) {
  const FeatureProvider* provider =
      BaseFeatureProvider::GetByName("permission");
  // NOTE: This feature cannot have multiple rules, otherwise it is not a
  // SimpleFeature.
  SimpleFeature* feature =
      static_cast<SimpleFeature*>(provider->GetFeature("power"));
  ASSERT_TRUE(feature);
  std::set<Manifest::Type>* extension_types = feature->extension_types();
  EXPECT_EQ(3u, extension_types->size());
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1u, extension_types->count(Manifest::TYPE_PLATFORM_APP));
}

// Tests that real permission features have the correct availability for an app.
TEST(BaseFeatureProviderTest, PermissionFeatureAvailability) {
  const FeatureProvider* provider =
      BaseFeatureProvider::GetByName("permission");

  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "test app")
                           .Set("version", "1")
                           .Set("app",
                                DictionaryBuilder().Set(
                                    "background",
                                    DictionaryBuilder().Set(
                                        "scripts",
                                        ListBuilder().Append("background.js"))))
                           .Set("permissions", ListBuilder().Append("power")))
          .Build();
  ASSERT_TRUE(app.get());
  ASSERT_TRUE(app->is_platform_app());

  // A permission requested in the manifest is available.
  Feature* feature = provider->GetFeature("power");
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToContext(
                   app.get(), Feature::UNSPECIFIED_CONTEXT, GURL()).result());

  // A permission only available to whitelisted extensions returns availability
  // NOT_FOUND_IN_WHITELIST.
  feature = provider->GetFeature("bluetoothPrivate");
  ASSERT_TRUE(feature);
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature->IsAvailableToContext(
                   app.get(), Feature::UNSPECIFIED_CONTEXT, GURL()).result());

  // A permission that isn't part of the manifest returns NOT_PRESENT.
  feature = provider->GetFeature("serial");
  ASSERT_TRUE(feature);
  EXPECT_EQ(
      Feature::NOT_PRESENT,
      feature->IsAvailableToContext(
                   app.get(), Feature::UNSPECIFIED_CONTEXT, GURL()).result());
}

}  // namespace extensions
