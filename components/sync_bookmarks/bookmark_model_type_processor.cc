// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync_bookmarks/bookmark_model_observer_impl.h"
#include "components/undo/bookmark_undo_utils.h"

namespace sync_bookmarks {

namespace {

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags that aren't supported by bookmarks (at the time of writing). Each tag
// corresponds to a singleton instance of a particular top-level node in a
// user's share; the tags are consistent across users. The tags allow us to
// locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "bookmark_bar" represents the permanent
// node for bookmarks bar in Chrome. The tag "other_bookmarks" represents the
// permanent folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// ProfileSyncService can rely on the presence of tagged nodes.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Id is created by concatenating the specifics field number and the server tag
// similar to LookbackServerEntity::CreateId() that uses
// GetSpecificsFieldNumberFromModelType() to compute the field number.
static const char kBookmarksRootId[] = "32904_google_chrome_bookmarks";

// |sync_entity| must contain a bookmark specifics.
// Metainfo entries must have unique keys.
bookmarks::BookmarkNode::MetaInfoMap GetBookmarkMetaInfo(
    const syncer::EntityData& sync_entity) {
  const sync_pb::BookmarkSpecifics& specifics =
      sync_entity.specifics.bookmark();
  bookmarks::BookmarkNode::MetaInfoMap meta_info_map;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    meta_info_map[meta_info.key()] = meta_info.value();
  }
  DCHECK_EQ(static_cast<size_t>(specifics.meta_info_size()),
            meta_info_map.size());
  return meta_info_map;
}

// Creates a bookmark node under the given parent node from the given sync node.
// Returns the newly created node. |sync_entity| must contain a bookmark
// specifics with Metainfo entries having unique keys.
const bookmarks::BookmarkNode* CreateBookmarkNode(
    const syncer::EntityData& sync_entity,
    const bookmarks::BookmarkNode* parent,
    bookmarks::BookmarkModel* model,
    int index) {
  DCHECK(parent);
  DCHECK(model);

  const sync_pb::BookmarkSpecifics& specifics =
      sync_entity.specifics.bookmark();
  bookmarks::BookmarkNode::MetaInfoMap metainfo =
      GetBookmarkMetaInfo(sync_entity);
  if (sync_entity.is_folder) {
    return model->AddFolderWithMetaInfo(
        parent, index, base::UTF8ToUTF16(specifics.title()), &metainfo);
  }
  // 'creation_time_us' was added in M24. Assume a time of 0 means now.
  const int64_t create_time_us = specifics.creation_time_us();
  base::Time create_time =
      (create_time_us == 0)
          ? base::Time::Now()
          : base::Time::FromDeltaSinceWindowsEpoch(
                // Use FromDeltaSinceWindowsEpoch because create_time_us has
                // always used the Windows epoch.
                base::TimeDelta::FromMicroseconds(create_time_us));
  return model->AddURLWithCreationTimeAndMetaInfo(
      parent, index, base::UTF8ToUTF16(specifics.title()),
      GURL(specifics.url()), create_time, &metainfo);
  // TODO(crbug.com/516866): Add the favicon related code.
}

// Check whether an incoming specifics represent a valid bookmark or not.
// |is_folder| is whether this specifics is for a folder or not.
// Folders and tomstones entail different validation conditions.
bool IsValidBookmark(const sync_pb::BookmarkSpecifics& specifics,
                     bool is_folder) {
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid bookmark: empty specifics.";
    return false;
  }
  if (is_folder) {
    return true;
  }
  if (!GURL(specifics.url()).is_valid()) {
    DLOG(ERROR) << "Invalid bookmark: invalid url in the specifics.";
    return false;
  }
  return true;
}

sync_pb::EntitySpecifics SpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(node->url().spec());
  // TODO(crbug.com/516866): Set the favicon.
  bm_specifics->set_title(base::UTF16ToUTF8(node->GetTitle()));
  bm_specifics->set_creation_time_us(
      node->date_added().ToDeltaSinceWindowsEpoch().InMicroseconds());

  bm_specifics->set_icon_url(node->icon_url() ? node->icon_url()->spec()
                                              : std::string());

  for (const std::pair<std::string, std::string>& pair :
       *node->GetMetaInfoMap()) {
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key(pair.first);
    meta_info->set_value(pair.second);
  }
  return specifics;
}

