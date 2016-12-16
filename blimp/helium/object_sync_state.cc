// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/object_sync_state.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/update_scheduler.h"

namespace blimp {
namespace helium {

ObjectSyncState::ObjectSyncState(
    HeliumObjectId object_id,
    std::unique_ptr<LazySyncable<std::string>> object)
    : object_id_(object_id), object_(std::move(object)) {
  object_->SetLocalUpdateCallback(
      base::Bind(&ObjectSyncState::OnLocalUpdate, base::Unretained(this)));
}

ObjectSyncState::~ObjectSyncState() = default;

void ObjectSyncState::OnLocalUpdate() {
  is_dirty_ = true;
  if (scheduler_) {
    scheduler_->OnLocalUpdate(object_id_);
  }
}

void ObjectSyncState::PrepareChangesetMessage(
    MessagePreparedCallback callback) {
  DCHECK(callback);
  DCHECK(scheduler_);

  // Set as not dirty now so that if there are local changes between now and
  // when we send the Changeset, it is reflected in |is_dirty_|.
  is_dirty_ = false;
  prepare_callback_ = std::move(callback);
  object_->PrepareToCreateChangeset(
      sent_revision_, prepared_for_changeset_callback_.callback());
}

void ObjectSyncState::PrepareAckMessage(MessagePreparedCallback callback) {
  DCHECK(callback);

  prepare_callback_ = std::move(callback);
  OnPreparedChangeset();
}

void ObjectSyncState::OnStreamConnected(UpdateScheduler* scheduler) {
  DCHECK(scheduler);

  scheduler_ = scheduler;
  prepared_for_changeset_callback_.Reset(base::Bind(
      &ObjectSyncState::OnPreparedChangeset, base::Unretained(this)));

  // Determine if our last sent HeliumMessage was acknowledged by the peer.
  is_confirmed_ = (sent_revision_ == acked_revision_);

  // Mark the object as requiring an outgoing acknowledgement and inform
  // |scheduler_|.
  remote_revision_needs_ack_ = true;
  scheduler_->OnNeedsAck(object_id_);

  // Schedule an update if the object is confirmed and dirty.
  if (is_confirmed_ && is_dirty_) {
    scheduler_->OnLocalUpdate(object_id_);
  }
}

void ObjectSyncState::OnStreamDisconnected() {
  DCHECK(scheduler_);

  prepared_for_changeset_callback_.Cancel();
  prepare_callback_.Reset();
  scheduler_ = nullptr;
}

void ObjectSyncState::OnChangesetReceived(Revision end_revision,
                                          const std::string& changeset) {
  DCHECK(scheduler_);

  if (object_->ValidateChangeset(changeset)) {
    object_->ApplyChangeset(changeset);
    remote_revision_ = end_revision;
    remote_revision_needs_ack_ = true;
    scheduler_->OnNeedsAck(object_id_);
  }
}

void ObjectSyncState::OnAckReceived(Revision ack_revision) {
  DCHECK(scheduler_);

  acked_revision_ = ack_revision;
  object_->ReleaseBefore(acked_revision_);
  if (!is_confirmed_) {
    is_confirmed_ = true;
    sent_revision_ = ack_revision;
    if (is_dirty_) {
      scheduler_->OnLocalUpdate(object_id_);
    }
  }
}

void ObjectSyncState::OnPreparedChangeset() {
  DCHECK(prepare_callback_);

  proto::HeliumMessage message;
  message.set_object_id(object_id_);

  bool has_changed = object_->GetRevision() > sent_revision_;
  if (is_confirmed_ && has_changed) {
    message.set_start_revision(sent_revision_);
    message.set_end_revision(object_->GetRevision());

    std::unique_ptr<std::string> changeset =
        object_->CreateChangeset(sent_revision_);
    message.set_changeset(*changeset);

    sent_revision_ = message.end_revision();
  }

  if (remote_revision_needs_ack_) {
    message.set_ack_revision(remote_revision_);
    remote_revision_needs_ack_ = false;
  }

  prepare_callback_.Run(std::move(message));
}

}  // namespace helium
}  // namespace blimp
