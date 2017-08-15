// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_LABEL_MANAGER_H_
#define CHROME_INSTALLER_ZUCCHINI_LABEL_MANAGER_H_

#include <stddef.h>

#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

// A LabelManager stores a list of Labels. By definition, all offsets and
// indices must be distinct. It also provides functions to:
// - Get the offset of a stored index.
// - Get the index of a stored offset.
// - Create new Labels.
// A LabelManager allows to have a bijection between offsets and indices.
// Since not all targets have associated labels from LabelManager, targets
// represented both as offset or indices are mixed. Hence, indices are
// represented as "marked" values (implemented by setting the MSB), and offsets
// are "unmarked". So when working with stored targets:
// - IsMarked() distinguishes offsets (false) from indexes (true).
// - MarkIndex() is used to encode indexes to their stored value.
// - UnmarkIndex() is used to decode stored indexes to their actual value.
// - Target offsets are stored verbatim, but they must not be marked. This
//   affects reference parsing, where we reject all references whose offsets
//   happen to be marked.

// Base class for OrderedLabelManager and UnorderedLabelManager.
class BaseLabelManager {
 public:
  BaseLabelManager();
  BaseLabelManager(const BaseLabelManager&);
  virtual ~BaseLabelManager();

  // Returns whether |offset_or_marked_index| is a valid offset.
  static constexpr bool IsOffset(offset_t offset_or_marked_index) {
    return !IsMarked(offset_or_marked_index);
  }

  // Returns whether |offset_or_marked_index| is a valid marked index.
  static constexpr bool IsMarkedIndex(offset_t offset_or_marked_index) {
    return IsMarked(offset_or_marked_index) &&
           offset_or_marked_index != kUnusedIndex;
  }

  // Returns whether a given (unmarked) |index| is used by a stored Label.
  bool IsIndexStored(offset_t index) const {
    DCHECK(!IsMarked(index));
    return index < labels_.size() && labels_[index] != kUnusedIndex;
  }

  // Returns the offset of a given (unmarked) |index| if it is associated with a
  // stored Label, or |kUnusedIndex| otherwise.
  offset_t OffsetOfIndex(offset_t index) const {
    return index < labels_.size() ? labels_[index] : kUnusedIndex;
  }

  // Returns the offset corresponding to |offset_or_marked_index|, which is a
  // target that is stored either as a marked index, or as an (unmarked) offset.
  offset_t OffsetFromMarkedIndex(offset_t offset_or_marked_index) const;

  // If |offset| has an associated stored Label, returns its index. Otherwise
  // returns |kUnusedIndex|.
  virtual offset_t IndexOfOffset(offset_t offset) const = 0;

  // Returns the marked index corresponding to |offset_or_marked_index|, which
  // is a target that is stored either as a marked index, or as an (unmarked)
  // offset.
  virtual offset_t MarkedIndexFromOffset(
      offset_t offset_or_marked_index) const = 0;

  size_t size() const { return labels_.size(); }

 protected:
  // Main storage of distinct offsets. This allows O(1) look up of an offset
  // from its index. UnorderedLabelManager may contain "gaps" with
  // |kUnusedIndex|.
  std::vector<offset_t> labels_;
};

// OrderedLabelManager is a LabelManager that prioritizes memory efficiency,
// storing Labels as a sorted list of offsets in |labels_|. Label insertions
// are performed in batch to reduce costs. Index-of-offset lookup is O(lg n)
// (binary search).
class OrderedLabelManager : public BaseLabelManager {
 public:
  OrderedLabelManager();
  OrderedLabelManager(const OrderedLabelManager&);
  ~OrderedLabelManager() override;

  // BaseLabelManager:
  offset_t IndexOfOffset(offset_t offset) const override;
  offset_t MarkedIndexFromOffset(
      offset_t offset_or_marked_index) const override;

  // Creates and stores a new Label for each unique offset in |offsets|. This
  // invalidates all previous Label lookups.
  void InsertOffsets(const std::vector<offset_t>& offsets);

  // For each unique target from |reader|, creates and stores a new Label. This
  // invalidates all previous Label lookups.
  void InsertTargets(ReferenceReader&& reader);

  const std::vector<offset_t>& Labels() const { return labels_; }
};

// UnorderedLabelManager is a LabelManager that does not requires Labels to be
// sorted. Therefore, it can be initialized from Labels given in any order. It
// also prioritizes speed for lookup and insertion, but uses more memory than
// OrderedLabelManager. In addition to using |labels_| to store *unsorted*
// distinct offsets, an unordered_map |labels_map_| is used for index-of-offset
// lookup.
class UnorderedLabelManager : public BaseLabelManager {
 public:
  UnorderedLabelManager();
  UnorderedLabelManager(const UnorderedLabelManager&);
  ~UnorderedLabelManager() override;

  // BaseLabelManager:
  offset_t IndexOfOffset(offset_t offset) const override;
  offset_t MarkedIndexFromOffset(
      offset_t offset_or_marked_index) const override;

  // Clears and reinitializes all stored data. Requires that |labels| consists
  // of unique offsets, but it may have "gaps" in the form of |kUnusedIndex|.
  void Init(std::vector<offset_t>&& labels);

  // Creates a new Label for |offset|. Behavior is undefined if |offset| is
  // already associated with a stored Label. If |kUnusedIndex| gaps exist, tries
  // to reused indices to create new Labels, otherwise it allocates new indices.
  // Previous lookup results involving stored offsets / indexes remain valid.
  void InsertNewOffset(offset_t offset);

 private:
  // Inverse map of |labels_| (excludes |kUnusedIndex|).
  std::unordered_map<offset_t, offset_t> labels_map_;

  // Index into |label_| to scan for |kUnusedIndex| entry in |labels_|.
  size_t gap_idx_ = 0;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_LABEL_MANAGER_H_