class ScopedRemoteUpdateBookmarks {
 public:
  // |bookmark_model|, |bookmark_undo_service| and |observer| must not be null
  // and must outlive this object.
  ScopedRemoteUpdateBookmarks(bookmarks::BookmarkModel* bookmark_model,
                              BookmarkUndoService* bookmark_undo_service,
                              bookmarks::BookmarkModelObserver* observer)
      : bookmark_model_(bookmark_model),
        suspend_undo_(bookmark_undo_service),
        observer_(observer) {
    // Notify UI intensive observers of BookmarkModel that we are about to make
    // potentially significant changes to it, so the updates may be batched. For
    // example, on Mac, the bookmarks bar displays animations when bookmark
    // items are added or deleted.
    DCHECK(bookmark_model_);
    bookmark_model_->BeginExtensiveChanges();
    // Shouldn't be notified upon changes due to sync.
    bookmark_model_->RemoveObserver(observer_);
  }

  ~ScopedRemoteUpdateBookmarks() {
    // Notify UI intensive observers of BookmarkModel that all updates have been
    // applied, and that they may now be consumed. This prevents issues like the
    // one described in https://crbug.com/281562, where old and new items on the
    // bookmarks bar would overlap.
    bookmark_model_->EndExtensiveChanges();
    bookmark_model_->AddObserver(observer_);
  }

 private:
  bookmarks::BookmarkModel* const bookmark_model_;

  // Changes made to the bookmark model due to sync should not be undoable.
  ScopedSuspendBookmarkUndo suspend_undo_;

  bookmarks::BookmarkModelObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRemoteUpdateBookmarks);
};
}  // namespace

BookmarkModelTypeProcessor::BookmarkModelTypeProcessor(
    BookmarkUndoService* bookmark_undo_service)
    : bookmark_undo_service_(bookmark_undo_service), weak_ptr_factory_(this) {}

BookmarkModelTypeProcessor::~BookmarkModelTypeProcessor() {
  if (bookmark_model_ && bookmark_model_observer_) {
    bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  }
}

void BookmarkModelTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  DCHECK(bookmark_model_);

  worker_ = std::move(worker);

  // |bookmark_tracker_| is instantiated only after initial sync is done.
  if (bookmark_tracker_) {
    NudgeForCommitIfNeeded();
  }
}

void BookmarkModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    const GetLocalChangesCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<syncer::CommitRequestData> local_changes =
      BuildCommitRequestsForLocalChanges(max_entries);
  callback.Run(std::move(local_changes));
}

void BookmarkModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const syncer::CommitResponseDataList& response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const syncer::CommitResponseData& response : response_list) {
    // In order to save space, |response.id_in_request| is written when it's
    // different from |response.id|. If it's empty, then there was no id change
    // during the commit, and |response.id| carries both the old and new ids.
    const std::string& old_sync_id =
        response.id_in_request.empty() ? response.id : response.id_in_request;
    bookmark_tracker_->UpdateUponCommitResponse(old_sync_id, response.id,
                                                response.sequence_number,
                                                response.response_version);
  }
  bookmark_tracker_->set_model_type_state(
      std::make_unique<sync_pb::ModelTypeState>(type_state));
  schedule_save_closure_.Run();
}

void BookmarkModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const syncer::UpdateResponseDataList& updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_type_state.cache_guid().empty());
  DCHECK_EQ(model_type_state.cache_guid(), cache_guid_);
  DCHECK(model_type_state.initial_sync_done());

  if (!bookmark_tracker_) {
    // TODO(crbug.com/516866): Implement the merge logic.
    StartTrackingMetadata(
        std::vector<NodeMetadataPair>(),
        std::make_unique<sync_pb::ModelTypeState>(model_type_state));
  }
  // TODO(crbug.com/516866): Set the model type state.

  ScopedRemoteUpdateBookmarks update_bookmarks(
      bookmark_model_, bookmark_undo_service_, bookmark_model_observer_.get());

  for (const syncer::UpdateResponseData* update : ReorderUpdates(updates)) {
    const syncer::EntityData& update_entity = update->entity.value();
    // TODO(crbug.com/516866): Check |update_entity| for sanity.
    // 1. Has bookmark specifics or no specifics in case of delete.
    // 2. All meta info entries in the specifics have unique keys.
    const SyncedBookmarkTracker::Entity* tracked_entity =
        bookmark_tracker_->GetEntityForSyncId(update_entity.id);
    if (update_entity.is_deleted()) {
      ProcessRemoteDelete(update_entity, tracked_entity);
      continue;
    }
    if (!tracked_entity) {
      ProcessRemoteCreate(*update);
      continue;
    }
    // Ignore changes to the permanent nodes (e.g. bookmarks bar). We only care
    // about their children.
    if (bookmark_model_->is_permanent_node(tracked_entity->bookmark_node())) {
      continue;
    }
    ProcessRemoteUpdate(*update, tracked_entity);
  }
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkModelTypeProcessor::ReorderUpdatesForTest(
    const syncer::UpdateResponseDataList& updates) {
  return ReorderUpdates(updates);
}

const SyncedBookmarkTracker* BookmarkModelTypeProcessor::GetTrackerForTest()
    const {
  return bookmark_tracker_.get();
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkModelTypeProcessor::ReorderUpdates(
    const syncer::UpdateResponseDataList& updates) {
  // TODO(crbug.com/516866): This is a very simple (hacky) reordering algorithm
  // that assumes no folders exist except the top level permanent ones. This
  // should be fixed before enabling USS for bookmarks.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const syncer::UpdateResponseData& update : updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    if (update_entity.parent_id == "0") {
      continue;
    }
    if (update_entity.parent_id == kBookmarksRootId) {
      ordered_updates.push_back(&update);
    }
  }
  for (const syncer::UpdateResponseData& update : updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    // Deletions should come last.
    if (update_entity.is_deleted()) {
      continue;
    }
    if (update_entity.parent_id != "0" &&
        update_entity.parent_id != kBookmarksRootId) {
      ordered_updates.push_back(&update);
    }
  }
  // Now add deletions.
  for (const syncer::UpdateResponseData& update : updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    if (!update_entity.is_deleted()) {
      continue;
    }
    if (update_entity.parent_id != "0" &&
        update_entity.parent_id != kBookmarksRootId) {
      ordered_updates.push_back(&update);
    }
  }
  return ordered_updates;
}

void BookmarkModelTypeProcessor::ProcessRemoteCreate(
    const syncer::UpdateResponseData& update) {
  // Because the Synced Bookmarks node can be created server side, it's possible
  // it'll arrive at the client as an update. In that case it won't have been
  // associated at startup, the GetChromeNodeFromSyncId call above will return
  // null, and we won't detect it as a permanent node, resulting in us trying to
  // create it here (which will fail). Therefore, we add special logic here just
  // to detect the Synced Bookmarks folder.
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK(!update_entity.is_deleted());
  if (update_entity.parent_id == kBookmarksRootId) {
    // Associate permanent folders.
    // TODO(crbug.com/516866): Method documentation says this method should be
    // used in initial sync only. Make sure this is the case.
    AssociatePermanentFolder(update);
    return;
  }
  if (!IsValidBookmark(update_entity.specifics.bookmark(),
                       update_entity.is_folder)) {
    // Ignore creations with invalid specifics.
    DLOG(ERROR) << "Couldn't add bookmark with an invalid specifics.";
    return;
  }
  const bookmarks::BookmarkNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    DLOG(ERROR) << "Could not find parent of node being added."
                << " Node title: " << update_entity.specifics.bookmark().title()
                << ", parent id = " << update_entity.parent_id;
    return;
  }
  // TODO(crbug.com/516866): This code appends the code to the very end of the
  // list of the children by assigning the index to the
  // parent_node->child_count(). It should instead compute the exact using the
  // unique position information of the new node as well as the siblings.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(
      update_entity, parent_node, bookmark_model_, parent_node->child_count());
  if (!bookmark_node) {
    // We ignore bookmarks we can't add.
    DLOG(ERROR) << "Failed to create bookmark node with title "
                << update_entity.specifics.bookmark().title() << " and url "
                << update_entity.specifics.bookmark().url();
    return;
  }
  bookmark_tracker_->Add(update_entity.id, bookmark_node,
                         update.response_version, update_entity.creation_time,
                         update_entity.unique_position,
                         update_entity.specifics);
}

