// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/simple_feature.h"

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome::VersionInfo;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::Feature;
using extensions::ListBuilder;
using extensions::Manifest;
using extensions::ScopedCurrentChannel;
using extensions::SimpleFeature;

namespace {

struct IsAvailableTestData {
  std::string extension_id;
  Manifest::Type extension_type;
  Feature::Location location;
  Feature::Platform platform;
  int manifest_version;
  Feature::AvailabilityResult expected_result;
};

class ExtensionSimpleFeatureTest : public testing::Test {
 protected:
  ExtensionSimpleFeatureTest()
      : current_channel_(VersionInfo::CHANNEL_UNKNOWN) {}
  virtual ~ExtensionSimpleFeatureTest() {}

 private:
  ScopedCurrentChannel current_channel_;
};

TEST_F(ExtensionSimpleFeatureTest, IsAvailableNullCase) {
  const IsAvailableTestData tests[] = {
    { "", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "random-extension", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Manifest::TYPE_LEGACY_PACKAGED_APP,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Manifest::TYPE_UNKNOWN,
      Feature::COMPONENT_LOCATION, Feature::UNSPECIFIED_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::CHROMEOS_PLATFORM, -1,
      Feature::IS_AVAILABLE },
    { "", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, Feature::UNSPECIFIED_PLATFORM, 25,
      Feature::IS_AVAILABLE }
  };

  SimpleFeature feature;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    const IsAvailableTestData& test = tests[i];
    EXPECT_EQ(test.expected_result,
              feature.IsAvailableToManifest(test.extension_id,
                                            test.extension_type,
                                            test.location,
                                            test.manifest_version,
                                            test.platform).result());
  }
}

