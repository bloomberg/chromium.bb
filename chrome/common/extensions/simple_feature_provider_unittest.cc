// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/simple_feature_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

using extensions::Feature;
using extensions::SimpleFeatureProvider;

TEST(SimpleFeatureProvider, ManifestFeatures) {
  SimpleFeatureProvider* provider =
      SimpleFeatureProvider::GetManifestFeatures();
  scoped_ptr<Feature> feature = provider->GetFeature("name");
  ASSERT_TRUE(feature.get());
  EXPECT_EQ(5u, feature->extension_types()->size());
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

  SimpleFeatureProvider provider(value.Pass());

  // feature1 won't validate because it lacks an extension type.
  EXPECT_FALSE(provider.GetFeature("feature1").get());

  // If we add one, it works.
  feature1->Set("extension_types", extension_types->DeepCopy());
  EXPECT_TRUE(provider.GetFeature("feature1").get());

  // feature2 won't validate because of the presence of "contexts".
  EXPECT_FALSE(provider.GetFeature("feature2").get());

  // If we remove it, it works.
  feature2->Remove("contexts", NULL);
  EXPECT_TRUE(provider.GetFeature("feature2").get());
}
