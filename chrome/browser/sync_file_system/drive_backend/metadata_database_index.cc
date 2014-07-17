// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// LevelDB database schema
// =======================
//
// NOTE
// - Entries are sorted by keys.
// - int64 value is serialized as a string by base::Int64ToString().
// - ServiceMetadata, FileMetadata, and FileTracker values are serialized
//   as a string by SerializeToString() of protocol buffers.
//
// Version 3
//   # Version of this schema
//   key: "VERSION"
//   value: "3"
//
//   # Metadata of the SyncFS service
//   key: "SERVICE"
//   value: <ServiceMetadata 'service_metadata'>
//
//   # Metadata of remote files
//   key: "FILE: " + <string 'file_id'>
//   value: <FileMetadata 'metadata'>
//
//   # Trackers of local file updates
//   key: "TRACKER: " + <int64 'tracker_id'>
//   value: <FileTracker 'tracker'>

namespace sync_file_system {
namespace drive_backend {

ParentIDAndTitle::ParentIDAndTitle() : parent_id(0) {}
ParentIDAndTitle::ParentIDAndTitle(int64 parent_id,
                                   const std::string& title)
    : parent_id(parent_id), title(title) {}

bool operator==(const ParentIDAndTitle& left, const ParentIDAndTitle& right) {
  return left.parent_id == right.parent_id && left.title == right.title;
}

bool operator<(const ParentIDAndTitle& left, const ParentIDAndTitle& right) {
  if (left.parent_id != right.parent_id)
    return left.parent_id < right.parent_id;
  return left.title < right.title;
}

DatabaseContents::DatabaseContents() {}

DatabaseContents::~DatabaseContents() {}

namespace {

template <typename Container>
typename Container::mapped_type FindItem(
    const Container& container,
    const typename Container::key_type& key) {
  typename Container::const_iterator found = container.find(key);
  if (found == container.end())
    return typename Container::mapped_type();
  return found->second;
}

void ReadDatabaseContents(leveldb::DB* db,
                          DatabaseContents* contents) {
  DCHECK(db);
  DCHECK(contents);

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    std::string value = itr->value().ToString();

    std::string file_id;
    if (RemovePrefix(key, kFileMetadataKeyPrefix, &file_id)) {
      scoped_ptr<FileMetadata> metadata(new FileMetadata);
      if (!metadata->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a FileMetadata");
        continue;
      }

      contents->file_metadata.push_back(metadata.release());
      continue;
    }

    std::string tracker_id_str;
    if (RemovePrefix(key, kFileTrackerKeyPrefix, &tracker_id_str)) {
      int64 tracker_id = 0;
      if (!base::StringToInt64(tracker_id_str, &tracker_id)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse TrackerID");
        continue;
      }

      scoped_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }
      contents->file_trackers.push_back(tracker.release());
      continue;
    }
  }
}

void RemoveUnreachableItems(DatabaseContents* contents,
                            int64 sync_root_tracker_id,
                            leveldb::WriteBatch* batch) {
  typedef std::map<int64, std::set<int64> > ChildTrackersByParent;
  ChildTrackersByParent trackers_by_parent;

  // Set up links from parent tracker to child trackers.
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    const FileTracker& tracker = *contents->file_trackers[i];
    int64 parent_tracker_id = tracker.parent_tracker_id();
    int64 tracker_id = tracker.tracker_id();

    trackers_by_parent[parent_tracker_id].insert(tracker_id);
  }

  // Drop links from inactive trackers.
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    const FileTracker& tracker = *contents->file_trackers[i];

