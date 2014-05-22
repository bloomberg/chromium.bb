// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/base_feature_provider.h"

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome::VersionInfo;

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  SimpleFeature* feature = new FeatureClass();
  feature->AddFilter(
      scoped_ptr<SimpleFeatureFilter>(new ChromeChannelFeatureFilter(feature)));
  return feature;
}

}  // namespace

TEST(BaseFeatureProviderTest, ManifestFeatures) {
  const FeatureProvider* provider = BaseFeatureProvider::GetByName("manifest");
  SimpleFeature* feature =
      static_cast<SimpleFeature*>(provider->GetFeature("description"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(6u, feature->extension_types()->size());
  EXPECT_EQ(1u, feature->extension_types()->count(Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1u,
      feature->extension_types()->count(Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Manifest::TYPE_PLATFORM_APP));
  EXPECT_EQ(1u, feature->extension_types()->count(Manifest::TYPE_HOSTED_APP));
  EXPECT_EQ(1u, feature->extension_types()->count(Manifest::TYPE_THEME));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Manifest::TYPE_SHARED_MODULE));

  base::DictionaryValue manifest;
  manifest.SetString("name", "test extension");
  manifest.SetString("version", "1");
  manifest.SetString("description", "hello there");

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(Feature::IS_AVAILABLE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());

  feature =
    static_cast<SimpleFeature*>(provider->GetFeature("theme"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::INVALID_TYPE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());

  feature =
    static_cast<SimpleFeature*>(provider->GetFeature("devtools_page"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());
}

TEST(BaseFeatureProviderTest, PermissionFeatures) {
  const FeatureProvider* provider =
      BaseFeatureProvider::GetByName("permission");
  SimpleFeature* feature =
      static_cast<SimpleFeature*>(provider->GetFeature("contextMenus"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(3u, feature->extension_types()->size());
  EXPECT_EQ(1u, feature->extension_types()->count(Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1u,
      feature->extension_types()->count(Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Manifest::TYPE_PLATFORM_APP));

  base::DictionaryValue manifest;
  manifest.SetString("name", "test extension");
  manifest.SetString("version", "1");
  base::ListValue* permissions = new base::ListValue();
  manifest.Set("permissions", permissions);
  permissions->Append(new base::StringValue("contextMenus"));

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(Feature::IS_AVAILABLE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());

  feature =
    static_cast<SimpleFeature*>(provider->GetFeature("chromePrivate"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());

  feature =
    static_cast<SimpleFeature*>(provider->GetFeature("clipboardWrite"));
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT).result());
}

TEST(BaseFeatureProviderTest, Validation) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  base::DictionaryValue* feature1 = new base::DictionaryValue();
  feature1->SetString("channel", "trunk");
  value->Set("feature1", feature1);

  base::DictionaryValue* feature2 = new base::DictionaryValue();
  feature2->SetString("channel", "trunk");
  base::ListValue* extension_types = new base::ListValue();
  extension_types->Append(new base::StringValue("extension"));
  feature2->Set("extension_types", extension_types);
  base::ListValue* contexts = new base::ListValue();
  contexts->Append(new base::StringValue("blessed_extension"));
  feature2->Set("contexts", contexts);
  value->Set("feature2", feature2);

  scoped_ptr<BaseFeatureProvider> provider(
      new BaseFeatureProvider(*value, CreateFeature<PermissionFeature>));

  // feature1 won't validate because it lacks an extension type.
  EXPECT_FALSE(provider->GetFeature("feature1"));

  // If we add one, it works.
  feature1->Set("extension_types", extension_types->DeepCopy());
  provider.reset(
      new BaseFeatureProvider(*value, CreateFeature<PermissionFeature>));
  EXPECT_TRUE(provider->GetFeature("feature1"));

  // Remove the channel, and feature1 won't validate.
  feature1->Remove("channel", NULL);
  provider.reset(
      new BaseFeatureProvider(*value, CreateFeature<PermissionFeature>));
  EXPECT_FALSE(provider->GetFeature("feature1"));

  // feature2 won't validate because of the presence of "contexts".
  EXPECT_FALSE(provider->GetFeature("feature2"));

  // If we remove it, it works.
  feature2->Remove("contexts", NULL);
  provider.reset(
      new BaseFeatureProvider(*value, CreateFeature<PermissionFeature>));
  EXPECT_TRUE(provider->GetFeature("feature2"));
}

TEST(BaseFeatureProviderTest, ComplexFeatures) {
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
      .Set("feature1", ListBuilder()
          .Append(DictionaryBuilder()
              .Set("channel", "beta")
              .Set("extension_types", ListBuilder()
                  .Append("extension")))
                  .Append(DictionaryBuilder()
                      .Set("channel", "beta")
                      .Set("extension_types", ListBuilder()
                          .Append("legacy_packaged_app"))))
      .Build());

  scoped_ptr<BaseFeatureProvider> provider(
      new BaseFeatureProvider(*rule, CreateFeature<SimpleFeature>));

  Feature* feature = provider->GetFeature("feature1");
  EXPECT_TRUE(feature);

  // Make sure both rules are applied correctly.
  {
    ScopedCurrentChannel current_channel(VersionInfo::CHANNEL_BETA);
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("2",
                                       Manifest::TYPE_LEGACY_PACKAGED_APP,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
  }
  {
    ScopedCurrentChannel current_channel(VersionInfo::CHANNEL_STABLE);
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("2",
                                       Manifest::TYPE_LEGACY_PACKAGED_APP,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM).result());
  }
}

}  // namespace extensions
