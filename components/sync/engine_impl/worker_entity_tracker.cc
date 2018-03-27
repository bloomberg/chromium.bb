// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/worker_entity_tracker.h"

#include <algorithm>

#include "base/logging.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/syncable/syncable_util.h"

namespace syncer {

WorkerEntityTracker::WorkerEntityTracker(const std::string& client_tag_hash)
    : client_tag_hash_(client_tag_hash) {
  DCHECK(!client_tag_hash_.empty());
}

WorkerEntityTracker::~WorkerEntityTracker() {}

void WorkerEntityTracker::ReceiveUpdate(const UpdateResponseData& update) {
  if (!UpdateContainsNewVersion(update.response_version))
    return;

  highest_gu_response_version_ = update.response_version;
  DCHECK(!update.entity->id.empty());
  id_ = update.entity->id;

  // Got an applicable update newer than any pending updates. It must be safe
  // to discard the old encrypted update, if there was one.
  ClearEncryptedUpdate();
}

bool WorkerEntityTracker::UpdateContainsNewVersion(int64_t update_version) {
  return update_version > highest_gu_response_version_;
}

bool WorkerEntityTracker::ReceiveEncryptedUpdate(
    const UpdateResponseData& data) {
  if (data.response_version < highest_gu_response_version_)
    return false;

  highest_gu_response_version_ = data.response_version;
  encrypted_update_ = std::make_unique<UpdateResponseData>(data);
  return true;
}

bool WorkerEntityTracker::HasEncryptedUpdate() const {
  return !!encrypted_update_;
}

UpdateResponseData WorkerEntityTracker::GetEncryptedUpdate() const {
  return *encrypted_update_;
}

void WorkerEntityTracker::ClearEncryptedUpdate() {
  encrypted_update_.reset();
}

size_t WorkerEntityTracker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(client_tag_hash_);
  memory_usage += EstimateMemoryUsage(id_);
  memory_usage += EstimateMemoryUsage(encrypted_update_);
  return memory_usage;
}

}  // namespace syncer
