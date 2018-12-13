// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_forest.h"

namespace media {
namespace learning {

RandomForest::RandomForest(std::vector<std::unique_ptr<Model>> trees)
    : trees_(std::move(trees)) {}

RandomForest::~RandomForest() = default;

TargetDistribution RandomForest::PredictDistribution(
    const FeatureVector& instance) {
  TargetDistribution forest_distribution;

  for (auto iter = trees_.begin(); iter != trees_.end(); iter++)
    forest_distribution += (*iter)->PredictDistribution(instance);

  return forest_distribution;
}

}  // namespace learning
}  // namespace media
