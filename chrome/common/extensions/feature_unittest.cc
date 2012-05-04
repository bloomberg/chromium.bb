// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/feature.h"

#include "testing/gtest/include/gtest/gtest.h"

using chrome::VersionInfo;
using extensions::Feature;

namespace {

struct IsAvailableTestData {
  std::string extension_id;
  Extension::Type extension_type;
  Feature::Location location;
  Feature::Platform platform;
  int manifest_version;
  Feature::Availability expected_result;
};

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_IsAvailableNullCase DISABLED_IsAvailableNullCase
#else
#define MAYBE_IsAvailableNullCase IsAvailableNullCase
#endif
TEST(ExtensionFeatureTest, MAYBE_IsAvailableNullCase) {
  const IsAvailableTestData tests[] = {
    { "", Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "random-extension", Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_PACKAGED_APP,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN,
      Feature::COMPONENT_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::CHROMEOS_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, 25,
      Feature::IS_AVAILABLE }
  };

  Feature feature;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    const IsAvailableTestData& test = tests[i];
    EXPECT_EQ(test.expected_result,
              feature.IsAvailableToManifest(test.extension_id,
                                            test.extension_type,
                                            test.location,
                                            test.manifest_version,
                                            test.platform));
  }
}

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Whitelist DISABLED_Whitelist
#else
#define MAYBE_Whitelist Whitelist
#endif
TEST(ExtensionFeatureTest, MAYBE_Whitelist) {
  Feature feature;
  feature.whitelist()->insert("foo");
  feature.whitelist()->insert("bar");

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "foo", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "bar", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));

  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      "baz", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));

  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      "baz", Extension::TYPE_PACKAGED_APP, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
}

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_PackageType DISABLED_PackageType
#else
#define MAYBE_PackageType PackageType
#endif
TEST(ExtensionFeatureTest, MAYBE_PackageType) {
  Feature feature;
  feature.extension_types()->insert(Extension::TYPE_EXTENSION);
  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_EXTENSION, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_PACKAGED_APP, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));

  EXPECT_EQ(Feature::INVALID_TYPE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::INVALID_TYPE, feature.IsAvailableToManifest(
      "", Extension::TYPE_THEME, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
}

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Context DISABLED_Context
#else
#define MAYBE_Context Context
#endif
TEST(ExtensionFeatureTest, MAYBE_Context) {
  Feature feature;
  feature.contexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);
  feature.set_platform(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(21);
  feature.set_max_manifest_version(25);

  DictionaryValue manifest;
  manifest.SetString("name", "test");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 21);
  manifest.SetString("app.launch.local_path", "foo.html");

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      FilePath(), Extension::INTERNAL, manifest, Extension::NO_FLAGS, &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(extension.get());

  feature.whitelist()->insert("monkey");
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.whitelist()->clear();

  feature.extension_types()->clear();
  feature.extension_types()->insert(Extension::TYPE_THEME);
  EXPECT_EQ(Feature::INVALID_TYPE, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.extension_types()->clear();
  feature.extension_types()->insert(Extension::TYPE_PACKAGED_APP);

  feature.contexts()->clear();
  feature.contexts()->insert(Feature::UNBLESSED_EXTENSION_CONTEXT);
  EXPECT_EQ(Feature::INVALID_CONTEXT, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.contexts()->clear();
  feature.contexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);

  feature.set_location(Feature::COMPONENT_LOCATION);
  EXPECT_EQ(Feature::INVALID_LOCATION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.set_location(Feature::UNSPECIFIED_LOCATION);

  EXPECT_EQ(Feature::INVALID_PLATFORM, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::UNSPECIFIED_PLATFORM));

  feature.set_min_manifest_version(22);
  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.set_min_manifest_version(21);

  feature.set_max_manifest_version(18);
  EXPECT_EQ(Feature::INVALID_MAX_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM));
  feature.set_max_manifest_version(25);
}