void BookmarkModelTypeProcessor::ProcessRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity.value();
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());
  if (!IsValidBookmark(update_entity.specifics.bookmark(),
                       update_entity.is_folder)) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR) << "Couldn't update bookmark with an invalid specifics.";
    return;
  }
  if (tracked_entity->IsUnsynced()) {
    // TODO(crbug.com/516866): Handle conflict resolution.
    return;
  }
  if (tracked_entity->MatchesData(update_entity)) {
    bookmark_tracker_->Update(update_entity.id, update.response_version,
                              update_entity.modification_time,
                              update_entity.specifics);
    // Since there is no change in the model data, we should trigger data
    // persistence here to save latest metadata.
    schedule_save_closure_.Run();
    return;
  }
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  if (update_entity.is_folder != node->is_folder()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.is_folder ? "folder" : "bookmark")
                << " while local node is a "
                << (node->is_folder() ? "folder" : "bookmark");
    return;
  }
  const sync_pb::BookmarkSpecifics& specifics =
      update_entity.specifics.bookmark();
  if (!update_entity.is_folder) {
    bookmark_model_->SetURL(node, GURL(specifics.url()));
  }

  bookmark_model_->SetTitle(node, base::UTF8ToUTF16(specifics.title()));
  // TODO(crbug.com/516866): Add the favicon related code.
  bookmark_model_->SetNodeMetaInfoMap(node, GetBookmarkMetaInfo(update_entity));
  bookmark_tracker_->Update(update_entity.id, update.response_version,
                            update_entity.modification_time,
                            update_entity.specifics);
  // TODO(crbug.com/516866): Handle reparenting.
  // TODO(crbug.com/516866): Handle the case of moving the bookmark to a new
  // position under the same parent (i.e. change in the unique position)
}

void BookmarkModelTypeProcessor::ProcessRemoteDelete(
    const syncer::EntityData& update_entity,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Local entity doesn't exist and update is tombstone.
    DLOG(WARNING) << "Received remote delete for a non-existing item.";
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  // Ignore changes to the permanent top-level nodes.  We only care about
  // their children.
  if (bookmark_model_->is_permanent_node(node)) {
    return;
  }
  // TODO(crbug.com/516866): Allow deletions of non-empty direcoties if makes
  // sense, and recursively delete children.
  if (node->child_count() > 0) {
    DLOG(WARNING) << "Trying to delete a non-empty folder.";
    return;
  }

  bookmark_model_->Remove(node);
  bookmark_tracker_->Remove(update_entity.id);
}

const bookmarks::BookmarkNode* BookmarkModelTypeProcessor::GetParentNode(
    const syncer::EntityData& update_entity) const {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->bookmark_node();
}

void BookmarkModelTypeProcessor::AssociatePermanentFolder(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK_EQ(update_entity.parent_id, kBookmarksRootId);

  const bookmarks::BookmarkNode* permanent_node = nullptr;
  if (update_entity.server_defined_unique_tag == kBookmarkBarTag) {
    permanent_node = bookmark_model_->bookmark_bar_node();
  } else if (update_entity.server_defined_unique_tag == kOtherBookmarksTag) {
    permanent_node = bookmark_model_->other_node();
  } else if (update_entity.server_defined_unique_tag == kMobileBookmarksTag) {
    permanent_node = bookmark_model_->mobile_node();
  }

  if (permanent_node != nullptr) {
    bookmark_tracker_->Add(update_entity.id, permanent_node,
                           update.response_version, update_entity.creation_time,
                           update_entity.unique_position,
                           update_entity.specifics);
  }
}

