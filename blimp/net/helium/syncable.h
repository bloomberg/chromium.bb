// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_SYNCABLE_H_
#define BLIMP_NET_HELIUM_SYNCABLE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/common/mandatory_callback.h"
#include "blimp/net/helium/version_vector.h"
#include "blimp/net/helium/version_vector_generator.h"

namespace blimp {

namespace proto {
class ChangesetMessage;
}

// Syncable is something that supports creating and restoring Changesets.
// These objects exchange Changesets between the client and server to keep
// their peer counterparts eventually synchronized.
//
// It provides the following things:
// 1. Mapping: There is a one-to-one relationship between instances on the
// client and instances on the engine.
// 2. Consistency: The values stored in client-engine pairs should eventually be
// equal.
//
// Syncable is a base interface that is used for both self contained
// objects (i.e. Simple register) and objects which are disconnected replicas
// of external state.
//
template <class ChangesetType>
class Syncable {
 public:
  virtual ~Syncable() {}

  // Constructs a changeset between the |from| revision and its current state.
  // The Sync layer will encapsulate the changeset with details since |from|,
  // but the Syncable is responsible for including any revision information
  // additional to that expressed by the VersionVectors, that is necessary to
  // detect and resolve conflicts.
  // The changeset is returned as a return value.
  virtual std::unique_ptr<ChangesetType> CreateChangesetToCurrent(
      const VersionVector& from) = 0;

  // Applies a |changeset| given as parameter to the contents of the
  // Syncable.
  // The VersionVectors |from| and |to| can be used to detect and resolve
  // concurrent change conflicts.
  virtual void ApplyChangeset(const VersionVector& from,
                              const VersionVector& to,
                              std::unique_ptr<ChangesetType> changeset) = 0;

  // Gives a chance for the Syncable to delete any old data previous to the
  // |checkpoint|.
  virtual void ReleaseCheckpointsBefore(const VersionVector& checkpoint) = 0;

  // Returns true if the object has been modified since |from|.
  virtual bool ModifiedSince(const VersionVector& from) const = 0;
};

// Extends the Syncable interface by adding support to asynchronously replicate
// state with some
// external entity.
//
// TwoPhaseSyncable name derives from the fact that the state is both created
// and applied in two stages:
//
// 1. Creation
// 1.1) PreCreateChangesetToCurrent is called which retrieves the state
// from an external state and saves locally.
// 1.2) CreateChangesetToCurrent is called to actually create the changeset.
//
// 2. Updating
// 2.1) ApplyChangeset is called which updates the local state.
// 2.2) PostApplyChangeset is called to apply the state from the local
// object into the external state.
class TwoPhaseSyncable : public Syncable<proto::ChangesetMessage> {
 public:
  ~TwoPhaseSyncable() override {}

  // This is before calling CreateChangesetToCurrent to give a change for the
  // TwoPhaseSyncable to pull the latest changes into its local state.
  //
  // The callback |done| should be called once the local instance is ready
  // to accept the call to CreateChangesetToCurrent.
  virtual void PreCreateChangesetToCurrent(const VersionVector& from,
                                           MandatoryClosure&& done) = 0;

  // This is called after calling ApplyChangeset to allow the changes to
  // propagate to the actual external object.
  //
  // The callback |done| should be called once the external world object is
  // updated.
  virtual void PostApplyChangeset(const VersionVector& from,
                                  const VersionVector& to,
                                  MandatoryClosure&& done) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_SYNCABLE_H_
