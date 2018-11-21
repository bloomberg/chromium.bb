// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_

#include <vector>

#include "media/learning/common/value.h"
#include "media/learning/mojo/public/mojom/learning_types.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<media::learning::mojom::TrainingExampleDataView,
                   media::learning::TrainingExample> {
 public:
  static const std::vector<media::learning::FeatureValue>& features(
      const media::learning::TrainingExample& e) {
    return e.features;
  }
  static media::learning::TargetValue target_value(
      const media::learning::TrainingExample& e) {
    return e.target_value;
  }

  static bool Read(media::learning::mojom::TrainingExampleDataView data,
                   media::learning::TrainingExample* out_example);
};

template <>
class StructTraits<media::learning::mojom::FeatureValueDataView,
                   media::learning::FeatureValue> {
 public:
  static int64_t value(const media::learning::FeatureValue& e) {
    return e.value();
  }
  static bool Read(media::learning::mojom::FeatureValueDataView data,
                   media::learning::FeatureValue* out_feature_value);
};

template <>
class StructTraits<media::learning::mojom::TargetValueDataView,
                   media::learning::TargetValue> {
 public:
  static int64_t value(const media::learning::TargetValue& e) {
    return e.value();
  }
  static bool Read(media::learning::mojom::TargetValueDataView data,
                   media::learning::TargetValue* out_target_value);
};

}  // namespace mojo

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
