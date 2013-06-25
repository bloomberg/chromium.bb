// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_FILTER_OPERATIONS_H_
#define CC_OUTPUT_FILTER_OPERATIONS_H_

#include <vector>

#include "base/logging.h"
#include "cc/output/filter_operation.h"

namespace cc {

// An ordered list of filter operations.
class CC_EXPORT FilterOperations {
 public:
  FilterOperations();

  FilterOperations(const FilterOperations& other);

  ~FilterOperations();

  FilterOperations& operator=(const FilterOperations& other);

  bool operator==(const FilterOperations& other) const;

  bool operator!=(const FilterOperations& other) const {
    return !(*this == other);
  }

  void Append(const FilterOperation& filter);

  // Removes all filter operations.
  void Clear();

  bool IsEmpty() const;

  void GetOutsets(int* top, int* right, int* bottom, int* left) const;
  bool HasFilterThatMovesPixels() const;
  bool HasFilterThatAffectsOpacity() const;

  size_t size() const {
    return operations_.size();
  }

  FilterOperation at(size_t index) const {
    DCHECK_LT(index, size());
    return operations_[index];
  }

 private:
  std::vector<FilterOperation> operations_;
};

}  // namespace cc

#endif  // CC_OUTPUT_FILTER_OPERATIONS_H_
