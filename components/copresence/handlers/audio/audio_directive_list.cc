// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

namespace copresence {

// Public functions.

AudioDirective::AudioDirective() {}

AudioDirective::AudioDirective(const std::string& op_id,
                               base::TimeTicks end_time)
    : op_id(op_id), end_time(end_time) {}

AudioDirectiveList::AudioDirectiveList(
    const scoped_refptr<TickClockRefCounted>& clock)
    : clock_(clock) {}

AudioDirectiveList::~AudioDirectiveList() {}

void AudioDirectiveList::AddDirective(const std::string& op_id,
                                      base::TimeDelta ttl) {
  base::TimeTicks end_time = clock_->NowTicks() + ttl;

  // If this op is already in the list, update it instead of adding it again.
  auto it = FindDirectiveByOpId(op_id);
  if (it != active_directives_.end()) {
    it->end_time = end_time;
    std::make_heap(active_directives_.begin(),
                   active_directives_.end(),
                   LatestFirstComparator());
    return;
  }

  active_directives_.push_back(AudioDirective(op_id, end_time));
  std::push_heap(active_directives_.begin(),
                 active_directives_.end(),
                 LatestFirstComparator());
}

void AudioDirectiveList::RemoveDirective(const std::string& op_id) {
  auto it = FindDirectiveByOpId(op_id);
  if (it != active_directives_.end())
    active_directives_.erase(it);

  std::make_heap(active_directives_.begin(),
                 active_directives_.end(),
                 LatestFirstComparator());
}

scoped_ptr<AudioDirective> AudioDirectiveList::GetActiveDirective() {
  // The top is always the instruction that is ending the latest.
  // If that time has passed, all our previous instructions have expired too.
  // So we clear the list.
  if (active_directives_.empty() ||
      active_directives_.front().end_time < clock_->NowTicks()) {
    active_directives_.clear();
    return scoped_ptr<AudioDirective>().Pass();
  }

  return make_scoped_ptr(new AudioDirective(active_directives_.front()));
}


// Private functions.

std::vector<AudioDirective>::iterator AudioDirectiveList::FindDirectiveByOpId(
    const std::string& op_id) {
  for (auto it = active_directives_.begin();
       it != active_directives_.end();
       ++it) {
    if (it->op_id == op_id)
      return it;
  }
  return active_directives_.end();
}

}  // namespace copresence
