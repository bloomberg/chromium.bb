// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/update_scheduler.h"

#include <map>
#include <set>

#include "base/memory/ptr_util.h"
#include "blimp/helium/object_sync_state.h"
#include "blimp/helium/syncable.h"

namespace blimp {
namespace helium {
namespace {

class UpdateSchedulerImpl : public UpdateScheduler {
 public:
  UpdateSchedulerImpl() = default;
  ~UpdateSchedulerImpl() override = default;

  // UpdateScheduler implementation.
  void AddObject(HeliumObjectId object_id, ObjectSyncState* object) override;
  void RemoveObject(HeliumObjectId object_id) override;
  bool HasMessageReady() const override;
  void PrepareMessage(MessagePreparedCallback callback) override;
  void OnLocalUpdate(HeliumObjectId object_id) override;
  void OnNeedsAck(HeliumObjectId object_id) override;
  void OnStreamConnected(StreamPump* pump) override;

 private:
  StreamPump* pump_ = nullptr;

  // TODO(steimel): replace with to-be-created ObjectDirectory.
  std::map<HeliumObjectId, ObjectSyncState*> objects_;
  std::set<HeliumObjectId> dirty_;
  std::set<HeliumObjectId> needs_ack_;

  DISALLOW_COPY_AND_ASSIGN(UpdateSchedulerImpl);
};

void UpdateSchedulerImpl::AddObject(HeliumObjectId object_id,
                                    ObjectSyncState* object_sync_state) {
  DCHECK(object_sync_state);

  objects_[object_id] = object_sync_state;
}

void UpdateSchedulerImpl::RemoveObject(HeliumObjectId object_id) {
  auto object_iterator = objects_.find(object_id);
  DCHECK(object_iterator != objects_.end());
  object_iterator->second->OnStreamDisconnected();

  objects_.erase(object_id);
  dirty_.erase(object_id);
  needs_ack_.erase(object_id);
}

bool UpdateSchedulerImpl::HasMessageReady() const {
  return !dirty_.empty() || !needs_ack_.empty();
}

void UpdateSchedulerImpl::PrepareMessage(MessagePreparedCallback callback) {
  if (!dirty_.empty()) {
    HeliumObjectId object_id = *(dirty_.begin());
    dirty_.erase(object_id);
    needs_ack_.erase(object_id);

    auto object_iterator = objects_.find(object_id);
    DCHECK(object_iterator != objects_.end());
    object_iterator->second->PrepareChangesetMessage(std::move(callback));
  } else if (!needs_ack_.empty()) {
    HeliumObjectId object_id = *(needs_ack_.begin());
    needs_ack_.erase(object_id);

    auto object_iterator = objects_.find(object_id);
    DCHECK(object_iterator != objects_.end());
    object_iterator->second->PrepareAckMessage(std::move(callback));
  } else {
    NOTREACHED();
  }
}

void UpdateSchedulerImpl::OnLocalUpdate(HeliumObjectId object_id) {
  DCHECK_EQ(1u, objects_.count(object_id));

  dirty_.insert(object_id);
  if (pump_) {
    pump_->OnMessageAvailable();
  }
}

void UpdateSchedulerImpl::OnNeedsAck(HeliumObjectId object_id) {
  DCHECK_EQ(1u, objects_.count(object_id));

  needs_ack_.insert(object_id);
  if (pump_) {
    pump_->OnMessageAvailable();
  }
}

void UpdateSchedulerImpl::OnStreamConnected(StreamPump* pump) {
  pump_ = pump;

  for (auto object : objects_) {
    object.second->OnStreamConnected(this);
  }

  pump_->OnMessageAvailable();
}

}  // namespace

// static
std::unique_ptr<UpdateScheduler> UpdateScheduler::Create() {
  return base::MakeUnique<UpdateSchedulerImpl>();
}

}  // namespace helium
}  // namespace blimp
