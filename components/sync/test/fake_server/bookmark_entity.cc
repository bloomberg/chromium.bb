// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/bookmark_entity.h"

#include "base/guid.h"

using std::string;

namespace fake_server {
namespace {

// Returns true if and only if |client_entity| is a bookmark.
bool IsBookmark(const sync_pb::SyncEntity& client_entity) {
  return syncer::GetModelType(client_entity) == syncer::BOOKMARKS;
}

}  // namespace

BookmarkEntity::~BookmarkEntity() {}

// static
std::unique_ptr<FakeServerEntity> BookmarkEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity,
    const string& parent_id,
    const string& client_guid) {
  CHECK_EQ(0, client_entity.version()) << "New entities must have version = 0.";
  CHECK(IsBookmark(client_entity)) << "The given entity must be a bookmark.";

  const string id =
      FakeServerEntity::CreateId(syncer::BOOKMARKS, base::GenerateGUID());
  const string originator_cache_guid = client_guid;
  const string originator_client_item_id = client_entity.id_string();

  return std::unique_ptr<FakeServerEntity>(new BookmarkEntity(
      id, client_entity.version(), client_entity.name(), originator_cache_guid,
      originator_client_item_id, client_entity.unique_position(),
      client_entity.specifics(), client_entity.folder(), parent_id,
      client_entity.ctime(), client_entity.mtime()));
}

// static
std::unique_ptr<FakeServerEntity> BookmarkEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    const FakeServerEntity& current_server_entity,
    const string& parent_id) {
  CHECK_NE(0, client_entity.version()) << "Existing entities must not have a "
                                       << "version = 0.";
  CHECK(IsBookmark(client_entity)) << "The given entity must be a bookmark.";

  const BookmarkEntity& current_bookmark_entity =
      static_cast<const BookmarkEntity&>(current_server_entity);
  const string originator_cache_guid =
      current_bookmark_entity.originator_cache_guid_;
  const string originator_client_item_id =
      current_bookmark_entity.originator_client_item_id_;

  return std::unique_ptr<FakeServerEntity>(new BookmarkEntity(
      client_entity.id_string(), client_entity.version(), client_entity.name(),
      originator_cache_guid, originator_client_item_id,
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), parent_id, client_entity.ctime(),
      client_entity.mtime()));
}

BookmarkEntity::BookmarkEntity(const string& id,
                               int64_t version,
                               const string& name,
                               const string& originator_cache_guid,
                               const string& originator_client_item_id,
                               const sync_pb::UniquePosition& unique_position,
                               const sync_pb::EntitySpecifics& specifics,
                               bool is_folder,
                               const string& parent_id,
                               int64_t creation_time,
                               int64_t last_modified_time)
    : FakeServerEntity(id, string(), syncer::BOOKMARKS, version, name),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      unique_position_(unique_position),
      is_folder_(is_folder),
      parent_id_(parent_id),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  SetSpecifics(specifics);
}

void BookmarkEntity::SetParentId(const string& parent_id) {
  parent_id_ = parent_id;
}

bool BookmarkEntity::RequiresParentId() const {
  // Bookmarks are stored as a hierarchy. All bookmarks must have a parent ID.
  return true;
}

string BookmarkEntity::GetParentId() const {
  return parent_id_;
}

void BookmarkEntity::SerializeAsProto(sync_pb::SyncEntity* proto) const {
  FakeServerEntity::SerializeBaseProtoFields(proto);

  proto->set_originator_cache_guid(originator_cache_guid_);
  proto->set_originator_client_item_id(originator_client_item_id_);

  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);

  sync_pb::UniquePosition* unique_position = proto->mutable_unique_position();
  unique_position->CopyFrom(unique_position_);
}

bool BookmarkEntity::IsFolder() const {
  return is_folder_;
}

}  // namespace fake_server
