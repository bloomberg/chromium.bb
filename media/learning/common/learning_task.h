// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LEARNING_TASK_H_
#define MEDIA_LEARNING_COMMON_LEARNING_TASK_H_

#include <initializer_list>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Description of a learning task.  This includes both the description of the
// inputs (features) and output (target value), plus a choice of the model and
// parameters for learning.
// TODO(liberato): Consider separating the task from the choice of model.
// TODO(liberato): should this be in impl?  Probably not if we want to allow
// registering tasks.
struct COMPONENT_EXPORT(LEARNING_COMMON) LearningTask {
  // Not all models support all feature / target descriptions.  For example,
  // NaiveBayes requires kUnordered features.  Similarly, LogLinear woudln't
  // support kUnordered features or targets.  kRandomForest might support more
  // combination of orderings and types.
  enum class Model {
    kRandomForest,
  };

  enum class Ordering {
    // Values are not ordered; nearby values might have wildly different
    // meanings.  For example, two ints that are computed by taking the hash
    // of a string are unordered; it's categorical data.  Values of type DOUBLE
    // should almost certainly not be kUnordered; discretize them in some way
    // if you really want to make discrete, unordered buckets out of them.
    kUnordered,

    // Values may be interpreted as being in numeric order.  For example, two
    // ints that represent the number of elapsed milliseconds are numerically
    // ordered in a meaningful way.
    kNumeric,
  };

  enum class PrivacyMode {
    // Value represents private information, such as a URL that was visited by
    // the user.
    kPrivate,

    // Value does not represent private information, such as video width.
    kPublic,
  };

  // Description of how a Value should be interpreted.
  struct ValueDescription {
    // Name of this value, such as "source_url" or "width".
    std::string name;

    // Is this value nominal or not?
    Ordering ordering = Ordering::kUnordered;

    // Should this value be treated as being private?
    PrivacyMode privacy_mode = PrivacyMode::kPublic;
  };

  LearningTask();
  LearningTask(const std::string& name,
               Model model,
               std::initializer_list<ValueDescription> feature_init_list,
               ValueDescription target_description);
  LearningTask(const LearningTask&);
  ~LearningTask();

  // Unique name for this learner.
  std::string name;

  Model model = Model::kRandomForest;

  std::vector<ValueDescription> feature_descriptions;

  // Note that kUnordered targets indicate classification, while kOrdered
  // targes indicate regression.
  ValueDescription target_description;

  // TODO(liberato): add training parameters, like smoothing constants.  It's
  // okay if some of these are model-specific.
  // TODO(liberato): switch to base::DictionaryValue?

  // Number of examples before we'll train a model.
  size_t min_data_set_size = 10u;

  // Should the accuracy of this model be recorded to UMA?
  bool record_accuracy_via_uma = true;

  // RandomTree parameters

  // How RandomTree handles unknown feature values.
  enum class RTUnknownValueHandling {
    // Return an empty distribution as the prediction.
    kEmptyDistribution,

    // Return the sum of the traversal of all splits.
    kUseAllSplits,
  };
  RTUnknownValueHandling rt_unknown_value_handling =
      RTUnknownValueHandling::kUseAllSplits;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LEARNING_TASK_H_