    if (!tracker.active())
      trackers_by_parent.erase(tracker.tracker_id());
  }

  std::vector<int64> pending;
  if (sync_root_tracker_id != kInvalidTrackerID)
    pending.push_back(sync_root_tracker_id);

  // Traverse tracker tree from sync-root.
  std::set<int64> visited_trackers;
  while (!pending.empty()) {
    int64 tracker_id = pending.back();
    DCHECK_NE(kInvalidTrackerID, tracker_id);
    pending.pop_back();

    if (!visited_trackers.insert(tracker_id).second) {
      NOTREACHED();
      continue;
    }

    AppendContents(
        LookUpMap(trackers_by_parent, tracker_id, std::set<int64>()),
        &pending);
  }

  // Delete all unreachable trackers.
  ScopedVector<FileTracker> reachable_trackers;
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    FileTracker* tracker = contents->file_trackers[i];
    if (ContainsKey(visited_trackers, tracker->tracker_id())) {
      reachable_trackers.push_back(tracker);
      contents->file_trackers[i] = NULL;
    } else {
      PutFileTrackerDeletionToBatch(tracker->tracker_id(), batch);
    }
  }
  contents->file_trackers = reachable_trackers.Pass();

  // List all |file_id| referred by a tracker.
  base::hash_set<std::string> referred_file_ids;
  for (size_t i = 0; i < contents->file_trackers.size(); ++i)
    referred_file_ids.insert(contents->file_trackers[i]->file_id());

  // Delete all unreferred metadata.
  ScopedVector<FileMetadata> referred_file_metadata;
  for (size_t i = 0; i < contents->file_metadata.size(); ++i) {
    FileMetadata* metadata = contents->file_metadata[i];
    if (ContainsKey(referred_file_ids, metadata->file_id())) {
      referred_file_metadata.push_back(metadata);
      contents->file_metadata[i] = NULL;
    } else {
      PutFileMetadataDeletionToBatch(metadata->file_id(), batch);
    }
  }
  contents->file_metadata = referred_file_metadata.Pass();
}

}  // namespace

// static
scoped_ptr<MetadataDatabaseIndex>
MetadataDatabaseIndex::Create(leveldb::DB* db, leveldb::WriteBatch* batch) {
  DCHECK(db);
  DCHECK(batch);

  scoped_ptr<ServiceMetadata> service_metadata = InitializeServiceMetadata(db);
  DatabaseContents contents;

  PutVersionToBatch(kCurrentDatabaseVersion, batch);
  ReadDatabaseContents(db, &contents);
  RemoveUnreachableItems(&contents,
                         service_metadata->sync_root_tracker_id(),
                         batch);

  scoped_ptr<MetadataDatabaseIndex> index(new MetadataDatabaseIndex);
  index->Initialize(service_metadata.Pass(), &contents);
  return index.Pass();
}

// static
scoped_ptr<MetadataDatabaseIndex>
MetadataDatabaseIndex::CreateForTesting(DatabaseContents* contents) {
  scoped_ptr<MetadataDatabaseIndex> index(new MetadataDatabaseIndex);
  index->Initialize(make_scoped_ptr(new ServiceMetadata), contents);
  return index.Pass();
}

void MetadataDatabaseIndex::Initialize(
    scoped_ptr<ServiceMetadata> service_metadata,
    DatabaseContents* contents) {
  service_metadata_ = service_metadata.Pass();

  for (size_t i = 0; i < contents->file_metadata.size(); ++i)
    StoreFileMetadata(make_scoped_ptr(contents->file_metadata[i]), NULL);
  contents->file_metadata.weak_clear();

  for (size_t i = 0; i < contents->file_trackers.size(); ++i)
    StoreFileTracker(make_scoped_ptr(contents->file_trackers[i]), NULL);
  contents->file_trackers.weak_clear();

  UMA_HISTOGRAM_COUNTS("SyncFileSystem.MetadataNumber", metadata_by_id_.size());
  UMA_HISTOGRAM_COUNTS("SyncFileSystem.TrackerNumber", tracker_by_id_.size());
  UMA_HISTOGRAM_COUNTS_100("SyncFileSystem.RegisteredAppNumber",
                           app_root_by_app_id_.size());
}

MetadataDatabaseIndex::MetadataDatabaseIndex() {}
MetadataDatabaseIndex::~MetadataDatabaseIndex() {}

bool MetadataDatabaseIndex::GetFileMetadata(
    const std::string& file_id, FileMetadata* metadata) const {
  FileMetadata* identified = metadata_by_id_.get(file_id);
  if (!identified)
    return false;
  if (metadata)
    metadata->CopyFrom(*identified);
  return true;
}

bool MetadataDatabaseIndex::GetFileTracker(
    int64 tracker_id, FileTracker* tracker) const {
  FileTracker* identified = tracker_by_id_.get(tracker_id);
  if (!identified)
    return false;
  if (tracker)
    tracker->CopyFrom(*identified);
  return true;
}

void MetadataDatabaseIndex::StoreFileMetadata(
    scoped_ptr<FileMetadata> metadata, leveldb::WriteBatch* batch) {
  PutFileMetadataToBatch(*metadata.get(), batch);
  if (!metadata) {
    NOTREACHED();
    return;
  }

  std::string file_id = metadata->file_id();
  metadata_by_id_.set(file_id, metadata.Pass());
}

