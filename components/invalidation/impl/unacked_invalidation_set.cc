// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/unacked_invalidation_set.h"

#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/topic_invalidation_map.h"

namespace syncer {

const size_t UnackedInvalidationSet::kMaxBufferedInvalidations = 5;

UnackedInvalidationSet::UnackedInvalidationSet(const Topic& topic)
    : registered_(false), topic_(topic) {}

UnackedInvalidationSet::UnackedInvalidationSet(
    const UnackedInvalidationSet& other) = default;

UnackedInvalidationSet::~UnackedInvalidationSet() = default;

const Topic& UnackedInvalidationSet::topic() const {
  return topic_;
}

void UnackedInvalidationSet::Add(
    const Invalidation& invalidation) {
  SingleObjectInvalidationSet set;
  set.Insert(invalidation);
  AddSet(set);
  if (!registered_)
    Truncate(kMaxBufferedInvalidations);
}

void UnackedInvalidationSet::AddSet(
    const SingleObjectInvalidationSet& invalidations) {
  invalidations_.insert(invalidations.begin(), invalidations.end());
  if (!registered_)
    Truncate(kMaxBufferedInvalidations);
}

void UnackedInvalidationSet::ExportInvalidations(
    base::WeakPtr<AckHandler> ack_handler,
    scoped_refptr<base::SingleThreadTaskRunner> ack_handler_task_runner,
    TopicInvalidationMap* out) const {
  for (const Invalidation& invalidation : invalidations_) {
    // Copy the invalidation and set the copy's ack_handler.
    Invalidation invalidation_copy = invalidation;
    invalidation_copy.SetAckHandler(ack_handler, ack_handler_task_runner);
    out->Insert(invalidation_copy);
  }
}

void UnackedInvalidationSet::Clear() {
  invalidations_.clear();
}

void UnackedInvalidationSet::SetHandlerIsRegistered() {
  registered_ = true;
}

void UnackedInvalidationSet::SetHandlerIsUnregistered() {
  registered_ = false;
  Truncate(kMaxBufferedInvalidations);
}

// Removes the matching ack handle from the list.
void UnackedInvalidationSet::Acknowledge(const AckHandle& handle) {
  bool handle_found = false;
  for (auto it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    if (it->ack_handle().Equals(handle)) {
      invalidations_.erase(*it);
      handle_found = true;
      break;
    }
  }
  DLOG_IF(WARNING, !handle_found) << "Unrecognized to ack for topic " << topic_;
  (void)handle_found;  // Silence unused variable warning in release builds.
}

// Erase the invalidation with matching ack handle from the list.  Also creates
// an 'UnknownVersion' invalidation with the same ack handle and places it at
// the beginning of the list.  If an unknown version invalidation currently
// exists, it is replaced.
void UnackedInvalidationSet::Drop(const AckHandle& handle) {
  SingleObjectInvalidationSet::const_iterator it;
  for (it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    if (it->ack_handle().Equals(handle)) {
      break;
    }
  }
  if (it == invalidations_.end()) {
    DLOG(WARNING) << "Unrecognized drop request for topic " << topic_;
    return;
  }

  Invalidation unknown_version = Invalidation::InitFromDroppedInvalidation(*it);
  invalidations_.erase(*it);

  // If an unknown version is in the list, we remove it so we can replace it.
  if (!invalidations_.empty() && invalidations_.begin()->is_unknown_version()) {
    invalidations_.erase(*invalidations_.begin());
  }

  invalidations_.insert(unknown_version);
}

void UnackedInvalidationSet::Truncate(size_t max_size) {
  DCHECK_GT(max_size, 0U);

  if (invalidations_.size() <= max_size) {
    return;
  }

  while (invalidations_.size() > max_size) {
    invalidations_.erase(*invalidations_.begin());
  }

  // We dropped some invalidations.  We remember the fact that an unknown
  // amount of information has been lost by ensuring this list begins with
  // an UnknownVersion invalidation.  We remove the oldest remaining
  // invalidation to make room for it.
  Topic topic = invalidations_.begin()->topic();
  invalidations_.erase(*invalidations_.begin());

  Invalidation unknown_version = Invalidation::InitUnknownVersion(topic);
  invalidations_.insert(unknown_version);
}

}  // namespace syncer
