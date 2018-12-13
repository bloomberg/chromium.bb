// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_RANDOM_FOREST_H_
#define MEDIA_LEARNING_IMPL_RANDOM_FOREST_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// Bagged forest of randomized trees.
// TODO(liberato): consider a generic Bagging class, since this doesn't really
// depend on RandomTree at all.
class COMPONENT_EXPORT(LEARNING_IMPL) RandomForest : public Model {
 public:
  RandomForest(std::vector<std::unique_ptr<Model>> trees);
  ~RandomForest() override;

  // Model
  TargetDistribution PredictDistribution(
      const FeatureVector& instance) override;

 private:
  std::vector<std::unique_ptr<Model>> trees_;

  DISALLOW_COPY_AND_ASSIGN(RandomForest);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_RANDOM_FOREST_H_
