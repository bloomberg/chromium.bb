// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_RULESET_DEALER_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace subresource_filter {

class MemoryMappedRuleset;

// Memory maps the subresource filtering ruleset file, and makes it available to
// all call sites of GetRuleset() within the current process.
//
// Although most OS'es will take care of memory mapping pages of the ruleset
// only on-demand, and of swapping out pages eagerly when they are no longer
// used, this class has extra logic to make sure memory is conserved as much as
// possible, and syscall overhead is minimized:
//
//   * The ruleset is only memory mapped on-demand the first time GetRuleset()
//     is called, and unmapped once the last caller drops its reference to it.
//
//   * If the ruleset is used by multiple call sites within the same process,
//     they will use the same, cached, MemoryMappedRuleset instance, and will
//     not call mmap() multiple times.
//
class RulesetDealer {
 public:
  RulesetDealer();
  virtual ~RulesetDealer();

  // Sets the |ruleset_file| to memory map and distribute from now on.
  void SetRulesetFile(base::File ruleset_file);

  // Returns whether a subsequent call to GetRuleset() would return a non-null
  // ruleset, but without memory mapping the ruleset.
  bool IsRulesetAvailable() const;

  // Returns the set |ruleset_file|. Normally, the same instance is used by all
  // call sites in a given process. That intance is mapped lazily and umapped
  // eagerly as soon as the last reference to it is dropped.
  scoped_refptr<const MemoryMappedRuleset> GetRuleset();

 private:
  friend class SubresourceFilterRulesetDealerTest;

  // Exposed for testing only.
  bool has_cached_ruleset() const { return !!weak_cached_ruleset_.get(); }

  base::File ruleset_file_;
  base::WeakPtr<MemoryMappedRuleset> weak_cached_ruleset_;

  DISALLOW_COPY_AND_ASSIGN(RulesetDealer);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_RULESET_DEALER_H_