std::string BookmarkModelTypeProcessor::EncodeSyncMetadata() const {
  std::string metadata_str;
  if (bookmark_tracker_) {
    sync_pb::BookmarkModelMetadata model_metadata =
        bookmark_tracker_->BuildBookmarkModelMetadata();
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void BookmarkModelTypeProcessor::DecodeSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    bookmarks::BookmarkModel* model) {
  DCHECK(model);
  DCHECK(!bookmark_model_);
  DCHECK(!bookmark_tracker_);
  DCHECK(!bookmark_model_observer_);

  bookmark_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  auto model_type_state = std::make_unique<sync_pb::ModelTypeState>();
  model_type_state->Swap(model_metadata.mutable_model_type_state());

  if (model_type_state->initial_sync_done()) {
    std::vector<NodeMetadataPair> nodes_metadata;
    for (sync_pb::BookmarkMetadata& bookmark_metadata :
         *model_metadata.mutable_bookmarks_metadata()) {
      // TODO(crbug.com/516866): Replace with a more efficient way to retrieve
      // all nodes and store in a map keyed by id instead of doing a lookup for
      // every id.
      const bookmarks::BookmarkNode* node = nullptr;
      if (bookmark_metadata.metadata().is_deleted()) {
        if (bookmark_metadata.has_id()) {
          DLOG(ERROR) << "Error when decoding sync metadata: Tombstones "
                         "shouldn't have a bookmark id.";
          continue;
        }
      } else {
        if (!bookmark_metadata.has_id()) {
          DLOG(ERROR)
              << "Error when decoding sync metadata: Bookmark id is missing.";
          continue;
        }
        node = GetBookmarkNodeByID(bookmark_model_, bookmark_metadata.id());
        if (node == nullptr) {
          DLOG(ERROR) << "Error when decoding sync metadata: Cannot find the "
                         "bookmark node.";
          continue;
        }
      }
      auto metadata = std::make_unique<sync_pb::EntityMetadata>();
      metadata->Swap(bookmark_metadata.mutable_metadata());
      nodes_metadata.emplace_back(node, std::move(metadata));
    }
    // TODO(crbug.com/516866): Handle local nodes that don't have a
    // corresponding
    // metadata.
    StartTrackingMetadata(std::move(nodes_metadata),
                          std::move(model_type_state));
  } else if (!model_metadata.bookmarks_metadata().empty()) {
    DLOG(ERROR)
        << "Persisted Metadata not empty while initial sync is not done.";
  }
  ConnectIfReady();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
BookmarkModelTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void BookmarkModelTypeProcessor::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(start_callback);
  DVLOG(1) << "Sync is starting for Bookmarks";

  cache_guid_ = request.cache_guid;
  start_callback_ = std::move(start_callback);
  DCHECK(!cache_guid_.empty());
  ConnectIfReady();
}

void BookmarkModelTypeProcessor::ConnectIfReady() {
  // Return if the model isn't ready.
  if (!bookmark_model_) {
    return;
  }
  // Return if Sync didn't start yet.
  if (!start_callback_) {
    return;
  }

  DCHECK(!cache_guid_.empty());

  if (bookmark_tracker_ &&
      bookmark_tracker_->model_type_state().cache_guid() != cache_guid_) {
    // TODO(crbug.com/820049): Properly handle a mismatch between the loaded
    // cache guid stored in |bookmark_tracker_.model_type_state_| at
    // DecodeSyncMetadata() and the one received from sync at OnSyncStarting()
    // stored in |cache_guid_|.
    return;
  }

  auto activation_context =
      std::make_unique<syncer::DataTypeActivationResponse>();
  if (bookmark_tracker_) {
    activation_context->model_type_state =
        bookmark_tracker_->model_type_state();
  } else {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::BOOKMARKS));
    model_type_state.set_cache_guid(cache_guid_);
    activation_context->model_type_state = model_type_state;
  }
  activation_context->type_processor =
      std::make_unique<syncer::ModelTypeProcessorProxy>(
          weak_ptr_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get());
  std::move(start_callback_).Run(std::move(activation_context));
}

