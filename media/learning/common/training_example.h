// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
#define MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_

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

// Collection of training examples.  We use a vector since we allow duplicates.
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingDataStorage
    : public base::RefCountedThreadSafe<TrainingDataStorage> {
 public:
  using StorageVector = std::vector<TrainingExample>;
  using const_iterator = StorageVector::const_iterator;

  TrainingDataStorage();

  StorageVector::const_iterator begin() const { return examples_.begin(); }
  StorageVector::const_iterator end() const { return examples_.end(); }

  void push_back(const TrainingExample& example) {
    examples_.push_back(example);
  }

  // Returns true if and only if |example| is included in our data.  Note that
  // this checks that the pointer itself is included, so that one might tell if
  // an example is backed by this storage or not.  It does not care if there is
  // an example in our storage that would TrainingExample::operator==(*example).
  bool contains(const TrainingExample* example) const {
    return (example >= examples_.data()) &&
           (example < examples_.data() + examples_.size());
  }

 private:
  friend class base::RefCountedThreadSafe<TrainingDataStorage>;

  ~TrainingDataStorage();

  std::vector<TrainingExample> examples_;

  DISALLOW_COPY_AND_ASSIGN(TrainingDataStorage);
};

// Collection of pointers to training data.  References would be more convenient
// but they're not allowed.
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingData {
 public:
  using ExampleVector = std::vector<const TrainingExample*>;
  using const_iterator = ExampleVector::const_iterator;

  // Construct an empty set of examples, with |backing_storage| as the allowed
  // underlying storage.
  TrainingData(scoped_refptr<TrainingDataStorage> backing_storage);

  // Construct a list of examples from |begin| to excluding |end|.
  TrainingData(scoped_refptr<TrainingDataStorage> backing_storage,
               TrainingDataStorage::const_iterator begin,
               TrainingDataStorage::const_iterator end);

  TrainingData(const TrainingData& rhs);
  TrainingData(TrainingData&& rhs);

  ~TrainingData();

  void push_back(const TrainingExample* example) {
    DCHECK(backing_storage_);
    DCHECK(backing_storage_->contains(example));
    examples_.push_back(example);
  }

  bool empty() const { return examples_.empty(); }

  size_t size() const { return examples_.size(); }

  const_iterator begin() const { return examples_.begin(); }
  const_iterator end() const { return examples_.end(); }

  const TrainingExample* operator[](size_t i) const { return examples_[i]; }

  scoped_refptr<TrainingDataStorage> storage() const {
    return backing_storage_;
  }

 private:
  // It would be nice if we insisted that
  scoped_refptr<TrainingDataStorage> backing_storage_;

  ExampleVector examples_;

  // Copy / assignment is allowed.
};

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const TrainingExample& example);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_TRAINING_EXAMPLE_H_