TEST_F(ExtensionSimpleFeatureTest, Whitelist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBaz("bazabbbbccccddddeeeeffffgggghhhh");
  SimpleFeature feature;
  feature.whitelist()->insert(kIdFoo);
  feature.whitelist()->insert(kIdBar);

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      kIdFoo, Manifest::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      kIdBar, Manifest::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      kIdBaz, Manifest::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::NOT_FOUND_IN_WHITELIST,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  feature.extension_types()->insert(Manifest::TYPE_LEGACY_PACKAGED_APP);
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      kIdBaz, Manifest::TYPE_LEGACY_PACKAGED_APP,
      Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, HashedIdWhitelist) {
  // echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
  //   sha1sum | tr '[:lower:]' '[:upper:]'
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdFooHashed("55BC7228A0D502A2A48C9BB16B07062A01E62897");
  SimpleFeature feature;

  feature.whitelist()->insert(kIdFooHashed);

  EXPECT_EQ(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      kIdFoo, Manifest::TYPE_UNKNOWN, Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_NE(Feature::IS_AVAILABLE, feature.IsAvailableToManifest(
      kIdFooHashed, Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      "slightlytoooolongforanextensionid", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToManifest(
      "tooshortforanextensionid", Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION, -1,
      Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, PackageType) {
  SimpleFeature feature;
  feature.extension_types()->insert(Manifest::TYPE_EXTENSION);
  feature.extension_types()->insert(Manifest::TYPE_LEGACY_PACKAGED_APP);

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_EXTENSION,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_LEGACY_PACKAGED_APP,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_THEME,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, Context) {
  SimpleFeature feature;
  feature.set_name("somefeature");
  feature.GetContexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.extension_types()->insert(Manifest::TYPE_LEGACY_PACKAGED_APP);
  feature.platforms()->insert(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(21);
  feature.set_max_manifest_version(25);

  base::DictionaryValue manifest;
  manifest.SetString("name", "test");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 21);
  manifest.SetString("app.launch.local_path", "foo.html");

  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, manifest, Extension::NO_FLAGS,
      &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(extension.get());

  feature.whitelist()->insert("monkey");
  EXPECT_EQ(Feature::NOT_FOUND_IN_WHITELIST, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.whitelist()->clear();

  feature.extension_types()->clear();
  feature.extension_types()->insert(Manifest::TYPE_THEME);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_TYPE, availability.result());
    EXPECT_EQ("'somefeature' is only allowed for themes, "
              "but this is a legacy packaged app.",
              availability.message());
  }

  feature.extension_types()->clear();
  feature.extension_types()->insert(Manifest::TYPE_LEGACY_PACKAGED_APP);
  feature.GetContexts()->clear();
  feature.GetContexts()->insert(Feature::UNBLESSED_EXTENSION_CONTEXT);
  feature.GetContexts()->insert(Feature::CONTENT_SCRIPT_CONTEXT);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_CONTEXT, availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes and "
              "content scripts, but this is a privileged page",
              availability.message());
  }

  feature.GetContexts()->insert(Feature::WEB_PAGE_CONTEXT);
  {
    Feature::Availability availability = feature.IsAvailableToContext(
        extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
        Feature::CHROMEOS_PLATFORM);
    EXPECT_EQ(Feature::INVALID_CONTEXT, availability.result());
    EXPECT_EQ("'somefeature' is only allowed to run in extension iframes, "
              "content scripts, and web pages, but this is a privileged page",
              availability.message());
  }

  feature.GetContexts()->clear();
  feature.GetContexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.set_location(Feature::COMPONENT_LOCATION);
  EXPECT_EQ(Feature::INVALID_LOCATION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_location(Feature::UNSPECIFIED_LOCATION);

  EXPECT_EQ(Feature::INVALID_PLATFORM, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::UNSPECIFIED_PLATFORM).result());

  feature.set_min_manifest_version(22);
  EXPECT_EQ(Feature::INVALID_MIN_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_min_manifest_version(21);

  feature.set_max_manifest_version(18);
  EXPECT_EQ(Feature::INVALID_MAX_MANIFEST_VERSION, feature.IsAvailableToContext(
      extension.get(), Feature::BLESSED_EXTENSION_CONTEXT,
      Feature::CHROMEOS_PLATFORM).result());
  feature.set_max_manifest_version(25);
}

TEST_F(ExtensionSimpleFeatureTest, Location) {
  SimpleFeature feature;

  // If the feature specifies "component" as its location, then only component
  // extensions can access it.
  feature.set_location(Feature::COMPONENT_LOCATION);
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::COMPONENT_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_LOCATION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  // But component extensions can access anything else, whatever their location.
  feature.set_location(Feature::UNSPECIFIED_LOCATION);
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::COMPONENT_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, Platform) {
  SimpleFeature feature;
  feature.platforms()->insert(Feature::CHROMEOS_PLATFORM);
  EXPECT_EQ(Feature::IS_AVAILABLE,
            feature.IsAvailableToManifest(std::string(),
                                          Manifest::TYPE_UNKNOWN,
                                          Feature::UNSPECIFIED_LOCATION,
                                          -1,
                                          Feature::CHROMEOS_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_PLATFORM,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    -1,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, Version) {
  SimpleFeature feature;
  feature.set_min_manifest_version(5);

  EXPECT_EQ(
      Feature::INVALID_MIN_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    0,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::INVALID_MIN_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    4,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    5,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    10,
                                    Feature::UNSPECIFIED_PLATFORM).result());

  feature.set_max_manifest_version(8);

  EXPECT_EQ(
      Feature::INVALID_MAX_MANIFEST_VERSION,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    10,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    8,
                                    Feature::UNSPECIFIED_PLATFORM).result());
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature.IsAvailableToManifest(std::string(),
                                    Manifest::TYPE_UNKNOWN,
                                    Feature::UNSPECIFIED_LOCATION,
                                    7,
                                    Feature::UNSPECIFIED_PLATFORM).result());
}

TEST_F(ExtensionSimpleFeatureTest, ParseNull) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_TRUE(feature->whitelist()->empty());
  EXPECT_TRUE(feature->extension_types()->empty());
  EXPECT_TRUE(feature->GetContexts()->empty());
  EXPECT_EQ(Feature::UNSPECIFIED_LOCATION, feature->location());
  EXPECT_TRUE(feature->platforms()->empty());
  EXPECT_EQ(0, feature->min_manifest_version());
  EXPECT_EQ(0, feature->max_manifest_version());
}