void MetadataDatabaseIndex::StoreFileTracker(scoped_ptr<FileTracker> tracker,
                                             leveldb::WriteBatch* batch) {
  PutFileTrackerToBatch(*tracker.get(), batch);
  if (!tracker) {
    NOTREACHED();
    return;
  }

  int64 tracker_id = tracker->tracker_id();
  FileTracker* old_tracker = tracker_by_id_.get(tracker_id);

  if (!old_tracker) {
    DVLOG(3) << "Adding new tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);

    AddToAppIDIndex(*tracker);
    AddToPathIndexes(*tracker);
    AddToFileIDIndexes(*tracker);
    AddToDirtyTrackerIndexes(*tracker);
  } else {
    DVLOG(3) << "Updating tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);

    UpdateInAppIDIndex(*old_tracker, *tracker);
    UpdateInPathIndexes(*old_tracker, *tracker);
    UpdateInFileIDIndexes(*old_tracker, *tracker);
    UpdateInDirtyTrackerIndexes(*old_tracker, *tracker);
  }

  tracker_by_id_.set(tracker_id, tracker.Pass());
}

void MetadataDatabaseIndex::RemoveFileMetadata(const std::string& file_id,
                                               leveldb::WriteBatch* batch) {
  PutFileMetadataDeletionToBatch(file_id, batch);
  metadata_by_id_.erase(file_id);
}

void MetadataDatabaseIndex::RemoveFileTracker(int64 tracker_id,
                                              leveldb::WriteBatch* batch) {
  PutFileTrackerDeletionToBatch(tracker_id, batch);

  FileTracker* tracker = tracker_by_id_.get(tracker_id);
  if (!tracker) {
    NOTREACHED();
    return;
  }

  DVLOG(3) << "Removing tracker: "
           << tracker->tracker_id() << " " << GetTrackerTitle(*tracker);

  RemoveFromAppIDIndex(*tracker);
  RemoveFromPathIndexes(*tracker);
  RemoveFromFileIDIndexes(*tracker);
  RemoveFromDirtyTrackerIndexes(*tracker);

  tracker_by_id_.erase(tracker_id);
}

TrackerIDSet MetadataDatabaseIndex::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  return FindItem(trackers_by_file_id_, file_id);
}

int64 MetadataDatabaseIndex::GetAppRootTracker(
    const std::string& app_id) const {
  return FindItem(app_root_by_app_id_, app_id);
}

TrackerIDSet MetadataDatabaseIndex::GetFileTrackerIDsByParentAndTitle(
    int64 parent_tracker_id,
    const std::string& title) const {
  TrackerIDsByParentAndTitle::const_iterator found =
      trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found == trackers_by_parent_and_title_.end())
    return TrackerIDSet();
  return FindItem(found->second, title);
}

std::vector<int64> MetadataDatabaseIndex::GetFileTrackerIDsByParent(
    int64 parent_tracker_id) const {
  std::vector<int64> result;
  TrackerIDsByParentAndTitle::const_iterator found =
      trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found == trackers_by_parent_and_title_.end())
    return result;

  for (TrackerIDsByTitle::const_iterator itr = found->second.begin();
       itr != found->second.end(); ++itr) {
    result.insert(result.end(), itr->second.begin(), itr->second.end());
  }

  return result;
}

std::string MetadataDatabaseIndex::PickMultiTrackerFileID() const {
  if (multi_tracker_file_ids_.empty())
    return std::string();
  return *multi_tracker_file_ids_.begin();
}

ParentIDAndTitle MetadataDatabaseIndex::PickMultiBackingFilePath() const {
  if (multi_backing_file_paths_.empty())
    return ParentIDAndTitle(kInvalidTrackerID, std::string());
  return *multi_backing_file_paths_.begin();
}

int64 MetadataDatabaseIndex::PickDirtyTracker() const {
  if (dirty_trackers_.empty())
    return kInvalidTrackerID;
  return *dirty_trackers_.begin();
}

void MetadataDatabaseIndex::DemoteDirtyTracker(
    int64 tracker_id, leveldb::WriteBatch* /* unused_batch */) {
  if (dirty_trackers_.erase(tracker_id))
    demoted_dirty_trackers_.insert(tracker_id);
}

bool MetadataDatabaseIndex::HasDemotedDirtyTracker() const {
  return !demoted_dirty_trackers_.empty();
}

