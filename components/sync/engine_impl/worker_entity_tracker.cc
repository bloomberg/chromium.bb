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

WorkerEntityTracker::WorkerEntityTracker() {}

WorkerEntityTracker::~WorkerEntityTracker() {}

void WorkerEntityTracker::ReceiveEncryptedUpdate(
    const UpdateResponseData& data) {
  encrypted_update_ = std::make_unique<UpdateResponseData>(data);
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
  memory_usage += EstimateMemoryUsage(encrypted_update_);
  return memory_usage;
}

}  // namespace syncer
