// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_FAKE_INVALIDATION_STATE_TRACKER_H_
#define COMPONENTS_INVALIDATION_FAKE_INVALIDATION_STATE_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "components/invalidation/invalidation_state_tracker.h"

namespace syncer {

// InvalidationStateTracker implementation that simply keeps track of
// the max versions and invalidation state in memory.
class FakeInvalidationStateTracker
    : public InvalidationStateTracker,
      public base::SupportsWeakPtr<FakeInvalidationStateTracker> {
 public:
  FakeInvalidationStateTracker();
  virtual ~FakeInvalidationStateTracker();

  // InvalidationStateTracker implementation.
  virtual void ClearAndSetNewClientId(const std::string& client_id) OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual void SetBootstrapData(const std::string& data) OVERRIDE;
  virtual std::string GetBootstrapData() const OVERRIDE;
  virtual void SetSavedInvalidations(
      const UnackedInvalidationsMap& states) OVERRIDE;
  virtual UnackedInvalidationsMap GetSavedInvalidations() const OVERRIDE;
  virtual void Clear() OVERRIDE;

  static const int64 kMinVersion;

 private:
  std::string invalidator_client_id_;
  std::string bootstrap_data_;
  UnackedInvalidationsMap unacked_invalidations_map_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_FAKE_INVALIDATION_STATE_TRACKER_H_
