// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  SimpleFeature* feature = new FeatureClass();
  feature->AddFilter(
      scoped_ptr<SimpleFeatureFilter>(new ChromeChannelFeatureFilter(feature)));
  return feature;
}

Feature::AvailabilityResult IsAvailableInChannel(
    const std::string& channel,
    version_info::Channel channel_for_testing) {
  ScopedCurrentChannel current_channel(channel_for_testing);

  SimpleFeature feature;
  feature.AddFilter(scoped_ptr<SimpleFeatureFilter>(
      new ChromeChannelFeatureFilter(&feature)));

  base::DictionaryValue feature_value;
  feature_value.SetString("channel", channel);
  feature.Parse(&feature_value);

  return feature.IsAvailableToManifest("random-extension",
                                       Manifest::TYPE_UNKNOWN,
                                       Manifest::INVALID_LOCATION,
                                       -1,
                                       Feature::GetCurrentPlatform()).result();
}

}  // namespace

class ChromeChannelFeatureFilterTest : public testing::Test {
 protected:
  ChromeChannelFeatureFilterTest()
      : current_channel_(version_info::Channel::UNKNOWN) {}
  ~ChromeChannelFeatureFilterTest() override {}

 private:
  ScopedCurrentChannel current_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChromeChannelFeatureFilterTest);
};

// Tests that all combinations of feature channel and Chrome channel correctly
// compute feature availability.
TEST_F(ChromeChannelFeatureFilterTest, SupportedChannel) {
  // stable supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("stable", version_info::Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("stable", version_info::Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("stable", version_info::Channel::DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("stable", version_info::Channel::BETA));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("stable", version_info::Channel::STABLE));

  // beta supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("beta", version_info::Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("beta", version_info::Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("beta", version_info::Channel::DEV));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("beta", version_info::Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("beta", version_info::Channel::STABLE));

  // dev supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("dev", version_info::Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("dev", version_info::Channel::CANARY));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("dev", version_info::Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("dev", version_info::Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("dev", version_info::Channel::STABLE));

  // canary supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("canary", version_info::Channel::UNKNOWN));
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("canary", version_info::Channel::CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("canary", version_info::Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("canary", version_info::Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("canary", version_info::Channel::STABLE));

  // trunk supported.
  EXPECT_EQ(Feature::IS_AVAILABLE,
            IsAvailableInChannel("trunk", version_info::Channel::UNKNOWN));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("trunk", version_info::Channel::CANARY));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("trunk", version_info::Channel::DEV));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("trunk", version_info::Channel::BETA));
  EXPECT_EQ(Feature::UNSUPPORTED_CHANNEL,
            IsAvailableInChannel("trunk", version_info::Channel::STABLE));
}

// Tests the validation of features with channel entries.
TEST_F(ChromeChannelFeatureFilterTest, FeatureValidation) {
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

// Tests simple feature availability across channels.
TEST_F(ChromeChannelFeatureFilterTest, SimpleFeatureAvailability) {
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
          .Set("feature1",
               ListBuilder()
                   .Append(DictionaryBuilder().Set("channel", "beta").Set(
                       "extension_types", ListBuilder().Append("extension")))
                   .Append(DictionaryBuilder().Set("channel", "beta").Set(
                       "extension_types",
                       ListBuilder().Append("legacy_packaged_app"))))
          .Build());

  scoped_ptr<BaseFeatureProvider> provider(
      new BaseFeatureProvider(*rule, CreateFeature<SimpleFeature>));

  Feature* feature = provider->GetFeature("feature1");
  EXPECT_TRUE(feature);

  // Make sure both rules are applied correctly.
  {
    ScopedCurrentChannel current_channel(version_info::Channel::BETA);
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
    ScopedCurrentChannel current_channel(version_info::Channel::STABLE);
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

// Tests complex feature availability across channels.
TEST_F(ChromeChannelFeatureFilterTest, ComplexFeatureAvailability) {
  scoped_ptr<ComplexFeature::FeatureList> features(
      new ComplexFeature::FeatureList());

  // Rule: "extension", channel trunk.
  scoped_ptr<SimpleFeature> simple_feature(CreateFeature<SimpleFeature>());
  scoped_ptr<base::DictionaryValue> rule(
      DictionaryBuilder()
          .Set("channel", "trunk")
          .Set("extension_types", ListBuilder().Append("extension"))
          .Build());
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.Pass());

  // Rule: "legacy_packaged_app", channel stable.
  simple_feature.reset(CreateFeature<SimpleFeature>());
  rule =
      DictionaryBuilder()
          .Set("channel", "stable")
          .Set("extension_types", ListBuilder().Append("legacy_packaged_app"))
          .Build();
  simple_feature->Parse(rule.get());
  features->push_back(simple_feature.Pass());

  scoped_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));

  // Test match 1st rule.
  {
    ScopedCurrentChannel current_channel(version_info::Channel::UNKNOWN);
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
    ScopedCurrentChannel current_channel(version_info::Channel::BETA);
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
    ScopedCurrentChannel current_channel(version_info::Channel::BETA);
    EXPECT_NE(
        Feature::IS_AVAILABLE,
        feature->IsAvailableToManifest("1",
                                       Manifest::TYPE_EXTENSION,
                                       Manifest::INVALID_LOCATION,
                                       Feature::UNSPECIFIED_PLATFORM,
                                       Feature::GetCurrentPlatform()).result());
  }
}

}  // namespace extensions