void MetadataDatabaseIndex::PromoteDemotedDirtyTrackers(
    leveldb::WriteBatch* /* unused_batch */) {
  dirty_trackers_.insert(demoted_dirty_trackers_.begin(),
                         demoted_dirty_trackers_.end());
  demoted_dirty_trackers_.clear();
}

size_t MetadataDatabaseIndex::CountDirtyTracker() const {
  return dirty_trackers_.size() + demoted_dirty_trackers_.size();
}

size_t MetadataDatabaseIndex::CountFileMetadata() const {
  return metadata_by_id_.size();
}

size_t MetadataDatabaseIndex::CountFileTracker() const {
  return tracker_by_id_.size();
}

void MetadataDatabaseIndex::SetSyncRootTrackerID(
    int64 sync_root_id, leveldb::WriteBatch* batch) const {
  service_metadata_->set_sync_root_tracker_id(sync_root_id);
  PutServiceMetadataToBatch(*service_metadata_, batch);
}

void MetadataDatabaseIndex::SetLargestChangeID(
    int64 largest_change_id, leveldb::WriteBatch* batch) const {
  service_metadata_->set_largest_change_id(largest_change_id);
  PutServiceMetadataToBatch(*service_metadata_, batch);
}

void MetadataDatabaseIndex::SetNextTrackerID(
    int64 next_tracker_id, leveldb::WriteBatch* batch) const {
  service_metadata_->set_next_tracker_id(next_tracker_id);
  PutServiceMetadataToBatch(*service_metadata_, batch);
}

int64 MetadataDatabaseIndex::GetSyncRootTrackerID() const {
  if (!service_metadata_->has_sync_root_tracker_id())
    return kInvalidTrackerID;
  return service_metadata_->sync_root_tracker_id();
}

int64 MetadataDatabaseIndex::GetLargestChangeID() const {
  if (!service_metadata_->has_largest_change_id())
    return kInvalidTrackerID;
  return service_metadata_->largest_change_id();
}

int64 MetadataDatabaseIndex::GetNextTrackerID() const {
  if (!service_metadata_->has_next_tracker_id()) {
    NOTREACHED();
    return kInvalidTrackerID;
  }
  return service_metadata_->next_tracker_id();
}

std::vector<std::string> MetadataDatabaseIndex::GetRegisteredAppIDs() const {
  std::vector<std::string> result;
  result.reserve(app_root_by_app_id_.size());
  for (TrackerIDByAppID::const_iterator itr = app_root_by_app_id_.begin();
       itr != app_root_by_app_id_.end(); ++itr)
    result.push_back(itr->first);
  return result;
}

std::vector<int64> MetadataDatabaseIndex::GetAllTrackerIDs() const {
  std::vector<int64> result;
  for (TrackerByID::const_iterator itr = tracker_by_id_.begin();
       itr != tracker_by_id_.end(); ++itr) {
    result.push_back(itr->first);
  }
  return result;
}

std::vector<std::string> MetadataDatabaseIndex::GetAllMetadataIDs() const {
  std::vector<std::string> result;
  for (MetadataByID::const_iterator itr = metadata_by_id_.begin();
       itr != metadata_by_id_.end(); ++itr) {
    result.push_back(itr->first);
  }
  return result;
}

void MetadataDatabaseIndex::AddToAppIDIndex(
    const FileTracker& new_tracker) {
  if (!IsAppRoot(new_tracker))
    return;

  DVLOG(3) << "  Add to app_root_by_app_id_: " << new_tracker.app_id();

  DCHECK(new_tracker.active());
  DCHECK(!ContainsKey(app_root_by_app_id_, new_tracker.app_id()));
  app_root_by_app_id_[new_tracker.app_id()] = new_tracker.tracker_id();
}

void MetadataDatabaseIndex::UpdateInAppIDIndex(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  if (IsAppRoot(old_tracker) && !IsAppRoot(new_tracker)) {
    DCHECK(old_tracker.active());
    DCHECK(!new_tracker.active());
    DCHECK(ContainsKey(app_root_by_app_id_, old_tracker.app_id()));

    DVLOG(3) << "  Remove from app_root_by_app_id_: " << old_tracker.app_id();

    app_root_by_app_id_.erase(old_tracker.app_id());
  } else if (!IsAppRoot(old_tracker) && IsAppRoot(new_tracker)) {
    DCHECK(!old_tracker.active());
    DCHECK(new_tracker.active());
    DCHECK(!ContainsKey(app_root_by_app_id_, new_tracker.app_id()));

    DVLOG(3) << "  Add to app_root_by_app_id_: " << new_tracker.app_id();

    app_root_by_app_id_[new_tracker.app_id()] = new_tracker.tracker_id();
  }
}

