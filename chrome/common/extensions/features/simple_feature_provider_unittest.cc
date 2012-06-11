// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/simple_feature_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::Feature;
using extensions::SimpleFeatureProvider;

TEST(SimpleFeatureProvider, ManifestFeatures) {
  SimpleFeatureProvider* provider =
      SimpleFeatureProvider::GetManifestFeatures();
  Feature* feature = provider->GetFeature("description");
  ASSERT_TRUE(feature);
  EXPECT_EQ(5u, feature->extension_types()->size());
  EXPECT_EQ(1u, feature->extension_types()->count(Extension::TYPE_EXTENSION));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Extension::TYPE_PACKAGED_APP));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Extension::TYPE_PLATFORM_APP));
  EXPECT_EQ(1u, feature->extension_types()->count(Extension::TYPE_HOSTED_APP));
  EXPECT_EQ(1u, feature->extension_types()->count(Extension::TYPE_THEME));

  DictionaryValue manifest;
  manifest.SetString("name", "test extension");
  manifest.SetString("version", "1");
  manifest.SetString("description", "hello there");

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      FilePath(), Extension::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(Feature::IS_AVAILABLE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));

  feature = provider->GetFeature("theme");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::INVALID_TYPE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));

  feature = provider->GetFeature("devtools_page");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));
}

TEST(SimpleFeatureProvider, PermissionFeatures) {
  SimpleFeatureProvider* provider =
      SimpleFeatureProvider::GetPermissionFeatures();
  Feature* feature = provider->GetFeature("browsingData");
  ASSERT_TRUE(feature);
  EXPECT_EQ(3u, feature->extension_types()->size());
  EXPECT_EQ(1u, feature->extension_types()->count(Extension::TYPE_EXTENSION));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Extension::TYPE_PACKAGED_APP));
  EXPECT_EQ(1u,
            feature->extension_types()->count(Extension::TYPE_PLATFORM_APP));

  DictionaryValue manifest;
  manifest.SetString("name", "test extension");
  manifest.SetString("version", "1");
  ListValue* permissions = new ListValue();
  manifest.Set("permissions", permissions);
  permissions->Append(Value::CreateStringValue("browsingData"));

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      FilePath(), Extension::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(Feature::IS_AVAILABLE, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));

  feature = provider->GetFeature("chromePrivate");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));

  feature = provider->GetFeature("clipboardWrite");
  ASSERT_TRUE(feature);
  EXPECT_EQ(Feature::NOT_PRESENT, feature->IsAvailableToContext(
      extension.get(), Feature::UNSPECIFIED_CONTEXT));
}

TEST(SimpleFeatureProvider, Validation) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  DictionaryValue* feature1 = new DictionaryValue();
  value->Set("feature1", feature1);

  DictionaryValue* feature2 = new DictionaryValue();
  ListValue* extension_types = new ListValue();
  extension_types->Append(Value::CreateStringValue("extension"));
  feature2->Set("extension_types", extension_types);
  ListValue* contexts = new ListValue();
  contexts->Append(Value::CreateStringValue("blessed_extension"));
  feature2->Set("contexts", contexts);
  value->Set("feature2", feature2);

  scoped_ptr<SimpleFeatureProvider> provider(
      new SimpleFeatureProvider(value.get(), NULL));

  // feature1 won't validate because it lacks an extension type.
  EXPECT_FALSE(provider->GetFeature("feature1"));

  // If we add one, it works.
  feature1->Set("extension_types", extension_types->DeepCopy());
  provider.reset(new SimpleFeatureProvider(value.get(), NULL));
  EXPECT_TRUE(provider->GetFeature("feature1"));

  // feature2 won't validate because of the presence of "contexts".
  EXPECT_FALSE(provider->GetFeature("feature2"));

  // If we remove it, it works.
  feature2->Remove("contexts", NULL);
  provider.reset(new SimpleFeatureProvider(value.get(), NULL));
  EXPECT_TRUE(provider->GetFeature("feature2"));
}
