// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/equivalence_map.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/installer/zucchini/encoded_view.h"
#include "chrome/installer/zucchini/suffix_array.h"

namespace zucchini {

/******** Utility Functions ********/

double GetTokenSimilarity(const ImageIndex& old_image,
                          const ImageIndex& new_image,
                          offset_t src,
                          offset_t dst) {
  DCHECK(old_image.IsToken(src));
  DCHECK(new_image.IsToken(dst));

  TypeTag old_type = old_image.LookupType(src);
  TypeTag new_type = new_image.LookupType(dst);
  if (old_type != new_type)
    return kMismatchFatal;

  // Raw comparison.
  if (!old_image.IsReference(src) && !new_image.IsReference(dst)) {
    return old_image.GetRawValue(src) == new_image.GetRawValue(dst) ? 1.0
                                                                    : -1.5;
  }

  Reference old_reference = old_image.FindReference(old_type, src);
  Reference new_reference = new_image.FindReference(new_type, dst);

  // Both targets are not associated, which implies a weak match.
  if (!IsMarked(old_reference.target) && !IsMarked(new_reference.target))
    return 0.5 * old_image.GetTraits(old_type).width;

  // At least one target is associated, so values are compared.
  return old_reference.target == new_reference.target
             ? old_image.GetTraits(old_type).width
             : -2.0;
}

double GetEquivalenceSimilarity(const ImageIndex& old_image,
                                const ImageIndex& new_image,
                                const Equivalence& equivalence) {
  double similarity = 0.0;
  for (offset_t k = 0; k < equivalence.length; ++k) {
    // Non-tokens are joined with the nearest previous token: skip until we
    // cover the unit.
    if (!new_image.IsToken(equivalence.dst_offset + k))
      continue;

    similarity +=
        GetTokenSimilarity(old_image, new_image, equivalence.src_offset + k,
                           equivalence.dst_offset + k);
    if (similarity == kMismatchFatal)
      return kMismatchFatal;
  }
  return similarity;
}

EquivalenceCandidate ExtendEquivalenceForward(
    const ImageIndex& old_image,
    const ImageIndex& new_image,
    const EquivalenceCandidate& candidate,
    double min_similarity) {
  Equivalence equivalence = candidate.eq;
  offset_t best_k = equivalence.length;
  double current_similarity = candidate.similarity;
  double best_similarity = current_similarity;
  double current_penalty = min_similarity;
  for (offset_t k = best_k; equivalence.src_offset + k < old_image.size() &&
                            equivalence.dst_offset + k < new_image.size();
       ++k) {
    // Mismatch in type, |candidate| cannot be extended further.
    if (old_image.LookupType(equivalence.src_offset + k) !=
        new_image.LookupType(equivalence.dst_offset + k)) {
      break;
    }

    if (!new_image.IsToken(equivalence.dst_offset + k)) {
      // Non-tokens are joined with the nearest previous token: skip until we
      // cover the unit, and extend |best_k| if applicable.
      if (best_k == k)
        best_k = k + 1;
      continue;
    }

    double similarity =
        GetTokenSimilarity(old_image, new_image, equivalence.src_offset + k,
                           equivalence.dst_offset + k);
    current_similarity += similarity;
    current_penalty = std::max(0.0, current_penalty) - similarity;

    if (current_similarity < 0.0 || current_penalty >= min_similarity)
      break;
    if (current_similarity >= best_similarity) {
      best_similarity = current_similarity;
      best_k = k + 1;
    }
  }
  equivalence.length = best_k;
  return {equivalence, best_similarity};
}

EquivalenceCandidate ExtendEquivalenceBackward(
    const ImageIndex& old_image,
    const ImageIndex& new_image,
    const EquivalenceCandidate& candidate,
    double min_similarity) {
  Equivalence equivalence = candidate.eq;
  offset_t best_k = 0;
  double current_similarity = candidate.similarity;
  double best_similarity = current_similarity;
  double current_penalty = 0.0;
  for (offset_t k = 1;
       k <= equivalence.dst_offset && k <= equivalence.src_offset; ++k) {
    // Mismatch in type, |candidate| cannot be extended further.
    if (old_image.LookupType(equivalence.src_offset - k) !=
        new_image.LookupType(equivalence.dst_offset - k)) {
      break;
    }

    // Non-tokens are joined with the nearest previous token: skip until we
    // reach the next token.
    if (!new_image.IsToken(equivalence.dst_offset - k))
      continue;

    DCHECK_EQ(
        old_image.LookupType(equivalence.src_offset - k),
        new_image.LookupType(equivalence.dst_offset - k));  // Sanity check.
    double similarity =
        GetTokenSimilarity(old_image, new_image, equivalence.src_offset - k,
                           equivalence.dst_offset - k);
    current_similarity += similarity;
    current_penalty = std::max(0.0, current_penalty) - similarity;

    if (current_similarity < 0.0 || current_penalty >= min_similarity)
      break;
    if (current_similarity >= best_similarity) {
      best_similarity = current_similarity;
      best_k = k;
    }
  }

  equivalence.dst_offset -= best_k;
  equivalence.src_offset -= best_k;
  equivalence.length += best_k;
  return {equivalence, best_similarity};
}

EquivalenceCandidate VisitEquivalenceSeed(const ImageIndex& old_image,
                                          const ImageIndex& new_image,
                                          offset_t src,
                                          offset_t dst,
                                          double min_similarity) {
  EquivalenceCandidate candidate{{src, dst, 0}, 0.0};  // Empty.
  candidate =
      ExtendEquivalenceForward(old_image, new_image, candidate, min_similarity);
  if (candidate.similarity < min_similarity)
    return candidate;  // Not worth exploring any more.
  return ExtendEquivalenceBackward(old_image, new_image, candidate,
                                   min_similarity);
}

/******** EquivalenceMap ********/

EquivalenceMap::EquivalenceMap() = default;

EquivalenceMap::EquivalenceMap(
    const std::vector<EquivalenceCandidate>& equivalences)
    : candidates_(equivalences) {
  SortByDestination();
}

EquivalenceMap::EquivalenceMap(EquivalenceMap&&) = default;

EquivalenceMap::~EquivalenceMap() = default;

void EquivalenceMap::Build(const std::vector<offset_t>& old_sa,
                           const ImageIndex& old_image,
                           const ImageIndex& new_image,
                           double min_similarity) {
  DCHECK_EQ(old_sa.size(), old_image.size());

  CreateCandidates(old_sa, old_image, new_image, min_similarity);
  SortByDestination();
  Prune(old_image, new_image, min_similarity);

  offset_t coverage = 0;
  offset_t current_offset = 0;
  for (auto candidate : candidates_) {
    DCHECK_GE(candidate.eq.dst_offset, current_offset);
    coverage += candidate.eq.length;
    current_offset = candidate.eq.dst_end();
  }
  LOG(INFO) << "Equivalence Count: " << size();
  LOG(INFO) << "Coverage / Extra / Total: " << coverage << " / "
            << new_image.size() - coverage << " / " << new_image.size();
}

std::vector<Equivalence> EquivalenceMap::MakeForwardEquivalences() const {
  std::vector<Equivalence> equivalences(size());
  std::transform(begin(), end(), equivalences.begin(),
                 [](const EquivalenceCandidate& c) { return c.eq; });
  std::sort(equivalences.begin(), equivalences.end(),
            [](const Equivalence& a, const Equivalence& b) {
              return a.src_offset < b.src_offset;
            });
  return equivalences;
}

void EquivalenceMap::CreateCandidates(const std::vector<offset_t>& old_sa,
                                      const ImageIndex& old_image,
                                      const ImageIndex& new_image,
                                      double min_similarity) {
  candidates_.clear();

  // This is an heuristic to find 'good' equivalences on encoded views.
  // Equivalences are found in ascending order of |new_image|.
  EncodedView old_view(old_image);
  EncodedView new_view(new_image);
  offset_t dst_offset = 0;

  while (dst_offset < new_image.size()) {
    if (!new_image.IsToken(dst_offset)) {
      ++dst_offset;
      continue;
    }
    auto match =
        SuffixLowerBound(old_sa, old_view.begin(),
                         new_view.begin() + dst_offset, new_view.end());

    offset_t next_dst_offset = dst_offset + 1;
    // TODO(huangs): Clean up.
    double best_similarity = min_similarity;
    EquivalenceCandidate best_candidate = {{0, 0, 0}, 0.0};
    for (auto it = match; it != old_sa.end(); ++it) {
      EquivalenceCandidate candidate =
          VisitEquivalenceSeed(old_image, new_image, static_cast<offset_t>(*it),
                               dst_offset, min_similarity);
      if (candidate.similarity > best_similarity) {
        best_candidate = candidate;
        best_similarity = candidate.similarity;
        next_dst_offset = candidate.eq.dst_end();
      } else {
        break;
      }
    }
    for (auto it = match; it != old_sa.begin(); --it) {
      EquivalenceCandidate candidate = VisitEquivalenceSeed(
          old_image, new_image, static_cast<offset_t>(it[-1]), dst_offset,
          min_similarity);
      if (candidate.similarity > best_similarity) {
        best_candidate = candidate;
        best_similarity = candidate.similarity;
        next_dst_offset = candidate.eq.dst_end();
      } else {
        break;
      }
    }
    if (best_candidate.similarity >= min_similarity) {
      candidates_.push_back(best_candidate);
    }

    dst_offset = next_dst_offset;
  }
}

void EquivalenceMap::SortByDestination() {
  std::sort(candidates_.begin(), candidates_.end(),
            [](const EquivalenceCandidate& a, const EquivalenceCandidate& b) {
              return a.eq.dst_offset < b.eq.dst_offset;
            });
}

void EquivalenceMap::Prune(const ImageIndex& old_image,
                           const ImageIndex& new_image,
                           double min_similarity) {
  for (auto current = candidates_.begin(); current != candidates_.end();
       ++current) {
    if (current->similarity < min_similarity)
      continue;  // This candidate will be discarded anyways.

    // Look ahead to resolve overlaps, until a better candidate is found.
    for (auto next = current + 1; next != candidates_.end(); ++next) {
      DCHECK_GE(next->eq.dst_offset, current->eq.dst_offset);
      if (next->eq.dst_offset >= current->eq.dst_offset + current->eq.length)
        break;  // No more overlap.

      offset_t delta = current->eq.dst_end() - next->eq.dst_offset;

      // |next| is better, so |current| shrinks.
      if (current->similarity < next->similarity) {
        current->eq.length -= delta;
        current->similarity =
            GetEquivalenceSimilarity(old_image, new_image, current->eq);
        break;
      }
    }

    // Shrinks all overlapping candidates following and worse than |current|.
    for (auto next = current + 1; next != candidates_.end(); ++next) {
      if (next->eq.dst_offset >= current->eq.dst_offset + current->eq.length)
        break;  // No more overlap.

      offset_t delta = current->eq.dst_end() - next->eq.dst_offset;
      next->eq.length = next->eq.length > delta ? next->eq.length - delta : 0;
      next->eq.src_offset += delta;
      next->eq.dst_offset += delta;
      next->similarity =
          GetEquivalenceSimilarity(old_image, new_image, next->eq);
      DCHECK_EQ(next->eq.dst_offset, current->eq.dst_end());
    }
  }

  // Discard all candidates with similarity smaller than |min_similarity|.
  candidates_.erase(
      std::remove_if(candidates_.begin(), candidates_.end(),
                     [min_similarity](const EquivalenceCandidate& candidate) {
                       return candidate.similarity < min_similarity;
                     }),
      candidates_.end());
}

}  // namespace zucchini
