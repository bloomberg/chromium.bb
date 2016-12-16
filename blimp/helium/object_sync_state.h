// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_OBJECT_SYNC_STATE_H_
#define BLIMP_HELIUM_OBJECT_SYNC_STATE_H_

#include "base/cancelable_callback.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/lazy_syncable_adapter.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/update_scheduler.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

// Each registered LazySyncable is associated with an ObjectSyncState object
// which retains state used to manage Changeset delivery and resynchronization
// from potential message loss.
class BLIMP_HELIUM_EXPORT ObjectSyncState {
 public:
  using MessagePreparedCallback = base::Callback<void(proto::HeliumMessage)>;

  ~ObjectSyncState();

  // Creates an ObjectSyncState instance for a LazySyncable<ChangesetType>,
  // including a specialized adaptor to the generic LazySyncable<string>
  // interface.
  template <class ChangesetType>
  static std::unique_ptr<ObjectSyncState> CreateForObject(
      HeliumObjectId object_id,
      LazySyncable<ChangesetType>* object) {
    return base::WrapUnique<ObjectSyncState>(new ObjectSyncState(
        object_id,
        base::MakeUnique<LazySyncableAdapter<ChangesetType>>(object)));
  }

  // Called by the UpdateScheduler to request a Changeset for the
  // LazySyncable.
  void PrepareChangesetMessage(MessagePreparedCallback callback);

  // Called by the UpdateScheduler to request an ACK-only message.
  void PrepareAckMessage(MessagePreparedCallback callback);

  // Informs the ObjectSyncState that there is a connected Stream and gives an
  // UpdateScheduler to send updates to. The scheduler object will not be
  // destroyed until after OnStreamDisconnected is called.
  void OnStreamConnected(UpdateScheduler* scheduler);

  // Informs the ObjectSyncState that the Stream has been disconnected and
  // so the ObjectSyncState should stop any in-process HeliumMessage
  // preparation.
  void OnStreamDisconnected();

  // Applies the given Changeset to the LazySyncable and informs the
  // UpdateScheduler that an acknowledgement message is needed.
  void OnChangesetReceived(Revision end_revision, const std::string& changeset);

  // Informs the ObjectSyncState that a previously sent HeliumMessage has been
  // received and acknowledged by the peer.
  void OnAckReceived(Revision ack_revision);

 private:
  ObjectSyncState(HeliumObjectId object_id,
                  std::unique_ptr<LazySyncable<std::string>> object);

  // Called by the LazySyncable object when it is updated locally.
  void OnLocalUpdate();

  // Called when |object_| finishes preparing. Pulls a Changeset from |object_|
  // and supplies it to |prepare_callback_|.
  void OnPreparedChangeset();

  // Determines the best time for |object_| to generate a Changeset or
  // acknowledgement message.
  UpdateScheduler* scheduler_ = nullptr;

  // Unique ID for |object_|.
  HeliumObjectId object_id_;

  // Object whose sync state is being tracked.
  std::unique_ptr<LazySyncable<std::string>> object_;

  // Used to track Changeset receipt and delivery.
  Revision sent_revision_ = 0;
  Revision remote_revision_ = 0;
  Revision acked_revision_ = 0;

  // True if |object_| has local modifications which need delivering.
  bool is_dirty_ = false;

  // True if we have received a Changeset but have not yet sent an associated
  // acknowledgement HeliumMessage to the peer.
  bool remote_revision_needs_ack_ = false;

  // True if all Changesets sent to the peer have been acknowledged.
  bool is_confirmed_ = true;

  // Callback received from |scheduler_| when it is requesting a HeliumMessage.
  MessagePreparedCallback prepare_callback_;

  // Used to allow in-progress Changeset preparation by |object_| to be canceled
  // if the stream is torn down in the meantime.
  base::CancelableCallback<void()> prepared_for_changeset_callback_;

  DISALLOW_COPY_AND_ASSIGN(ObjectSyncState);
};

}  // namespace helium
}  // namespace blimp
#endif  // BLIMP_HELIUM_OBJECT_SYNC_STATE_H_
