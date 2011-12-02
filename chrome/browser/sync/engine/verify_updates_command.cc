// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/verify_updates_command.h"

#include <string>

#include "base/location.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

using syncable::GET_BY_ID;
using syncable::SYNCER;

VerifyUpdatesCommand::VerifyUpdatesCommand() {}
VerifyUpdatesCommand::~VerifyUpdatesCommand() {}

std::set<ModelSafeGroup> VerifyUpdatesCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  std::set<ModelSafeGroup> groups_with_updates;

  const GetUpdatesResponse& updates =
      session.status_controller().updates_response().get_updates();
  for (int i = 0; i < updates.entries().size(); i++) {
    groups_with_updates.insert(
        GetGroupForModelType(syncable::GetModelType(updates.entries(i)),
                             session.routing_info()));
  }

  return groups_with_updates;
}

void VerifyUpdatesCommand::ModelChangingExecuteImpl(
    sessions::SyncSession* session) {
  DVLOG(1) << "Beginning Update Verification";
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  WriteTransaction trans(FROM_HERE, SYNCER, dir);
  sessions::StatusController* status = session->mutable_status_controller();
  const GetUpdatesResponse& updates = status->updates_response().get_updates();
  int update_count = updates.entries().size();

  DVLOG(1) << update_count << " entries to verify";
  for (int i = 0; i < update_count; i++) {
    const SyncEntity& update =
        *reinterpret_cast<const SyncEntity *>(&(updates.entries(i)));
    ModelSafeGroup g = GetGroupForModelType(update.GetModelType(),
                                            session->routing_info());
    if (g != status->group_restriction())
      continue;

    VerifyUpdateResult result = VerifyUpdate(&trans, update,
                                             session->routing_info());
    status->mutable_update_progress()->AddVerifyResult(result.value, update);
    status->increment_num_updates_downloaded_by(1);
    if (update.deleted())
      status->increment_num_tombstone_updates_downloaded_by(1);
  }
}

namespace {
// In the event that IDs match, but tags differ AttemptReuniteClient tag
// will have refused to unify the update.
// We should not attempt to apply it at all since it violates consistency
// rules.
VerifyResult VerifyTagConsistency(const SyncEntity& entry,
                                  const syncable::MutableEntry& same_id) {
  if (entry.has_client_defined_unique_tag() &&
      entry.client_defined_unique_tag() !=
          same_id.Get(syncable::UNIQUE_CLIENT_TAG)) {
    return VERIFY_FAIL;
  }
  return VERIFY_UNDECIDED;
}
}  // namespace

VerifyUpdatesCommand::VerifyUpdateResult VerifyUpdatesCommand::VerifyUpdate(
    syncable::WriteTransaction* trans, const SyncEntity& entry,
    const ModelSafeRoutingInfo& routes) {
  syncable::Id id = entry.id();
  VerifyUpdateResult result = {VERIFY_FAIL, GROUP_PASSIVE};

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const syncable::ModelType model_type = entry.GetModelType();

  if (!id.ServerKnows()) {
    LOG(ERROR) << "Illegal negative id in received updates";
    return result;
  }
  {
    const std::string name = SyncerProtoUtil::NameFromSyncEntity(entry);
    if (name.empty() && !deleted) {
      LOG(ERROR) << "Zero length name in non-deleted update";
      return result;
    }
  }

  syncable::MutableEntry same_id(trans, GET_BY_ID, id);
  result.value = SyncerUtil::VerifyNewEntry(entry, &same_id, deleted);

  syncable::ModelType placement_type = !deleted ? entry.GetModelType()
      : same_id.good() ? same_id.GetModelType() : syncable::UNSPECIFIED;
  result.placement = GetGroupForModelType(placement_type, routes);

  if (VERIFY_UNDECIDED == result.value) {
    result.value = VerifyTagConsistency(entry, same_id);
  }

  if (VERIFY_UNDECIDED == result.value) {
    if (deleted)
      result.value = VERIFY_SUCCESS;
  }

  // If we have an existing entry, we check here for updates that break
  // consistency rules.
  if (VERIFY_UNDECIDED == result.value) {
    result.value = SyncerUtil::VerifyUpdateConsistency(trans, entry, &same_id,
        deleted, is_directory, model_type);
  }

  if (VERIFY_UNDECIDED == result.value)
    result.value = VERIFY_SUCCESS;  // No news is good news.

  return result;  // This might be VERIFY_SUCCESS as well
}

}  // namespace browser_sync
