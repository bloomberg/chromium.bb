// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/instance.h"

namespace media {
namespace learning {

Instance::Instance() = default;

Instance::Instance(std::initializer_list<FeatureValue> init_list)
    : features(init_list) {}

Instance::~Instance() = default;

std::ostream& operator<<(std::ostream& out, const Instance& instance) {
  for (const auto& feature : instance.features)
    out << " " << feature;

  return out;
}

bool Instance::operator==(const Instance& rhs) const {
  return features == rhs.features;
}

}  // namespace learning
}  // namespace media
