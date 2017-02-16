// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
#define COMPONENTS_SYNC_MODEL_SYNC_DATA_H_

#include <stdint.h>

#include <iosfwd>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/sync/base/immutable.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_service_proxy.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

class AttachmentService;
class SyncDataLocal;
class SyncDataRemote;

// A light-weight container for immutable sync data. Pass-by-value and storage
// in STL containers are supported and encouraged if helpful.
class SyncData {
 public:
  // Creates an empty and invalid SyncData.
  SyncData();
  SyncData(const SyncData& other);
  ~SyncData();

  // Default copy and assign welcome.

  // Helper methods for creating SyncData objects for local data.
  //
  // |sync_tag| Must be a string unique to this datatype and is used as a node
  // identifier server-side.
  //
  // For deletes: |datatype| must specify the datatype who node is being
  // deleted.
  //
  // For adds/updates: |specifics| must be valid and |non_unique_title| (can be
  // the same as |sync_tag|) must be specfied.  Note: |non_unique_title| is
  // primarily for debug purposes, and will be overwritten if the datatype is
  // encrypted.
  //
  // For data with attachments: |attachment_ids| must not contain duplicates.
  static SyncData CreateLocalDelete(const std::string& sync_tag,
                                    ModelType datatype);
  static SyncData CreateLocalData(const std::string& sync_tag,
                                  const std::string& non_unique_title,
                                  const sync_pb::EntitySpecifics& specifics);
  static SyncData CreateLocalDataWithAttachments(
      const std::string& sync_tag,
      const std::string& non_unique_title,
      const sync_pb::EntitySpecifics& specifics,
      const AttachmentIdList& attachment_ids);

  // Helper method for creating SyncData objects originating from the syncer.
  static SyncData CreateRemoteData(
      int64_t id,
      const sync_pb::EntitySpecifics& specifics,
      const base::Time& last_modified_time,
      const AttachmentIdList& attachment_ids,
      const AttachmentServiceProxy& attachment_service,
      const std::string& client_tag_hash = std::string());

  // Whether this SyncData holds valid data. The only way to have a SyncData
  // without valid data is to use the default constructor.
  bool IsValid() const;

  // Return the datatype we're holding information about. Derived from the sync
  // datatype specifics.
  ModelType GetDataType() const;

  // Return the current sync datatype specifics.
  const sync_pb::EntitySpecifics& GetSpecifics() const;

  // Return the non unique title (for debugging). Currently only set for data
  // going TO the syncer, not from.
  const std::string& GetTitle() const;

  // Whether this sync data is for local data or data coming from the syncer.
  bool IsLocal() const;

  std::string ToString() const;

  // Return a list of this SyncData's attachment ids.
  //
  // The attachments may or may not be present on this device.
  AttachmentIdList GetAttachmentIds() const;

  // TODO(zea): Query methods for other sync properties: parent, successor, etc.

 protected:
  // These data members are protected so derived types like SyncDataLocal and
  // SyncDataRemote can access them.

  // Necessary since we forward-declare sync_pb::SyncEntity; see
  // comments in immutable.h.
  struct ImmutableSyncEntityTraits {
    using Wrapper = sync_pb::SyncEntity*;

    static void InitializeWrapper(Wrapper* wrapper);

    static void DestroyWrapper(Wrapper* wrapper);

    static const sync_pb::SyncEntity& Unwrap(const Wrapper& wrapper);

    static sync_pb::SyncEntity* UnwrapMutable(Wrapper* wrapper);

    static void Swap(sync_pb::SyncEntity* t1, sync_pb::SyncEntity* t2);
  };

  using ImmutableSyncEntity =
      Immutable<sync_pb::SyncEntity, ImmutableSyncEntityTraits>;

  // Equal to kInvalidId iff this is local.
  int64_t id_;

  // This may be null if the SyncData represents a deleted item.
  base::Time remote_modification_time_;

  // The actual shared sync entity being held.
  ImmutableSyncEntity immutable_entity_;

  AttachmentServiceProxy attachment_service_;

 private:
  // Whether this SyncData holds valid data.
  bool is_valid_;

  // Clears |entity| and |attachments|.
  SyncData(int64_t id,
           sync_pb::SyncEntity* entity,
           const base::Time& remote_modification_time,
           const AttachmentServiceProxy& attachment_service);
};

// A SyncData going to the syncer.
class SyncDataLocal : public SyncData {
 public:
  // Construct a SyncDataLocal from a SyncData.
  //
  // |sync_data|'s IsLocal() must be true.
  explicit SyncDataLocal(const SyncData& sync_data);
  ~SyncDataLocal();

  // Return the value of the unique client tag. This is only set for data going
  // TO the syncer, not coming from.
  const std::string& GetTag() const;
};

// A SyncData that comes from the syncer.
class SyncDataRemote : public SyncData {
 public:
  // Construct a SyncDataRemote from a SyncData.
  //
  // |sync_data|'s IsLocal() must be false.
  explicit SyncDataRemote(const SyncData& sync_data);
  ~SyncDataRemote();

  // Return the last motification time according to the server. This may be null
  // if the SyncData represents a deleted item.
  const base::Time& GetModifiedTime() const;

  int64_t GetId() const;

  // Returns the tag hash value. May not always be present, in which case an
  // empty string will be returned.
  const std::string& GetClientTagHash() const;

  // Retrieve the attachments indentified by |attachment_ids|. Invoke
  // |callback| with the requested attachments.
  //
  // |callback| will be invoked when the operation is complete (successfully
  // or otherwise).
  //
  // Retrieving the requested attachments may require reading local storage or
  // requesting the attachments from the network.
  //
  void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const AttachmentService::GetOrDownloadCallback& callback);
};

// gmock printer helper.
void PrintTo(const SyncData& sync_data, std::ostream* os);

using SyncDataList = std::vector<SyncData>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