TEST_F(ExtensionSimpleFeatureTest, ParseWhitelist) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  base::ListValue* whitelist = new base::ListValue();
  whitelist->Append(new base::StringValue("foo"));
  whitelist->Append(new base::StringValue("bar"));
  value->Set("whitelist", whitelist);
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_EQ(2u, feature->whitelist()->size());
  EXPECT_TRUE(feature->whitelist()->count("foo"));
  EXPECT_TRUE(feature->whitelist()->count("bar"));
}

TEST_F(ExtensionSimpleFeatureTest, ParsePackageTypes) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  base::ListValue* extension_types = new base::ListValue();
  extension_types->Append(new base::StringValue("extension"));
  extension_types->Append(new base::StringValue("theme"));
  extension_types->Append(new base::StringValue("legacy_packaged_app"));
  extension_types->Append(new base::StringValue("hosted_app"));
  extension_types->Append(new base::StringValue("platform_app"));
  extension_types->Append(new base::StringValue("shared_module"));
  value->Set("extension_types", extension_types);
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_EQ(6u, feature->extension_types()->size());
  EXPECT_TRUE(feature->extension_types()->count(Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(feature->extension_types()->count(Manifest::TYPE_THEME));
  EXPECT_TRUE(feature->extension_types()->count(
      Manifest::TYPE_LEGACY_PACKAGED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Manifest::TYPE_HOSTED_APP));
  EXPECT_TRUE(feature->extension_types()->count(Manifest::TYPE_PLATFORM_APP));
  EXPECT_TRUE(feature->extension_types()->count(Manifest::TYPE_SHARED_MODULE));

  value->SetString("extension_types", "all");
  scoped_ptr<SimpleFeature> feature2(new SimpleFeature());
  feature2->Parse(value.get());
  EXPECT_EQ(*(feature->extension_types()), *(feature2->extension_types()));
}

TEST_F(ExtensionSimpleFeatureTest, ParseContexts) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  base::ListValue* contexts = new base::ListValue();
  contexts->Append(new base::StringValue("blessed_extension"));
  contexts->Append(new base::StringValue("unblessed_extension"));
  contexts->Append(new base::StringValue("content_script"));
  contexts->Append(new base::StringValue("web_page"));
  contexts->Append(new base::StringValue("blessed_web_page"));
  value->Set("contexts", contexts);
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_EQ(5u, feature->GetContexts()->size());
  EXPECT_TRUE(
      feature->GetContexts()->count(Feature::BLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(
      feature->GetContexts()->count(Feature::UNBLESSED_EXTENSION_CONTEXT));
  EXPECT_TRUE(
      feature->GetContexts()->count(Feature::CONTENT_SCRIPT_CONTEXT));
  EXPECT_TRUE(
      feature->GetContexts()->count(Feature::WEB_PAGE_CONTEXT));
  EXPECT_TRUE(
      feature->GetContexts()->count(Feature::BLESSED_WEB_PAGE_CONTEXT));

  value->SetString("contexts", "all");
  scoped_ptr<SimpleFeature> feature2(new SimpleFeature());
  feature2->Parse(value.get());
  EXPECT_EQ(*(feature->GetContexts()), *(feature2->GetContexts()));
}

TEST_F(ExtensionSimpleFeatureTest, ParseLocation) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("location", "component");
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_EQ(Feature::COMPONENT_LOCATION, feature->location());
}

TEST_F(ExtensionSimpleFeatureTest, ParsePlatforms) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  base::ListValue* platforms = new base::ListValue();
  value->Set("platforms", platforms);
  feature->Parse(value.get());
  EXPECT_TRUE(feature->platforms()->empty());

  platforms->AppendString("chromeos");
  feature->Parse(value.get());
  EXPECT_FALSE(feature->platforms()->empty());
  EXPECT_EQ(Feature::CHROMEOS_PLATFORM, *feature->platforms()->begin());

  platforms->Clear();
  platforms->AppendString("win");
  feature->Parse(value.get());
  EXPECT_FALSE(feature->platforms()->empty());
  EXPECT_EQ(Feature::WIN_PLATFORM, *feature->platforms()->begin());

  platforms->Clear();
  platforms->AppendString("win");
  platforms->AppendString("chromeos");
  feature->Parse(value.get());
  std::set<Feature::Platform> expected_platforms;
  expected_platforms.insert(Feature::CHROMEOS_PLATFORM);
  expected_platforms.insert(Feature::WIN_PLATFORM);

  EXPECT_FALSE(feature->platforms()->empty());
  EXPECT_EQ(expected_platforms, *feature->platforms());
}

