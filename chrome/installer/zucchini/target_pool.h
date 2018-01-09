// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_
#define CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_

#include <stddef.h>

#include <vector>

#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

// Ordered container of distinct targets that have the same semantics, along
// with a list of associated reference types, only used during patch generation.
class TargetPool {
 public:
  using const_iterator = std::vector<offset_t>::const_iterator;

  TargetPool();
  // Initializes the object with given sorted and unique |targets|.
  explicit TargetPool(std::vector<offset_t>&& targets);
  TargetPool(const TargetPool&) = delete;
  TargetPool(TargetPool&&);
  ~TargetPool();

  // The following functions insert each new target from |references|. This
  // invalidates all previous key lookups.
  void InsertTargets(const std::vector<Reference>& references);
  void InsertTargets(ReferenceReader&& references);

  // Adds |type| as a reference type associated with the pool of targets.
  void AddType(TypeTag type) { types_.push_back(type); }

  // Returns a canonical key associated with |offset|.
  key_t KeyForOffset(offset_t offset) const;

  // Returns the target for a |key|, which is assumed to be valid and held by
  // this class.
  offset_t OffsetForKey(key_t key) const { return targets_[key]; }

  // Accessors for testing.
  const std::vector<offset_t>& targets() const { return targets_; }
  const std::vector<TypeTag>& types() const { return types_; }

  // Returns the number of targets.
  size_t size() const { return targets_.size(); }
  const_iterator begin() const;
  const_iterator end() const;

 private:
  std::vector<TypeTag> types_;     // Enumerates type_tag for this pool.
  std::vector<offset_t> targets_;  // Targets for pool in ascending order.
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_
