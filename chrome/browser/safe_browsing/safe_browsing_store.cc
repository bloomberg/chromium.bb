// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store.h"

#include <algorithm>

#include "base/metrics/histogram.h"

namespace {

// Find items matching between |subs| and |adds|, and remove them,
// recording the item from |adds| in |adds_removed|.  To minimize
// copies, the inputs are processing in parallel, so |subs| and |adds|
// should be compatibly ordered (either by SBAddPrefixLess or
// SBAddPrefixHashLess).
//
// |predAS| provides add < sub, |predSA| provides sub < add, for the
// tightest compare appropriate (see calls in SBProcessSubs).
template <class S, class A, typename PredAS, typename PredSA>
void KnockoutSubs(std::vector<S>* subs,
                  std::vector<A>* adds,
                  PredAS predAS, PredSA predSA,
                  std::vector<A>* adds_removed) {
  // Keep a pair of output iterators for writing kept items.  Due to
  // deletions, these may lag the main iterators.  Using erase() on
  // individual items would result in O(N^2) copies.  Using std::list
  // would work around that, at double or triple the memory cost.
  typename std::vector<A>::iterator add_out = adds->begin();
  typename std::vector<S>::iterator sub_out = subs->begin();

  // Current location in vectors.
  // TODO(shess): I want these to be const_iterator, but then
  // std::copy() gets confused.  Could snag a const_iterator add_end,
  // or write an inline std::copy(), but it seems like I'm doing
  // something wrong.
  typename std::vector<A>::iterator add_iter = adds->begin();
  typename std::vector<S>::iterator sub_iter = subs->begin();

  while (add_iter != adds->end() && sub_iter != subs->end()) {
    // If |*sub_iter| < |*add_iter|, retain the sub.
    if (predSA(*sub_iter, *add_iter)) {
      *sub_out = *sub_iter;
      ++sub_out;
      ++sub_iter;

      // If |*add_iter| < |*sub_iter|, retain the add.
    } else if (predAS(*add_iter, *sub_iter)) {
      *add_out = *add_iter;
      ++add_out;
      ++add_iter;

      // Record equal items and drop them.
    } else {
      adds_removed->push_back(*add_iter);
      ++add_iter;
      ++sub_iter;
    }
  }

  // Erase any leftover gap.
  adds->erase(add_out, add_iter);
  subs->erase(sub_out, sub_iter);
}

// Remove items in |removes| from |full_hashes|.  |full_hashes| and
// |removes| should be ordered by SBAddPrefix component.
template <class T>
void RemoveMatchingPrefixes(const std::vector<SBAddPrefix>& removes,
                            std::vector<T>* full_hashes) {
  // This is basically an inline of std::set_difference().
  // Unfortunately, that algorithm requires that the two iterator
  // pairs use the same value types.

  // Where to store kept items.
  typename std::vector<T>::iterator out = full_hashes->begin();

  typename std::vector<T>::iterator hash_iter = full_hashes->begin();
  std::vector<SBAddPrefix>::const_iterator remove_iter = removes.begin();

  while (hash_iter != full_hashes->end() && remove_iter != removes.end()) {
    // Keep items less than |*remove_iter|.
    if (SBAddPrefixLess(*hash_iter, *remove_iter)) {
      *out = *hash_iter;
      ++out;
      ++hash_iter;

      // No hit for |*remove_iter|, bump it forward.
    } else if (SBAddPrefixLess(*remove_iter, *hash_iter)) {
      ++remove_iter;

      // Drop equal items, there may be multiple hits.
    } else {
      do {
        ++hash_iter;
      } while (hash_iter != full_hashes->end() &&
               !SBAddPrefixLess(*remove_iter, *hash_iter));
      ++remove_iter;
    }
  }

  // Erase any leftover gap.
  full_hashes->erase(out, hash_iter);
}

// Remove deleted items (|chunk_id| in |del_set|) from the vector.
template <class T>
void RemoveDeleted(std::vector<T>* vec, const base::hash_set<int32>& del_set) {
  DCHECK(vec);

  // Scan through the items read, dropping the items in |del_set|.
  typename std::vector<T>::iterator add_iter = vec->begin();
  for (typename std::vector<T>::iterator iter = add_iter;
       iter != vec->end(); ++iter) {
    if (del_set.count(iter->chunk_id) == 0) {
      *add_iter = *iter;
      ++add_iter;
    }
  }
  vec->erase(add_iter, vec->end());
}

enum MissTypes {
  MISS_TYPE_ALL,
  MISS_TYPE_FALSE,

  // Always at the end.
  MISS_TYPE_MAX
};

}  // namespace