void BookmarkModelTypeProcessor::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_guid_.clear();
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(bookmark_tracker_);
  // Don't bother sending anything if there's no one to send to.
  if (!worker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (bookmark_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

std::vector<syncer::CommitRequestData>
BookmarkModelTypeProcessor::BuildCommitRequestsForLocalChanges(
    size_t max_entries) {
  DCHECK(bookmark_tracker_);
  const std::vector<const SyncedBookmarkTracker::Entity*>
      entities_with_local_changes =
          bookmark_tracker_->GetEntitiesWithLocalChanges(max_entries);
  DCHECK_LE(entities_with_local_changes.size(), max_entries);

  std::vector<syncer::CommitRequestData> commit_requests;
  for (const SyncedBookmarkTracker::Entity* entity :
       entities_with_local_changes) {
    DCHECK(entity->IsUnsynced());
    const sync_pb::EntityMetadata* metadata = entity->metadata();

    syncer::CommitRequestData request;
    syncer::EntityData data;
    data.id = metadata->server_id();
    data.creation_time = syncer::ProtoTimeToTime(metadata->creation_time());
    data.modification_time =
        syncer::ProtoTimeToTime(metadata->modification_time());
    if (!metadata->is_deleted()) {
      const bookmarks::BookmarkNode* node = entity->bookmark_node();
      DCHECK(node);
      const bookmarks::BookmarkNode* parent = node->parent();
      const SyncedBookmarkTracker::Entity* parent_entity =
          bookmark_tracker_->GetEntityForBookmarkNode(parent);
      DCHECK(parent_entity);
      data.parent_id = parent_entity->metadata()->server_id();
      // TODO(crbug.com/516866): Double check that custom passphrase works well
      // with this implementation, because:
      // 1. NonBlockingTypeCommitContribution::AdjustCommitProto() clears the
      //    title out.
      // 2. Bookmarks (maybe ancient legacy bookmarks only?) use/used |name| to
      //    encode the title.
      data.is_folder = node->is_folder();
      // TODO(crbug.com/516866): Set the non_unique_name similar to directory
      // implementation.
      // https://cs.chromium.org/chromium/src/components/sync/syncable/write_node.cc?l=41&rcl=1675007db1e0eb03417e81442688bb11cd181f58
      data.non_unique_name = base::UTF16ToUTF8(node->GetTitle());
      data.unique_position = parent_entity->metadata()->unique_position();
      // In case of deletion, make an EntityData with empty specifics to
      // indicate deletion.
      data.specifics = SpecificsFromBookmarkNode(node);
    }
    request.entity = data.PassToPtr();
    request.sequence_number = metadata->sequence_number();
    request.base_version = metadata->server_version();
    // Specifics hash has been computed in the tracker when this entity has been
    // added/updated.
    request.specifics_hash = metadata->specifics_hash();

    commit_requests.push_back(std::move(request));
  }
  return commit_requests;
}

void BookmarkModelTypeProcessor::StartTrackingMetadata(
    std::vector<NodeMetadataPair> nodes_metadata,
    std::unique_ptr<sync_pb::ModelTypeState> model_type_state) {
  bookmark_tracker_ = std::make_unique<SyncedBookmarkTracker>(
      std::move(nodes_metadata), std::move(model_type_state));

  bookmark_model_observer_ = std::make_unique<BookmarkModelObserverImpl>(
      base::BindRepeating(&BookmarkModelTypeProcessor::NudgeForCommitIfNeeded,
                          base::Unretained(this)),
      bookmark_tracker_.get());
  bookmark_model_->AddObserver(bookmark_model_observer_.get());
}

void BookmarkModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::RecordMemoryUsageHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

}  // namespace sync_bookmarks
