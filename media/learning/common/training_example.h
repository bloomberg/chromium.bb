// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
#define MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_

#include <deque>
#include <initializer_list>
#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Vector of features, for training or prediction.
// To interpret the features, one probably needs to check a LearningTask.  It
// provides a description for each index.  For example, [0]=="height",
// [1]=="url", etc.
using FeatureVector = std::vector<FeatureValue>;

// One training example == group of feature values, plus the desired target.
struct COMPONENT_EXPORT(LEARNING_COMMON) TrainingExample {
  TrainingExample();
  TrainingExample(std::initializer_list<FeatureValue> init_list,
                  TargetValue target);
  TrainingExample(const TrainingExample& rhs);
  TrainingExample(TrainingExample&& rhs) noexcept;
  ~TrainingExample();

  bool operator==(const TrainingExample& rhs) const;
  bool operator!=(const TrainingExample& rhs) const;
  bool operator<(const TrainingExample& rhs) const;

  TrainingExample& operator=(const TrainingExample& rhs);
  TrainingExample& operator=(TrainingExample&& rhs);

  // Observed feature values.
  // Note that to interpret these values, you probably need to have the
  // LearningTask that they're supposed to be used with.
  FeatureVector features;

  // Observed output value, when given |features| as input.
  TargetValue target_value;

  // Copy / assignment is allowed.
};

// <TrainingExample, weight == number of counts this example is used>.
class COMPONENT_EXPORT(LEARNING_COMMON) WeightedExample {
 public:
  using weight_t = size_t;

  WeightedExample(const TrainingExample* example, weight_t weight);
  ~WeightedExample();

  const TrainingExample* example() const { return example_; }

  weight_t weight() const { return weight_; }

 private:
  const TrainingExample* example_ = nullptr;
  weight_t weight_ = 0u;

  // Allow copy and assign.
};

// Collection of training examples.
// TODO(liberato): This should probably move to impl/ .
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingDataStorage
    : public base::RefCountedThreadSafe<TrainingDataStorage> {
 public:
  // We store examples in a deque, since we don't want to invalidate pointers in
  // TrainingData collections (see below) as new examples are added.  Deques
  // promise not to do that when inserting at either end.
  using StorageVector = std::deque<TrainingExample>;

  // WeightedExamples can be stored in a vector, since we don't care if they're
  // reallocated.  The underlying ptr is all we care about.
  using WeightedExampleVector = std::vector<WeightedExample>;
  using const_iterator = WeightedExampleVector::const_iterator;

  // For convenience.
  using weight_t = WeightedExample::weight_t;

  TrainingDataStorage();

  const_iterator begin() const { return weighted_examples_.begin(); }
  const_iterator end() const { return weighted_examples_.end(); }

  // Note that it's okay to add examples at any time.
  void push_back(const TrainingExample& example, weight_t weight = 1u) {
    // TODO(liberato): de-duplicate here.
    size_t index = examples_.size();
    examples_.push_back(example);
    const TrainingExample* ptr = &(examples_[index]);
    weighted_examples_.push_back(WeightedExample(ptr, weight));
    total_weight_ += weight;
  }

  // Notice that there's no option to clear storage; that might invalidate
  // outstanding pointers in TrainingData (see below).  Instead, just create a
  // new TrainingDataStorage.

  // Return the number of examples that we store.
  size_t size() const { return examples_.size(); }
  weight_t total_weight() const { return total_weight_; }

  // |i| ranges over [0, size()).
  const WeightedExample& operator[](size_t i) const {
    return weighted_examples_[i];
  }

 private:
  friend class base::RefCountedThreadSafe<TrainingDataStorage>;

  ~TrainingDataStorage();

  StorageVector examples_;
  WeightedExampleVector weighted_examples_;
  weight_t total_weight_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(TrainingDataStorage);
};

// Collection of pointers to training data.  References would be more convenient
// but they're not allowed.
// TODO(liberato): This should probably move to impl/ .
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingData {
 public:
  using ExampleVector = std::vector<WeightedExample>;
  using const_iterator = ExampleVector::const_iterator;

  // For convenience.
  using weight_t = WeightedExample::weight_t;

  // Construct an empty set of examples, with |backing_storage| as the allowed
  // underlying storage.
  TrainingData(scoped_refptr<TrainingDataStorage> backing_storage);

  // Construct a list of examples from |begin| to excluding |end|.
  // TODO(liberato): provide an option to de-duplicate the storage for those
  // training algorithms that don't support weighted examples.
  TrainingData(scoped_refptr<TrainingDataStorage> backing_storage,
               TrainingDataStorage::const_iterator begin,
               TrainingDataStorage::const_iterator end);

  TrainingData(const TrainingData& rhs);
  TrainingData(TrainingData&& rhs);

  ~TrainingData();

  // Add |example| with weight 1.
  void push_back(const TrainingExample* example) {
    DCHECK(backing_storage_);
    examples_.push_back(WeightedExample(example, 1u));
    total_weight_++;
  }

  void push_back(const WeightedExample& weighted_example) {
    DCHECK(backing_storage_);
    examples_.push_back(weighted_example);
    total_weight_ += weighted_example.weight();
  }

  bool empty() const { return !total_weight_; }

  size_t size() const { return examples_.size(); }

  // Returns the number of instances, taking into account their weight.  For
  // example, if one adds an example with weight 2, then this will return two
  // more than it did before.
  weight_t total_weight() const { return total_weight_; }

  const_iterator begin() const { return examples_.begin(); }
  const_iterator end() const { return examples_.end(); }

  scoped_refptr<TrainingDataStorage> storage() const {
    return backing_storage_;
  }

  // Return true if and only if all examples have weight 1.
  bool is_unweighted() const { return size() == total_weight_; }

  // Provide the |i|-th example, over [0, size()).
  WeightedExample operator[](size_t i) const {
    DCHECK(is_unweighted());
    return examples_[i];
  }

 private:
  // It would be nice if we insisted that
  scoped_refptr<TrainingDataStorage> backing_storage_;

  ExampleVector examples_;

  weight_t total_weight_ = 0u;

  // Copy / assignment is allowed.
};

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const TrainingExample& example);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