void MetadataDatabaseIndex::RemoveFromAppIDIndex(
    const FileTracker& tracker) {
  if (IsAppRoot(tracker)) {
    DCHECK(tracker.active());
    DCHECK(ContainsKey(app_root_by_app_id_, tracker.app_id()));

    DVLOG(3) << "  Remove from app_root_by_app_id_: " << tracker.app_id();

    app_root_by_app_id_.erase(tracker.app_id());
  }
}

void MetadataDatabaseIndex::AddToFileIDIndexes(
    const FileTracker& new_tracker) {
  DVLOG(3) << "  Add to trackers_by_file_id_: " << new_tracker.file_id();

  trackers_by_file_id_[new_tracker.file_id()].Insert(new_tracker);

  if (trackers_by_file_id_[new_tracker.file_id()].size() > 1) {
    DVLOG_IF(3, !ContainsKey(multi_tracker_file_ids_, new_tracker.file_id()))
        << "  Add to multi_tracker_file_ids_: " << new_tracker.file_id();
    multi_tracker_file_ids_.insert(new_tracker.file_id());
  }
}

void MetadataDatabaseIndex::UpdateInFileIDIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.file_id(), new_tracker.file_id());

  std::string file_id = new_tracker.file_id();
  DCHECK(ContainsKey(trackers_by_file_id_, file_id));

  if (old_tracker.active() && !new_tracker.active())
    trackers_by_file_id_[file_id].Deactivate(new_tracker.tracker_id());
  else if (!old_tracker.active() && new_tracker.active())
    trackers_by_file_id_[file_id].Activate(new_tracker.tracker_id());
}

void MetadataDatabaseIndex::RemoveFromFileIDIndexes(
    const FileTracker& tracker) {
  TrackerIDsByFileID::iterator found =
      trackers_by_file_id_.find(tracker.file_id());
  if (found == trackers_by_file_id_.end()) {
    NOTREACHED();
    return;
  }

  DVLOG(3) << "  Remove from trackers_by_file_id_: "
           << tracker.tracker_id();
  found->second.Erase(tracker.tracker_id());

  if (trackers_by_file_id_[tracker.file_id()].size() <= 1) {
    DVLOG_IF(3, ContainsKey(multi_tracker_file_ids_, tracker.file_id()))
        << "  Remove from multi_tracker_file_ids_: " << tracker.file_id();
    multi_tracker_file_ids_.erase(tracker.file_id());
  }

  if (found->second.empty())
    trackers_by_file_id_.erase(found);
}

void MetadataDatabaseIndex::AddToPathIndexes(
    const FileTracker& new_tracker) {
  int64 parent = new_tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(new_tracker);

  DVLOG(3) << "  Add to trackers_by_parent_and_title_: "
           << parent << " " << title;

  trackers_by_parent_and_title_[parent][title].Insert(new_tracker);

  if (trackers_by_parent_and_title_[parent][title].size() > 1 &&
      !title.empty()) {
    DVLOG_IF(3, !ContainsKey(multi_backing_file_paths_,
                             ParentIDAndTitle(parent, title)))
        << "  Add to multi_backing_file_paths_: " << parent << " " << title;
    multi_backing_file_paths_.insert(ParentIDAndTitle(parent, title));
  }
}

