// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNC_DATA_H_
#define CHROME_BROWSER_SYNC_API_SYNC_DATA_H_
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/immutable.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

typedef syncable::ModelType SyncDataType;

// A light-weight container for immutable sync data. Pass-by-value and storage
// in STL containers are supported and encouraged if helpful.
class SyncData {
 public:
  // Creates an empty and invalid SyncData.
  SyncData();
   ~SyncData();

   // Default copy and assign welcome.

  // Helper methods for creating SyncData objects for local data.
  // Local sync data must always at least set the sync tag (which must be a
  // string unique to this datatype), for use as a node identifier server-side.
  // For adds/updates, the actual sync data that is changing (in the form of
  // sync_pb::EntitySpecifics) and a human-readable/non-unique title (can be the
  // same as sync tag) must also specfied.
  // Note: the non_unique_title is primarily for debug purposes, and will be
  // overwritten if the datatype is encrypted.
  // For deletes:
  static SyncData CreateLocalData(const std::string& sync_tag);
  // For adds/updates:
  static SyncData CreateLocalData(
      const std::string& sync_tag,
      const std::string& non_unique_title,
      const sync_pb::EntitySpecifics& specifics);

  // Helper methods for creating SyncData objects originating from the syncer.
  static SyncData CreateRemoteData(
      int64 id, const sync_pb::EntitySpecifics& specifics);

  // Whether this SyncData holds valid data. The only way to have a SyncData
  // without valid data is to use the default constructor.
  bool IsValid() const;

  // Return the datatype we're holding information about. Derived from the sync
  // datatype specifics.
  SyncDataType GetDataType() const;

  // Return the current sync datatype specifics.
  const sync_pb::EntitySpecifics& GetSpecifics() const;

  // Returns the value of the unique client tag. This is only set for data going
  // TO the syncer, not coming from.
  const std::string& GetTag() const;

  // Returns the non unique title (for debugging). Currently only set for data
  // going TO the syncer, not from.
  const std::string& GetTitle() const;

  // Should only be called by sync code when IsLocal() is false.
  int64 GetRemoteId() const;

  // Whether this sync data is for local data or data coming from the syncer.
  bool IsLocal() const;

  std::string ToString() const;

  // TODO(zea): Query methods for other sync properties: parent, successor, etc.

 private:
  // Necessary since we forward-declare sync_pb::SyncEntity; see
  // comments in immutable.h.
  struct ImmutableSyncEntityTraits {
    typedef sync_pb::SyncEntity* Wrapper;

    static void InitializeWrapper(Wrapper* wrapper);

    static void DestroyWrapper(Wrapper* wrapper);

    static const sync_pb::SyncEntity& Unwrap(const Wrapper& wrapper);

    static sync_pb::SyncEntity* UnwrapMutable(Wrapper* wrapper);

    static void Swap(sync_pb::SyncEntity* t1, sync_pb::SyncEntity* t2);
  };

  typedef browser_sync::Immutable<
    sync_pb::SyncEntity, ImmutableSyncEntityTraits>
      ImmutableSyncEntity;

  // Clears |entity|.
  SyncData(int64 id, sync_pb::SyncEntity* entity);

  // Whether this SyncData holds valid data.
  bool is_valid_;

  // Equal to sync_api::kInvalidId iff this is local.
  int64 id_;

  // The actual shared sync entity being held.
  ImmutableSyncEntity immutable_entity_;
};

// gmock printer helper.
void PrintTo(const SyncData& sync_data, std::ostream* os);

#endif  // CHROME_BROWSER_SYNC_API_SYNC_DATA_H_
