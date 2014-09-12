// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <string>

#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ComplexFeatureTest, MultipleRulesWhitelist) {
  const std::string kIdFoo("fooabbbbccccddddeeeeffffgggghhhh");
  const std::string kIdBar("barabbbbccccddddeeeeffffgggghhhh");
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule: "extension", whitelist "foo".
  scoped_ptr<SimpleFeature> simple_feature(new SimpleFeature);
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
      .Set("whitelist", ListBuilder().Append(kIdFoo))
      .Set("extension_types", ListBuilder()
          .Append("extension")).Build());
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  // Rule: "legacy_packaged_app", whitelist "bar".
  simple_feature.reset(new SimpleFeature);
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

// Tests that dependencies are correctly checked.
TEST(ComplexFeatureTest, Dependencies) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule which depends on an extension-only feature (content_security_policy).
  scoped_ptr<SimpleFeature> simple_feature(new SimpleFeature);
  scoped_ptr<base::DictionaryValue> rule =
      DictionaryBuilder()
          .Set("dependencies",
               ListBuilder().Append("manifest:content_security_policy"))
          .Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  // Rule which depends on an platform-app-only feature (serial).
  simple_feature.reset(new SimpleFeature);
  rule = DictionaryBuilder()
             .Set("dependencies", ListBuilder().Append("permission:serial"))
             .Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.release());

  scoped_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));

  // Available to extensions because of the content_security_policy rule.
  EXPECT_EQ(
      Feature::IS_AVAILABLE,
      feature->IsAvailableToManifest("extensionid",
                                     Manifest::TYPE_EXTENSION,
                                     Manifest::INVALID_LOCATION,
                                     Feature::UNSPECIFIED_PLATFORM,
                                     Feature::GetCurrentPlatform()).result());

  // Available to platform apps because of the serial rule.
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

}  // namespace extensions
