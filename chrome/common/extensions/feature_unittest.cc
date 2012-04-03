// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/feature.h"

#include "testing/gtest/include/gtest/gtest.h"

using extensions::Feature;

namespace {

struct IsAvailableTestData {
  std::string extension_id;
  Extension::Type extension_type;
  Feature::Context context;
  Feature::Location location;
  Feature::Platform platform;
  int manifest_version;
  Feature::Availability expected_result;
};

}  // namespace

TEST(ExtensionFeatureTest, IsAvailableNullCase) {
  const IsAvailableTestData tests[] = {
    { "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "random-extension", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_PACKAGED_APP, Feature::UNSPECIFIED_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN, Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_CONTEXT,
      Feature::COMPONENT_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::CHROMEOS_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_CONTEXT,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, 25,
      Feature::IS_AVAILABLE }
  };

  Feature feature;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    const IsAvailableTestData& test = tests[i];
    EXPECT_EQ(test.expected_result,
              feature.IsAvailable(test.extension_id, test.extension_type,
                                  test.location, test.context, test.platform,
                                  test.manifest_version));
  }
}

TEST(ExtensionFeatureTest, Whitelist) {
  Feature feature;
  feature.whitelist()->insert("foo");
  feature.whitelist()->insert("bar");

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "foo", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "bar", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));

  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailable(
      "baz", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));

  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailable(
      "baz", Extension::TYPE_PACKAGED_APP, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
}

TEST(ExtensionFeatureTest, PackageType) {
  Feature feature;
  feature.extension_types()->insert(Extension::TYPE_EXTENSION);
  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_EXTENSION, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_PACKAGED_APP, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));

  EXPECT_EQ(Feature::INVALID_TYPE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::INVALID_TYPE, feature.IsAvailable(
      "", Extension::TYPE_THEME, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
}

TEST(ExtensionFeatureTest, Context) {
  Feature feature;
  feature.contexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.contexts()->insert(Feature::CONTENT_SCRIPT_CONTEXT);

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::BLESSED_EXTENSION_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::CONTENT_SCRIPT_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));

  EXPECT_EQ(Feature::INVALID_CONTEXT, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNBLESSED_EXTENSION_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::INVALID_CONTEXT, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
}

TEST(ExtensionFeatureTest, Location) {
  Feature feature;

  // If the feature specifies "component" as its location, then only component
  // extensions can access it.
  feature.set_location(Feature::COMPONENT_LOCATION);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::COMPONENT_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
  EXPECT_EQ(Feature::INVALID_LOCATION, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));

  // But component extensions can access anything else, whatever their location.
  feature.set_location(Feature::UNSPECIFIED_LOCATION);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::COMPONENT_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
}

TEST(ExtensionFeatureTest, Platform) {
  Feature feature;
  feature.set_platform(Feature::CHROMEOS_PLATFORM);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::CHROMEOS_PLATFORM, -1));
  EXPECT_EQ(Feature::INVALID_PLATFORM, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, -1));
}

TEST(ExtensionFeatureTest, Version) {
  Feature feature;
  feature.set_min_manifest_version(5);

  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 0));
  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 4));

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 5));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 10));

  feature.set_max_manifest_version(8);

  EXPECT_EQ(Feature::INVALID_MAX_MANIFEST_VERSION, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 10));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 8));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailable(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      Feature::UNSPECIFIED_CONTEXT, Feature::UNSPECIFIED_PLATFORM, 7));
}

TEST(ExtensionFeatureTest, ParseNull) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_TRUE(feature->whitelist()->empty());
  EXPECT_TRUE(feature->extension_types()->empty());
  EXPECT_TRUE(feature->contexts()->empty());
  EXPECT_EQ(Feature::UNSPECIFIED_LOCATION, feature->location());
  EXPECT_EQ(Feature::UNSPECIFIED_PLATFORM, feature->platform());
  EXPECT_EQ(0, feature->min_manifest_version());
  EXPECT_EQ(0, feature->max_manifest_version());
}

TEST(ExtensionFeatureTest, ParseWhitelist) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  ListValue* whitelist = new ListValue();
  whitelist->Append(Value::CreateStringValue("foo"));
  whitelist->Append(Value::CreateStringValue("bar"));
  value->Set("whitelist", whitelist);
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(2u, feature->whitelist()->size());
  EXPECT_TRUE(feature->whitelist()->count("foo"));
  EXPECT_TRUE(feature->whitelist()->count("bar"));
}

TEST(ExtensionFeatureTest, ParsePackageTypes) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  ListValue* extension_types = new ListValue();
  extension_types->Append(Value::CreateStringValue("extension"));
  extension_types->Append(Value::CreateStringValue("theme"));
  extension_types->Append(Value::CreateStringValue("packaged_app"));
  extension_types->Append(Value::CreateStringValue("hosted_app"));
  extension_types->Append(Value::CreateStringValue("platform_app"));
  value->Set("extension_types", extension_types);
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(5u, feature->extension_types()->size());
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_EXTENSION));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_THEME));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_PACKAGED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_HOSTED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_PLATFORM_APP));

  value->SetString("extension_types", "all");
  scoped_ptr<Feature> feature2(Feature::Parse(value.get()));
  EXPECT_EQ(*(feature->extension_types()), *(feature2->extension_types()));
}

TEST(ExtensionFeatureTest, ParseContexts) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  ListValue* contexts = new ListValue();
  contexts->Append(Value::CreateStringValue("blessed_extension"));
  contexts->Append(Value::CreateStringValue("unblessed_extension"));
  contexts->Append(Value::CreateStringValue("content_script"));
  contexts->Append(Value::CreateStringValue("web_page"));
  value->Set("contexts", contexts);
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(4u, feature->contexts()->size());
  EXPECT_TRUE(feature->contexts()->count(Feature::BLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::UNBLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::CONTENT_SCRIPT_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::WEB_PAGE_CONTEXT));

  value->SetString("contexts", "all");
  scoped_ptr<Feature> feature2(Feature::Parse(value.get()));
  EXPECT_EQ(*(feature->contexts()), *(feature2->contexts()));
}

TEST(ExtensionFeatureTest, ParseLocation) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("location", "component");
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(Feature::COMPONENT_LOCATION, feature->location());
}

TEST(ExtensionFeatureTest, ParsePlatform) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("platform", "chromeos");
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(Feature::CHROMEOS_PLATFORM, feature->platform());
}

TEST(ExtensionFeatureTest, ManifestVersion) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetInteger("min_manifest_version", 1);
  value->SetInteger("max_manifest_version", 5);
  scoped_ptr<Feature> feature(Feature::Parse(value.get()));
  EXPECT_EQ(1, feature->min_manifest_version());
  EXPECT_EQ(5, feature->max_manifest_version());
}