void MetadataDatabaseIndex::UpdateInPathIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.parent_tracker_id(), new_tracker.parent_tracker_id());
  DCHECK(GetTrackerTitle(old_tracker) == GetTrackerTitle(new_tracker) ||
         !old_tracker.has_synced_details());

  int64 tracker_id = new_tracker.tracker_id();
  int64 parent = new_tracker.parent_tracker_id();
  std::string old_title = GetTrackerTitle(old_tracker);
  std::string title = GetTrackerTitle(new_tracker);

  TrackerIDsByTitle* trackers_by_title = &trackers_by_parent_and_title_[parent];

  if (old_title != title) {
    TrackerIDsByTitle::iterator found = trackers_by_title->find(old_title);
    if (found != trackers_by_title->end()) {
      DVLOG(3) << "  Remove from trackers_by_parent_and_title_: "
             << parent << " " << old_title;

      found->second.Erase(tracker_id);
      if (found->second.empty())
        trackers_by_title->erase(found);
    } else {
      NOTREACHED();
    }

    DVLOG(3) << "  Add to trackers_by_parent_and_title_: "
             << parent << " " << title;

    (*trackers_by_title)[title].Insert(new_tracker);

    if (trackers_by_parent_and_title_[parent][old_title].size() <= 1 &&
        !old_title.empty()) {
      DVLOG_IF(3, ContainsKey(multi_backing_file_paths_,
                              ParentIDAndTitle(parent, old_title)))
          << "  Remove from multi_backing_file_paths_: "
          << parent << " " << old_title;
      multi_backing_file_paths_.erase(ParentIDAndTitle(parent, old_title));
    }

    if (trackers_by_parent_and_title_[parent][title].size() > 1 &&
        !title.empty()) {
      DVLOG_IF(3, !ContainsKey(multi_backing_file_paths_,
                               ParentIDAndTitle(parent, title)))
          << "  Add to multi_backing_file_paths_: " << parent << " " << title;
      multi_backing_file_paths_.insert(ParentIDAndTitle(parent, title));
    }

    return;
  }

  if (old_tracker.active() && !new_tracker.active())
    trackers_by_parent_and_title_[parent][title].Deactivate(tracker_id);
  else if (!old_tracker.active() && new_tracker.active())
    trackers_by_parent_and_title_[parent][title].Activate(tracker_id);
}

void MetadataDatabaseIndex::RemoveFromPathIndexes(
    const FileTracker& tracker) {
  int64 tracker_id = tracker.tracker_id();
  int64 parent = tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(tracker);

  DCHECK(ContainsKey(trackers_by_parent_and_title_, parent));
  DCHECK(ContainsKey(trackers_by_parent_and_title_[parent], title));

  DVLOG(3) << "  Remove from trackers_by_parent_and_title_: "
           << parent << " " << title;

  trackers_by_parent_and_title_[parent][title].Erase(tracker_id);

  if (trackers_by_parent_and_title_[parent][title].size() <= 1 &&
      !title.empty()) {
    DVLOG_IF(3, ContainsKey(multi_backing_file_paths_,
                            ParentIDAndTitle(parent, title)))
        << "  Remove from multi_backing_file_paths_: "
        << parent << " " << title;
    multi_backing_file_paths_.erase(ParentIDAndTitle(parent, title));
  }

  if (trackers_by_parent_and_title_[parent][title].empty()) {
    trackers_by_parent_and_title_[parent].erase(title);
    if (trackers_by_parent_and_title_[parent].empty())
      trackers_by_parent_and_title_.erase(parent);
  }
}

void MetadataDatabaseIndex::AddToDirtyTrackerIndexes(
    const FileTracker& new_tracker) {
  DCHECK(!ContainsKey(dirty_trackers_, new_tracker.tracker_id()));
  DCHECK(!ContainsKey(demoted_dirty_trackers_, new_tracker.tracker_id()));

  if (new_tracker.dirty()) {
    DVLOG(3) << "  Add to dirty_trackers_: " << new_tracker.tracker_id();
    dirty_trackers_.insert(new_tracker.tracker_id());
  }
}

void MetadataDatabaseIndex::UpdateInDirtyTrackerIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  int64 tracker_id = new_tracker.tracker_id();
  if (old_tracker.dirty() && !new_tracker.dirty()) {
    DCHECK(ContainsKey(dirty_trackers_, tracker_id) ||
           ContainsKey(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Remove from dirty_trackers_: " << tracker_id;

    dirty_trackers_.erase(tracker_id);
    demoted_dirty_trackers_.erase(tracker_id);
  } else if (!old_tracker.dirty() && new_tracker.dirty()) {
    DCHECK(!ContainsKey(dirty_trackers_, tracker_id));
    DCHECK(!ContainsKey(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Add to dirty_trackers_: " << tracker_id;

    dirty_trackers_.insert(tracker_id);
  }
}

void MetadataDatabaseIndex::RemoveFromDirtyTrackerIndexes(
    const FileTracker& tracker) {
  if (tracker.dirty()) {
    int64 tracker_id = tracker.tracker_id();
    DCHECK(ContainsKey(dirty_trackers_, tracker_id) ||
           ContainsKey(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Remove from dirty_trackers_: " << tracker_id;
    dirty_trackers_.erase(tracker_id);

    demoted_dirty_trackers_.erase(tracker_id);
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
