// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_EQUIVALENCE_MAP_H_
#define CHROME_INSTALLER_ZUCCHINI_EQUIVALENCE_MAP_H_

#include <stddef.h>

#include <limits>
#include <vector>

#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

constexpr double kMismatchFatal = -std::numeric_limits<double>::infinity();

// Returns a similarity score between content in |old_view| and |new_view| at
// offsets |src| and |dst|, respectively. Both |src| and |dst| must refer to
// tokens in |old_image| and |new_image|.
double GetTokenSimilarity(const ImageIndex& old_image,
                          const ImageIndex& new_image,
                          offset_t src,
                          offset_t dst);

// Returns a similarity score between content in |old_view| and |new_view| at
// regions described by |equivalence|.
double GetEquivalenceSimilarity(const ImageIndex& old_image,
                                const ImageIndex& new_image,
                                const Equivalence& equivalence);

// Extends |equivalence| forward and returns the result. This is related to
// VisitEquivalenceSeed().
EquivalenceCandidate ExtendEquivalenceForward(
    const ImageIndex& old_image,
    const ImageIndex& new_image,
    const EquivalenceCandidate& equivalence,
    double min_similarity);

// Extends |equivalence| backward and returns the result. This is related to
// VisitEquivalenceSeed().
EquivalenceCandidate ExtendEquivalenceBackward(
    const ImageIndex& old_image,
    const ImageIndex& new_image,
    const EquivalenceCandidate& equivalence,
    double min_similarity);

// Creates an equivalence, starting with |src| and |dst| as offset hint, and
// extends it both forward and backward, trying to maximise similarity between
// |old_image| and |new_image|, and returns the result. |min_similarity|
// describes the minimum acceptable similarity score and is used as threshold to
// discard bad equivalences.
EquivalenceCandidate VisitEquivalenceSeed(const ImageIndex& old_image,
                                          const ImageIndex& new_image,
                                          offset_t src,
                                          offset_t dst,
                                          double min_similarity);

// Container of equivalences between |old_image| and |new_image|, sorted by
// |Equivalence::dst_offset|, only used during patch generation.
class EquivalenceMap {
 public:
  using const_iterator = std::vector<EquivalenceCandidate>::const_iterator;

  EquivalenceMap();
  // Initializes the object with |equivalences|.
  explicit EquivalenceMap(
      const std::vector<EquivalenceCandidate>& equivalences);
  EquivalenceMap(EquivalenceMap&&);
  EquivalenceMap(const EquivalenceMap&) = delete;
  ~EquivalenceMap();

  // Finds relevant equivalences between |old_image| and |new_image|, using
  // suffix array |old_sa| computed from |old_image|. This function is not
  // symmetric. Equivalences might overlap in |old_image|, but not in
  // |new_image|. It tries to maximize accumulated similarity within each
  // equivalence, while maximizing |new_image| coverage. The minimum similarity
  // of an equivalence is given by |min_similarity|.
  void Build(const std::vector<offset_t>& old_sa,
             const ImageIndex& old_image,
             const ImageIndex& new_image,
             double min_similarity);

  size_t size() const { return candidates_.size(); }
  const_iterator begin() const { return candidates_.begin(); }
  const_iterator end() const { return candidates_.end(); }

  // Returns a vector containing equivalences sorted by
  // |Equivalence::src_offset|.
  std::vector<Equivalence> MakeForwardEquivalences() const;

 private:
  // Discovers equivalence candidates between |old_image| and |new_image| and
  // stores them in the object. Note that resulting candidates are not sorted
  // and might be overlapping in new image.
  void CreateCandidates(const std::vector<offset_t>& old_sa,
                        const ImageIndex& old_image,
                        const ImageIndex& new_image,
                        double min_similarity);
  // Sorts candidates by their offset in new image.
  void SortByDestination();
  // Visits |candidates_| (sorted by |dst_offset|) and remove all destination
  // overlaps. Candidates with low similarity scores are more likely to be
  // shrunken. Unfit candidates may be removed.
  void Prune(const ImageIndex& old_image,
             const ImageIndex& new_image,
             double min_similarity);

  std::vector<EquivalenceCandidate> candidates_;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_EQUIVALENCE_MAP_H_
