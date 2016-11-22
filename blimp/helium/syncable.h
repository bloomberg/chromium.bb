// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SYNCABLE_H_
#define BLIMP_HELIUM_SYNCABLE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/helium/result.h"
#include "blimp/helium/version_vector.h"

namespace blimp {
namespace helium {

// LazySyncable is something that supports creating and restoring Changesets.
// These objects exchange Changesets between the client and server to keep
// their peer counterparts eventually synchronized.
//
// It provides the following things:
// 1. Mapping: There is a one-to-one relationship between instances on the
// client and instances on the engine.
// 2. Consistency: The values stored in client-engine pairs should eventually be
// equal.
//
// LazySyncable is a base interface that is used for both self contained objects
// (i.e. simple register) and objects which are disconnected replicas
// of external state. In the latter case, the method PrepareToCreateChangeset()
// is used to tell the LazySyncable to replicate its state asynchronously.
template <typename ChangesetType>
class LazySyncable {
 public:
  // Easy access to the type through which Changesets are serialized for this
  // Syncable.
  using Changeset = ChangesetType;

  virtual ~LazySyncable() = default;

  // Serializes a description of changes between |from| and the current state
  // into the specified |output| changeset. Note that the Sync layer will use
  // GetRevision() to determine the end-revision for the Changeset, so the
  // Syncable must ensure that GetRevision() is incremented at least on the next
  // modification after this call.
  virtual std::unique_ptr<ChangesetType> CreateChangeset(
      Revision from) const = 0;

  // Reads and applies a Changeset originating from the peer counterpart of
  // this Syncable. Failure to apply the changeset should result in a CHECK
  // failure.
  virtual void ApplyChangeset(const ChangesetType& changeset) = 0;

  // Sets the callback that the Syncable should invoke immediately after being
  // modified
  // locally (e.g. LwwRegister::Set() will call it before returning).
  virtual void SetLocalUpdateCallback(
      const base::Closure& local_update_callback) = 0;

  // Returns true if |changeset| is semantically correct and can be
  // Applied() to the state of |this|.
  virtual bool ValidateChangeset(const ChangesetType& changeset) const {
    return true;
  }

  // Allows the Syncable to release book-keeping state related to Revisions
  // prior to |before|. State relating to Revisions |before| and later must be
  // retained.
  virtual void ReleaseBefore(Revision checkpoint) = 0;

  // Returns the current Revision of this Syncable. If the returned Revision is
  // zero then the Syncable is newly-created and has not yet been modified.
  virtual Revision GetRevision() const = 0;

  // Called when the Sync layer is ready to request a Changeset for this
  // Syncable, to allow for external state to be pulled into the Syncable ready
  // for synchronous serialization.
  virtual void PrepareToCreateChangeset(Revision from, base::Closure done) = 0;
};

template <typename ChangesetType>
class Syncable : public LazySyncable<ChangesetType> {
 public:
  // LazySyncable implementation.
  void PrepareToCreateChangeset(Revision from, base::Closure done) override {
    // Default no-op implementation.
    done.Run();
  }
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SYNCABLE_H_