TEST(ExtensionFeatureTest, Location) {
  Feature feature;

  // If the feature specifies "component" as its location, then only component
  // extensions can access it.
  feature.set_location(Feature::COMPONENT_LOCATION);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::COMPONENT_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::INVALID_LOCATION, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));

  // But component extensions can access anything else, whatever their location.
  feature.set_location(Feature::UNSPECIFIED_LOCATION);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::COMPONENT_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
}

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Platform DISABLED_Platform
#else
#define MAYBE_Platform Platform
#endif
TEST(ExtensionFeatureTest, MAYBE_Platform) {
  Feature feature;
  feature.set_platform(Feature::CHROMEOS_PLATFORM);
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::CHROMEOS_PLATFORM));
  EXPECT_EQ(Feature::INVALID_PLATFORM, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM));
}

// Fails on official continuous builder - http://crbug.com/126274
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Version DISABLED_Version
#else
#define MAYBE_Version Version
#endif
TEST(ExtensionFeatureTest, MAYBE_Version) {
  Feature feature;
  feature.set_min_manifest_version(5);

  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION,
            feature.IsAvailableToManifest(
                "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
                0, Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION,
            feature.IsAvailableToManifest(
                "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
                4, Feature::UNSPECIFIED_PLATFORM));

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      5, Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      10, Feature::UNSPECIFIED_PLATFORM));

  feature.set_max_manifest_version(8);

  EXPECT_EQ(Feature::INVALID_MAX_MANIFEST_VERSION,
            feature.IsAvailableToManifest(
                "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
                10, Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature.IsAvailableToManifest(
                "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
                8, Feature::UNSPECIFIED_PLATFORM));
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      "", Extension::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION,
      7, Feature::UNSPECIFIED_PLATFORM));
}

TEST(ExtensionFeatureTest, ParseNull) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
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
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
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
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
  EXPECT_EQ(5u, feature->extension_types()->size());
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_EXTENSION));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_THEME));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_PACKAGED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_HOSTED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Extension::TYPE_PLATFORM_APP));

  value->SetString("extension_types", "all");
  scoped_ptr<Feature> feature2(new Feature());
  feature2->Parse(value.get());
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
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
  EXPECT_EQ(4u, feature->contexts()->size());
  EXPECT_TRUE(feature->contexts()->count(Feature::BLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::UNBLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::CONTENT_SCRIPT_CONTEXT));
  EXPECT_TRUE(feature->contexts()->count(Feature::WEB_PAGE_CONTEXT));

  value->SetString("contexts", "all");
  scoped_ptr<Feature> feature2(new Feature());
  feature2->Parse(value.get());
  EXPECT_EQ(*(feature->contexts()), *(feature2->contexts()));
}

TEST(ExtensionFeatureTest, ParseLocation) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("location", "component");
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
  EXPECT_EQ(Feature::COMPONENT_LOCATION, feature->location());
}

TEST(ExtensionFeatureTest, ParsePlatform) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("platform", "chromeos");
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
  EXPECT_EQ(Feature::CHROMEOS_PLATFORM, feature->platform());
}

TEST(ExtensionFeatureTest, ManifestVersion) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetInteger("min_manifest_version", 1);
  value->SetInteger("max_manifest_version", 5);
  scoped_ptr<Feature> feature(new Feature());
  feature->Parse(value.get());
  EXPECT_EQ(1, feature->min_manifest_version());
  EXPECT_EQ(5, feature->max_manifest_version());
}

