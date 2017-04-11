// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/model_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {
const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

void NoOpCallback(bool success) {}

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid) {
  FeatureConfig config;
  config.valid = valid;
  config.feature_used_event = feature.name;
  configuration->SetConfiguration(&feature, config);
}

class ModelImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<Store> store = base::MakeUnique<InMemoryStore>();
    std::unique_ptr<EditableConfiguration> configuration =
        base::MakeUnique<EditableConfiguration>();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar, true);

    scoped_feature_list_.InitWithFeatures({kTestFeatureFoo, kTestFeatureBar},
                                          {});

    model_.reset(new ModelImpl(std::move(store), std::move(configuration)));
  }

 protected:
  std::unique_ptr<ModelImpl> model_;
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

TEST_F(ModelImplTest, InitializationShouldMakeReady) {
  EXPECT_FALSE(model_->IsReady());
  model_->Initialize(base::Bind(&NoOpCallback));
  EXPECT_TRUE(model_->IsReady());
}

TEST_F(ModelImplTest, ShowingState) {
  model_->Initialize(base::Bind(&NoOpCallback));
  EXPECT_TRUE(model_->IsReady());

  EXPECT_FALSE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(false);
  EXPECT_FALSE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(true);
  EXPECT_TRUE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(false);
  EXPECT_FALSE(model_->IsCurrentlyShowing());
}

}  // namespace feature_engagement_tracker
