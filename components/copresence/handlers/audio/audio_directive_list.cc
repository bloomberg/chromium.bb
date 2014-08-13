// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace copresence {

// Public methods.

AudioDirective::AudioDirective() {
}

AudioDirective::AudioDirective(const std::string& op_id, base::Time end_time)
    : op_id(op_id), end_time(end_time) {
}

AudioDirectiveList::AudioDirectiveList() {
}

AudioDirectiveList::~AudioDirectiveList() {
}

void AudioDirectiveList::AddDirective(const std::string& op_id,
                                      base::TimeDelta ttl) {
  base::Time end_time = base::Time::Now() + ttl;

  // In case this op is already in the list, update it instead of adding
  // it again.
  std::vector<AudioDirective>::iterator it = FindDirectiveByOpId(op_id);
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
  std::vector<AudioDirective>::iterator it = FindDirectiveByOpId(op_id);
  if (it != active_directives_.end())
    active_directives_.erase(it);

  std::make_heap(active_directives_.begin(),
                 active_directives_.end(),
                 LatestFirstComparator());
}

scoped_ptr<AudioDirective> AudioDirectiveList::GetActiveDirective() {
  // The top is always the instruction that is ending the latest. If that time
  // has passed, means all our previous instructions have expired too, hence
  // clear the list.
  if (!active_directives_.empty() &&
      active_directives_.front().end_time < base::Time::Now()) {
    active_directives_.clear();
  }

  if (active_directives_.empty())
    return make_scoped_ptr<AudioDirective>(NULL);

  return make_scoped_ptr(new AudioDirective(active_directives_.front()));
}

std::vector<AudioDirective>::iterator AudioDirectiveList::FindDirectiveByOpId(
    const std::string& op_id) {
  for (std::vector<AudioDirective>::iterator it = active_directives_.begin();
       it != active_directives_.end();
       ++it) {
    if (it->op_id == op_id)
      return it;
  }
  return active_directives_.end();
}

}  // namespace copresence
