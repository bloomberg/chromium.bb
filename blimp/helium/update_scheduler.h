// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_UPDATE_SCHEDULER_H_
#define BLIMP_HELIUM_UPDATE_SCHEDULER_H_

#include "base/callback.h"
#include "blimp/helium/blimp_helium_export.h"

namespace blimp {

namespace proto {
class HeliumMessage;
}  // namespace proto

namespace helium {

class ObjectSyncState;
class StreamPump {
 public:
  virtual ~StreamPump() = default;

  virtual void OnMessageAvailable() = 0;
};

// TODO(steimel): #include sync_manager.h instead once it's ready.
using HeliumObjectId = uint32_t;

// The UpdateScheduler is responsible for determining the next message to be
// sent on each Stream. This primarily means choosing a dirty object to pull the
// next Changeset from and generating acknowledgement messages for received
// Changesets.
class BLIMP_HELIUM_EXPORT UpdateScheduler {
 public:
  using MessagePreparedCallback = base::Callback<void(proto::HeliumMessage)>;

  virtual ~UpdateScheduler() = default;

  // Adds/Removes objects bound to the Stream for which this instance schedules
  // updates. |object| should not be destroyed before calling RemoveObject().
  virtual void AddObject(HeliumObjectId object_id, ObjectSyncState* object) = 0;
  virtual void RemoveObject(HeliumObjectId object_id) = 0;

  // Called by StreamPump to determine if the UpdateScheduler has data to send.
  virtual bool HasMessageReady() const = 0;

  // Called by StreamPump to request next HeliumMessage.
  virtual void PrepareMessage(MessagePreparedCallback callback) = 0;

  // Called by the ObjectSyncState when an object is updated locally.
  // Note that this may be called (e.g. on Stream connection) when an object is
  // first registered with the UpdateScheduler, to force it to check for pending
  // changes.
  virtual void OnLocalUpdate(HeliumObjectId object_id) = 0;

  // Called by the ObjectSyncState when an object processes a Changeset, to
  // schedule acknowledgement.
  virtual void OnNeedsAck(HeliumObjectId object_id) = 0;

  // Associates |pump| with this instance and informs each ObjectSyncState of
  // the new connection. |pump| should not be destroyed before this instance.
  virtual void OnStreamConnected(StreamPump* pump) = 0;

  // Creates an UpdateScheduler instance.
  static std::unique_ptr<UpdateScheduler> Create();
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_UPDATE_SCHEDULER_H_
