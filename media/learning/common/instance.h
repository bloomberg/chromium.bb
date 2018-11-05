// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_INSTANCE_H_
#define MEDIA_LEARNING_COMMON_INSTANCE_H_

#include <initializer_list>
#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// One instance == group of feature values.
struct COMPONENT_EXPORT(LEARNING_COMMON) Instance {
  Instance();
  Instance(std::initializer_list<FeatureValue> init_list);
  ~Instance();

  bool operator==(const Instance& rhs) const;

  // It's up to you to add the right number of features to match the learner
  // description.  Otherwise, the learner will ignore (training) or lie to you
  // (inference), silently.
  std::vector<FeatureValue> features;

  // Copy / assignment is allowed.
};

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const Instance& instance);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_INSTANCE_H_
