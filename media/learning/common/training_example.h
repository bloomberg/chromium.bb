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

// Collection of training examples.
// TODO(liberato): This should probably move to impl/ .
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingDataStorage
    : public base::RefCountedThreadSafe<TrainingDataStorage> {
 public:
  // We store examples in a deque, since we don't want to invalidate pointers in
  // TrainingData collections (see below) as new examples are added.  Deques
  // promise not to do that when inserting at either end.
  using StorageVector = std::deque<TrainingExample>;
  using const_iterator = StorageVector::const_iterator;

  TrainingDataStorage();

  StorageVector::const_iterator begin() const { return examples_.begin(); }
  StorageVector::const_iterator end() const { return examples_.end(); }

  // Note that it's okay to add examples at any time.
  void push_back(const TrainingExample& example) {
    examples_.push_back(example);
  }

  // Notice that there's no option to clear storage; that might invalidate
  // outstanding pointers in TrainingData (see below).  Instead, just create a
  // new TrainingDataStorage.

  // Return the number of examples that we store.
  size_t size() const { return examples_.size(); }

 private:
  friend class base::RefCountedThreadSafe<TrainingDataStorage>;

  ~TrainingDataStorage();

  StorageVector examples_;

  DISALLOW_COPY_AND_ASSIGN(TrainingDataStorage);
};

// Collection of pointers to training data.  References would be more convenient
// but they're not allowed.
// TODO(liberato): This should probably move to impl/ .
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