void SBCheckPrefixMisses(const std::vector<SBAddPrefix>& add_prefixes,
                         const std::set<SBPrefix>& prefix_misses) {
  if (prefix_misses.empty())
    return;

  // Record a hit for all prefixes which missed when sent to the
  // server.
  for (size_t i = 0; i < prefix_misses.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("SB2.BloomFilterFalsePositives",
                              MISS_TYPE_ALL, MISS_TYPE_MAX);
  }

  // Collect the misses which are not present in |add_prefixes|.
  // Since |add_prefixes| can contain multiple copies of the same
  // prefix, it is not sufficient to count the number of elements
  // present in both collections.
  std::set<SBPrefix> false_misses(prefix_misses.begin(), prefix_misses.end());
  for (size_t i = 0; i < add_prefixes.size(); ++i) {
    // |erase()| on an absent element should cost like |find()|.
    false_misses.erase(add_prefixes[i].prefix);
  }

  // Record a hit for prefixes which we shouldn't have sent in the
  // first place.
  for (size_t i = 0; i < false_misses.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("SB2.BloomFilterFalsePositives",
                              MISS_TYPE_FALSE, MISS_TYPE_MAX);
  }

  // Divide |MISS_TYPE_FALSE| by |MISS_TYPE_ALL| to get the
  // bloom-filter false-positive rate.
}

void SBProcessSubs(std::vector<SBAddPrefix>* add_prefixes,
                   std::vector<SBSubPrefix>* sub_prefixes,
                   std::vector<SBAddFullHash>* add_full_hashes,
                   std::vector<SBSubFullHash>* sub_full_hashes,
                   const base::hash_set<int32>& add_chunks_deleted,
                   const base::hash_set<int32>& sub_chunks_deleted) {
  // It is possible to structure templates and template
  // specializations such that the following calls work without having
  // to qualify things.  It becomes very arbitrary, though, and less
  // clear how things are working.

  // Sort the inputs by the SBAddPrefix bits.
  std::sort(add_prefixes->begin(), add_prefixes->end(),
            SBAddPrefixLess<SBAddPrefix,SBAddPrefix>);
  std::sort(sub_prefixes->begin(), sub_prefixes->end(),
            SBAddPrefixLess<SBSubPrefix,SBSubPrefix>);
  std::sort(add_full_hashes->begin(), add_full_hashes->end(),
            SBAddPrefixHashLess<SBAddFullHash,SBAddFullHash>);
  std::sort(sub_full_hashes->begin(), sub_full_hashes->end(),
            SBAddPrefixHashLess<SBSubFullHash,SBSubFullHash>);

  // Factor out the prefix subs.
  std::vector<SBAddPrefix> removed_adds;
  KnockoutSubs(sub_prefixes, add_prefixes,
               SBAddPrefixLess<SBAddPrefix,SBSubPrefix>,
               SBAddPrefixLess<SBSubPrefix,SBAddPrefix>,
               &removed_adds);

  // Remove the full-hashes corrosponding to the adds which
  // KnockoutSubs() removed.  Processing these w/in KnockoutSubs()
  // would make the code more complicated, and they are very small
  // relative to the prefix lists so the gain would be modest.
  RemoveMatchingPrefixes(removed_adds, add_full_hashes);
  RemoveMatchingPrefixes(removed_adds, sub_full_hashes);

  // http://crbug.com/52385
  // TODO(shess): AFAICT this pass is not done on the trunk.  I
  // believe that's a bug, but it may not matter because full-hash
  // subs almost never happen (I think you'd need multiple collisions
  // where one of the sites stopped being flagged?).  Enable this once
  // everything is in.  [if(0) instead of #ifdef 0 to make sure it
  // compiles.]
  if (0) {
    // Factor out the full-hash subs.
    std::vector<SBAddFullHash> removed_full_adds;
    KnockoutSubs(sub_full_hashes, add_full_hashes,
                 SBAddPrefixHashLess<SBAddFullHash,SBSubFullHash>,
                 SBAddPrefixHashLess<SBSubFullHash,SBAddFullHash>,
                 &removed_full_adds);
  }

  // Remove items from the deleted chunks.  This is done after other
  // processing to allow subs to knock out adds (and be removed) even
  // if the add's chunk is deleted.
  RemoveDeleted(add_prefixes, add_chunks_deleted);
  RemoveDeleted(sub_prefixes, sub_chunks_deleted);
  RemoveDeleted(add_full_hashes, add_chunks_deleted);
  RemoveDeleted(sub_full_hashes, sub_chunks_deleted);
}