TEST(ExtensionFeatureTest, Inheritance) {
  Feature feature;
  feature.whitelist()->insert("foo");
  feature.extension_types()->insert(Extension::TYPE_THEME);
  feature.contexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.set_location(Feature::COMPONENT_LOCATION);
  feature.set_platform(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(1);
  feature.set_max_manifest_version(2);

  Feature feature2 = feature;
  EXPECT_TRUE(feature2.Equals(feature));

  DictionaryValue definition;
  feature2.Parse(&definition);
  EXPECT_TRUE(feature2.Equals(feature));

  ListValue* whitelist = new ListValue();
  ListValue* extension_types = new ListValue();
  ListValue* contexts = new ListValue();
  whitelist->Append(Value::CreateStringValue("bar"));
  extension_types->Append(Value::CreateStringValue("extension"));
  contexts->Append(Value::CreateStringValue("unblessed_extension"));
  definition.Set("whitelist", whitelist);
  definition.Set("extension_types", extension_types);
  definition.Set("contexts", contexts);
  // Can't test location or platform because we only have one value so far.
  definition.Set("min_manifest_version", Value::CreateIntegerValue(2));
  definition.Set("max_manifest_version", Value::CreateIntegerValue(3));

  feature2.Parse(&definition);
  EXPECT_FALSE(feature2.Equals(feature));
  EXPECT_EQ(1u, feature2.whitelist()->size());
  EXPECT_EQ(1u, feature2.extension_types()->size());
  EXPECT_EQ(1u, feature2.contexts()->size());
  EXPECT_EQ(1u, feature2.whitelist()->count("bar"));
  EXPECT_EQ(1u, feature2.extension_types()->count(Extension::TYPE_EXTENSION));
  EXPECT_EQ(1u,
            feature2.contexts()->count(Feature::UNBLESSED_EXTENSION_CONTEXT));
  EXPECT_EQ(2, feature2.min_manifest_version());
  EXPECT_EQ(3, feature2.max_manifest_version());
}

TEST(ExtensionFeatureTest, Equals) {
  Feature feature;
  feature.whitelist()->insert("foo");
  feature.extension_types()->insert(Extension::TYPE_THEME);
  feature.contexts()->insert(Feature::UNBLESSED_EXTENSION_CONTEXT);
  feature.set_location(Feature::COMPONENT_LOCATION);
  feature.set_platform(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(18);
  feature.set_max_manifest_version(25);

  Feature feature2(feature);
  EXPECT_TRUE(feature2.Equals(feature));

  feature2.whitelist()->clear();
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.extension_types()->clear();
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.contexts()->clear();
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.set_location(Feature::UNSPECIFIED_LOCATION);
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.set_platform(Feature::UNSPECIFIED_PLATFORM);
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.set_min_manifest_version(0);
  EXPECT_FALSE(feature2.Equals(feature));

  feature2 = feature;
  feature2.set_max_manifest_version(0);
  EXPECT_FALSE(feature2.Equals(feature));
}

Feature::Availability IsAvailableInChannel(
    const std::string& channel, VersionInfo::Channel channel_for_testing) {
  Feature::SetChannelForTesting(channel_for_testing);

  Feature feature;
  if (!channel.empty()) {
    DictionaryValue feature_value;
    feature_value.SetString("channel", channel);
    feature.Parse(&feature_value);
  }

  return feature.IsAvailableToManifest(
      "random-extension",
      Extension::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION,
      -1);
}

TEST(ExtensionFeatureTest, SupportedChannel) {
  // stable supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("stable", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("stable", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("stable", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("stable", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("stable", VersionInfo::CHANNEL_STABLE));

  // beta supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("beta", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("beta", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("beta", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("beta", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("beta", VersionInfo::CHANNEL_STABLE));

  // dev supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("dev", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("dev", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("dev", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("dev", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("dev", VersionInfo::CHANNEL_STABLE));

  // canary supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("canary", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("canary", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("canary", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("canary", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("canary", VersionInfo::CHANNEL_STABLE));

  // trunk supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("trunk", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("trunk", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("trunk", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("trunk", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("trunk", VersionInfo::CHANNEL_STABLE));

  // Default supported channel (trunk).
  EXPECT_EQ(Feature::IS_AVAILABLE,
      IsAvailableInChannel("", VersionInfo::CHANNEL_UNKNOWN));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("", VersionInfo::CHANNEL_CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("", VersionInfo::CHANNEL_DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("", VersionInfo::CHANNEL_BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
      IsAvailableInChannel("", VersionInfo::CHANNEL_STABLE));

  Feature::SetChannelForTesting(VersionInfo::CHANNEL_UNKNOWN);
}

}  // namespace
