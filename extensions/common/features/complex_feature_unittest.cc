// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome::VersionInfo;
using extensions::APIFeature;
using extensions::ComplexFeature;
using extensions::DictionaryBuilder;
using extensions::Feature;
using extensions::ListBuilder;
using extensions::Manifest;
using extensions::ScopedCurrentChannel;
using extensions::SimpleFeature;
using extensions::test_util::ParseJsonDictionaryWithSingleQuotes;

namespace {

class ExtensionComplexFeatureTest : public testing::Test {
 protected:
  ExtensionComplexFeatureTest()
      : current_channel_(VersionInfo::CHANNEL_UNKNOWN) {}
  virtual ~ExtensionComplexFeatureTest() {}

  SimpleFeature* CreateFeature() {
    SimpleFeature* feature = new SimpleFeature();
    feature->AddFilter(scoped_ptr<extensions::SimpleFeatureFilter>(
        new extensions::ChromeChannelFeatureFilter(feature)));
    return feature;
  }

 private:
  ScopedCurrentChannel current_channel_;
};

TEST_F(ExtensionComplexFeatureTest, MultipleRulesWhitelist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule: "extension", whitelist "foo".
  scoped_ptr<SimpleFeature> simple_feature(CreateFeature());
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
      .Set("whitelist", ListBuilder().Append(kIdFoo))
      .Set("extension_types", ListBuilder()
          .Append("extension")).Build());
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  // Rule: "legacy_packaged_app", whitelist "bar".
  simple_feature.reset(CreateFeature());
  rule = DictionaryBuilder()
      .Set("whitelist", ListBuilder().Append(kIdBar))
      .Set("extension_types", ListBuilder()
          .Append("legacy_packaged_app")).Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  scoped_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));

  // Test match 1st rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdFoo,
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Test match 2nd rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdBar,
                                     Manifest::TYPE_LEGACY_PACKAGED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Test whitelist with wrong extension type.
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdBar,
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
  EXPECT_NE(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest(kIdFoo,
                                     Manifest::TYPE_LEGACY_PACKAGED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
}

TEST_F(ExtensionComplexFeatureTest, MultipleRulesChannels) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule: "extension", channel trunk.
  scoped_ptr<SimpleFeature> simple_feature(CreateFeature());
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
      .Set("channel", "trunk")
      .Set("extension_types", ListBuilder().Append("extension")).Build());
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  // Rule: "legacy_packaged_app", channel stable.
  simple_feature.reset(CreateFeature());
  rule = DictionaryBuilder()
      .Set("channel", "stable")
      .Set("extension_types", ListBuilder()
          .Append("legacy_packaged_app")).Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  scoped_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));

  // Test match 1st rule.
  {
    ScopedCurrentChannel current_channel(VersionInfo::CHANNEL_UNKNOWN);
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform()).result());
  }

  // Test match 2nd rule.
  {
    ScopedCurrentChannel current_channel(VersionInfo::CHANNEL_BETA);
    EXPECT_EQ(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("2",
                                       Manifest::TYPE_LEGACY_PACKAGED_APP,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform()).result());
  }

  // Test feature not available to extensions above channel unknown.
  {
    ScopedCurrentChannel current_channel(VersionInfo::CHANNEL_BETA);
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform()).result());
  }
}

// Tests a complex feature with blocked_in_service_worker returns true for
// IsBlockedInServiceWorker().
TEST_F(ExtensionComplexFeatureTest, BlockedInServiceWorker) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Two feature rules, both with blocked_in_service_worker: true.
  scoped_ptr<SimpleFeature> api_feature(new APIFeature());
  api_feature->Parse(ParseJsonDictionaryWithSingleQuotes(
                         "{"
                         "  'channel': 'trunk',"
                         "  'blocked_in_service_worker': true"
                         "}").get());
  features->push_back(api_feature.release());

  api_feature.reset(new APIFeature());
  api_feature->Parse(ParseJsonDictionaryWithSingleQuotes(
                         "{"
                         "  'channel': 'stable',"
                         "  'blocked_in_service_worker': true"
                         "}").get());
  features->push_back(api_feature.release());

  EXPECT_TRUE(ComplexFeature(features.Pass()).IsBlockedInServiceWorker());
}

// Tests a complex feature without blocked_in_service_worker returns false for
// IsBlockedInServiceWorker().
TEST_F(ExtensionComplexFeatureTest, NotBlockedInServiceWorker) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Two feature rules without blocked_in_service_worker.
  scoped_ptr<SimpleFeature> api_feature(new APIFeature());
  api_feature->Parse(ParseJsonDictionaryWithSingleQuotes(
                         "{"
                         "  'channel': 'trunk'"
                         "}").get());
  features->push_back(api_feature.release());

  api_feature.reset(new APIFeature());
  api_feature->Parse(ParseJsonDictionaryWithSingleQuotes(
                         "{"
                         "  'channel': 'stable'"
                         "}").get());
  features->push_back(api_feature.release());

  EXPECT_FALSE(ComplexFeature(features.Pass()).IsBlockedInServiceWorker());
}

// Tests that dependencies are correctly checked.
TEST_F(ExtensionComplexFeatureTest, Dependencies) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule which depends on an extension-only feature (omnibox).
  scoped_ptr<SimpleFeature> simple_feature(CreateFeature());
  scoped_ptr<base::DictionaryValue> rule =
      DictionaryBuilder()
          .Set("dependencies", ListBuilder().Append("manifest:omnibox"))
          .Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  // Rule which depends on an platform-app-only feature (serial).
  simple_feature.reset(CreateFeature());
  rule = DictionaryBuilder()
             .Set("dependencies", ListBuilder().Append("permission:serial"))
             .Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  scoped_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));

  // Available to extensions because of the omnibox rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest("extensionid",
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Available to platofrm apps because of the serial rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest("platformappid",
                                     Manifest::TYPE_PLATFORM_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Not available to hosted apps.
  EXPECT_EQ(
      Feature::INVALID_TYPE,
      feature->IsAvailableToManifest("hostedappid",
                                     Manifest::TYPE_HOSTED_APP,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());
}

}  // namespace
