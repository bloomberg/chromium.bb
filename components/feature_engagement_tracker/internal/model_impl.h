// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace base {
struct Feature;
}

namespace feature_engagement_tracker {
class Store;

// A ModelImpl provides the default implementation of the Model.
class ModelImpl : public Model {
 public:
  ModelImpl(std::unique_ptr<Store> store,
            std::unique_ptr<Configuration> configuration);
  ~ModelImpl() override;

  // Model implementation.
  void Initialize(const OnModelInitializationFinished& callback) override;
  bool IsReady() const override;
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override;
  void SetIsCurrentlyShowing(bool is_showing) override;
  bool IsCurrentlyShowing() const override;

 private:
  // The underlying store for all events.
  std::unique_ptr<Store> store_;

  // The current configuration for all features.
  std::unique_ptr<Configuration> configuration_;

  // Whether the model has been fully initialized.
  bool ready_;

  // Whether the model is currently showing an in-product help.
  bool currently_showing_;

  DISALLOW_COPY_AND_ASSIGN(ModelImpl);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_MODEL_IMPL_H_
