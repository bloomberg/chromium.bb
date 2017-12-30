// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_
#define CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_

#include <stddef.h>

#include <vector>

#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

class BaseLabelManager;

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

  size_t label_bound() const { return label_bound_; }

  // Accessors for testing.
  const std::vector<offset_t>& targets() const { return targets_; }
  const std::vector<TypeTag>& types() const { return types_; }

  // Returns the number of targets.
  size_t size() const { return targets_.size(); }
  const_iterator begin() const;
  const_iterator end() const;

  // The three functions below are transition hacks that mark targets, which
  // will disappear.

  // Replaces every target represented as offset whose Label is in
  // |label_manager| by the index of this Label, and updates the Label bound
  // associated with |pool|.
  void LabelTargets(const BaseLabelManager& label_manager);

  // Replaces every associated target represented as offset whose Label is in
  // |label_manager| by the index of this Label, and updates the Label bound
  // associated with |pool|. A target is associated iff its Label index is also
  // used in |reference_label_manager|. All targets must have a Label in
  // |label_manager|, and must be represented as offset when calling this
  // function, implying it can only be called once for each |pool|, until
  // UnlabelTargets() (below) is called.
  void LabelAssociatedTargets(const BaseLabelManager& label_manager,
                              const BaseLabelManager& reference_label_manager);

  // Replaces every target represented as a Label index by its original offset,
  // assuming that |label_manager| still holds the same Labels referered to by
  // target indices. Resets Label bound associated with |pool| to 0.
  void UnlabelTargets(const BaseLabelManager& label_manager);

 private:
  std::vector<TypeTag> types_;     // Enumerates type_tag for this pool.
  std::vector<offset_t> targets_;  // Targets for pool in ascending order.
  size_t label_bound_ = 0;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_TARGET_POOL_H_