TEST_F(ExtensionSimpleFeatureTest, ManifestVersion) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("min_manifest_version", 1);
  value->SetInteger("max_manifest_version", 5);
  scoped_ptr<SimpleFeature> feature(new SimpleFeature());
  feature->Parse(value.get());
  EXPECT_EQ(1, feature->min_manifest_version());
  EXPECT_EQ(5, feature->max_manifest_version());
}

TEST_F(ExtensionSimpleFeatureTest, Inheritance) {
  SimpleFeature feature;
  feature.whitelist()->insert("foo");
  feature.extension_types()->insert(Manifest::TYPE_THEME);
  feature.GetContexts()->insert(Feature::BLESSED_EXTENSION_CONTEXT);
  feature.set_location(Feature::COMPONENT_LOCATION);
  feature.platforms()->insert(Feature::CHROMEOS_PLATFORM);
  feature.set_min_manifest_version(1);
  feature.set_max_manifest_version(2);

  // Test additive parsing. Parsing an empty dictionary should result in no
  // changes to a SimpleFeature.
  base::DictionaryValue definition;
  feature.Parse(&definition);
  EXPECT_EQ(1u, feature.whitelist()->size());
  EXPECT_EQ(1u, feature.extension_types()->size());
  EXPECT_EQ(1u, feature.GetContexts()->size());
  EXPECT_EQ(1u, feature.whitelist()->count("foo"));
  EXPECT_EQ(Feature::COMPONENT_LOCATION, feature.location());
  EXPECT_EQ(1u, feature.platforms()->size());
  EXPECT_EQ(1u, feature.platforms()->count(Feature::CHROMEOS_PLATFORM));
  EXPECT_EQ(1, feature.min_manifest_version());
  EXPECT_EQ(2, feature.max_manifest_version());

  base::ListValue* whitelist = new base::ListValue();
  base::ListValue* extension_types = new base::ListValue();
  base::ListValue* contexts = new base::ListValue();
  whitelist->Append(new base::StringValue("bar"));
  extension_types->Append(new base::StringValue("extension"));
  contexts->Append(new base::StringValue("unblessed_extension"));
  definition.Set("whitelist", whitelist);
  definition.Set("extension_types", extension_types);
  definition.Set("contexts", contexts);
  // Can't test location or platform because we only have one value so far.
  definition.Set("min_manifest_version", new base::FundamentalValue(2));
  definition.Set("max_manifest_version", new base::FundamentalValue(3));

  feature.Parse(&definition);
  EXPECT_EQ(1u, feature.whitelist()->size());
  EXPECT_EQ(1u, feature.extension_types()->size());
  EXPECT_EQ(1u, feature.GetContexts()->size());
  EXPECT_EQ(1u, feature.whitelist()->count("bar"));
  EXPECT_EQ(1u, feature.extension_types()->count(Manifest::TYPE_EXTENSION));
  EXPECT_EQ(1u,
            feature.GetContexts()->count(Feature::UNBLESSED_EXTENSION_CONTEXT));
  EXPECT_EQ(2, feature.min_manifest_version());
  EXPECT_EQ(3, feature.max_manifest_version());
}

Feature::AvailabilityResult IsAvailableInChannel(
    const std::string& channel, VersionInfo::Channel channel_for_testing) {
  ScopedCurrentChannel current_channel(channel_for_testing);

  SimpleFeature feature;
  feature.AddFilter(scoped_ptr<extensions::SimpleFeatureFilter>(
      new extensions::ChromeChannelFeatureFilter(&feature)));
  if (!channel.empty()) {
    base::DictionaryValue feature_value;
    feature_value.SetString("channel", channel);
    feature.Parse(&feature_value);
  }

  return feature.IsAvailableToManifest(
      "random-extension",
      Manifest::TYPE_UNKNOWN,
      Feature::UNSPECIFIED_LOCATION,
      -1,
      Feature::GetCurrentPlatform()).result();
}

TEST_F(ExtensionSimpleFeatureTest, SupportedChannel) {
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
}

}  // namespace
