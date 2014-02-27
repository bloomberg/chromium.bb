// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"

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

bool IsAppRoot(const FileTracker& tracker) {
  return tracker.tracker_kind() == TRACKER_KIND_APP_ROOT ||
      tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
}

std::string GetTrackerTitle(const FileTracker& tracker) {
  if (tracker.has_synced_details())
    return tracker.synced_details().title();
  return std::string();
}

}  // namespace

MetadataDatabaseIndex::MetadataDatabaseIndex(DatabaseContents* content) {
  for (size_t i = 0; i < content->file_metadata.size(); ++i)
    StoreFileMetadata(make_scoped_ptr(content->file_metadata[i]));
  content->file_metadata.weak_clear();

  for (size_t i = 0; i < content->file_trackers.size(); ++i)
    StoreFileTracker(make_scoped_ptr(content->file_trackers[i]));
  content->file_trackers.weak_clear();
}

MetadataDatabaseIndex::~MetadataDatabaseIndex() {}

const FileTracker* MetadataDatabaseIndex::GetFileTracker(
    int64 tracker_id) const {
  return tracker_by_id_.get(tracker_id);
}

const FileMetadata* MetadataDatabaseIndex::GetFileMetadata(
    const std::string& file_id) const {
  return metadata_by_id_.get(file_id);
}

void MetadataDatabaseIndex::StoreFileMetadata(
    scoped_ptr<FileMetadata> metadata) {
  if (!metadata) {
    NOTREACHED();
    return;
  }

  std::string file_id = metadata->file_id();
  metadata_by_id_.set(file_id, metadata.Pass());
}

void MetadataDatabaseIndex::StoreFileTracker(scoped_ptr<FileTracker> tracker) {
  if (!tracker) {
    NOTREACHED();
    return;
  }

  int64 tracker_id = tracker->tracker_id();
  FileTracker* old_tracker = tracker_by_id_.get(tracker_id);

  if (!old_tracker) {
    AddToAppIDIndex(*tracker);
    AddToPathIndexes(*tracker);
    AddToFileIDIndexes(*tracker);
    AddToDirtyTrackerIndexes(*tracker);
  } else {
    UpdateInAppIDIndex(*old_tracker, *tracker);
    UpdateInPathIndexes(*old_tracker, *tracker);
    UpdateInFileIDIndexes(*old_tracker, *tracker);
    UpdateInDirtyTrackerIndexes(*old_tracker, *tracker);
  }

  tracker_by_id_.set(tracker_id, tracker.Pass());
}

void MetadataDatabaseIndex::RemoveFileMetadata(const std::string& file_id) {
  metadata_by_id_.erase(file_id);
}

void MetadataDatabaseIndex::RemoveFileTracker(int64 tracker_id) {
  FileTracker* tracker = tracker_by_id_.get(tracker_id);
  if (!tracker) {
    NOTREACHED();
    return;
  }

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

void MetadataDatabaseIndex::DemoteDirtyTracker(int64 tracker_id) {
  if (dirty_trackers_.erase(tracker_id))
    demoted_dirty_trackers_.insert(tracker_id);
}

void MetadataDatabaseIndex::PromoteDemotedDirtyTrackers() {
  dirty_trackers_.insert(demoted_dirty_trackers_.begin(),
                         demoted_dirty_trackers_.end());
  demoted_dirty_trackers_.clear();
}

void MetadataDatabaseIndex::AddToAppIDIndex(
    const FileTracker& new_tracker) {
  if (!IsAppRoot(new_tracker))
    return;

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
    app_root_by_app_id_.erase(old_tracker.app_id());
  } else if (!IsAppRoot(old_tracker) && IsAppRoot(new_tracker)) {
    DCHECK(!old_tracker.active());
    DCHECK(new_tracker.active());
    DCHECK(!ContainsKey(app_root_by_app_id_, new_tracker.app_id()));
    app_root_by_app_id_[new_tracker.app_id()] = new_tracker.tracker_id();
  }
}

void MetadataDatabaseIndex::RemoveFromAppIDIndex(
    const FileTracker& tracker) {
  if (IsAppRoot(tracker)) {
    DCHECK(tracker.active());
    DCHECK(ContainsKey(app_root_by_app_id_, tracker.app_id()));
    app_root_by_app_id_.erase(tracker.app_id());
  }
}

void MetadataDatabaseIndex::AddToFileIDIndexes(
    const FileTracker& new_tracker) {
  trackers_by_file_id_[new_tracker.file_id()].Insert(new_tracker);

  if (trackers_by_file_id_[new_tracker.file_id()].size() > 1)
    multi_tracker_file_ids_.insert(new_tracker.file_id());
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
  DCHECK(ContainsKey(trackers_by_file_id_, tracker.file_id()));
  trackers_by_file_id_.erase(tracker.file_id());

  if (trackers_by_file_id_[tracker.file_id()].size() <= 1)
    multi_tracker_file_ids_.erase(tracker.file_id());
}

void MetadataDatabaseIndex::AddToPathIndexes(
    const FileTracker& new_tracker) {
  int64 parent = new_tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(new_tracker);
  trackers_by_parent_and_title_[parent][title].Insert(new_tracker);

  if (trackers_by_parent_and_title_[parent][title].size() > 1)
    multi_backing_file_paths_.insert(ParentIDAndTitle(parent, title));
}

void MetadataDatabaseIndex::UpdateInPathIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.parent_tracker_id(), new_tracker.parent_tracker_id());
  DCHECK_EQ(GetTrackerTitle(old_tracker), GetTrackerTitle(new_tracker));

  int64 tracker_id = new_tracker.tracker_id();
  int64 parent = new_tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(new_tracker);

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
  trackers_by_parent_and_title_[parent][title].Erase(tracker_id);

  if (trackers_by_parent_and_title_[parent][title].size() <= 1)
    multi_backing_file_paths_.erase(ParentIDAndTitle(parent, title));

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

  if (new_tracker.dirty())
    dirty_trackers_.insert(new_tracker.tracker_id());
}

void MetadataDatabaseIndex::UpdateInDirtyTrackerIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  int64 tracker_id = new_tracker.tracker_id();
  if (old_tracker.dirty() && !new_tracker.dirty()) {
    DCHECK(ContainsKey(dirty_trackers_, tracker_id) ||
           ContainsKey(demoted_dirty_trackers_, tracker_id));
    dirty_trackers_.erase(tracker_id);
    demoted_dirty_trackers_.erase(tracker_id);
  } else if (!old_tracker.dirty() && new_tracker.dirty()) {
    DCHECK(!ContainsKey(dirty_trackers_, tracker_id));
    DCHECK(!ContainsKey(demoted_dirty_trackers_, tracker_id));
    dirty_trackers_.insert(tracker_id);
  }
}

void MetadataDatabaseIndex::RemoveFromDirtyTrackerIndexes(
    const FileTracker& tracker) {
  if (tracker.dirty()) {
    int64 tracker_id = tracker.tracker_id();
    DCHECK(ContainsKey(dirty_trackers_, tracker_id) ||
           ContainsKey(demoted_dirty_trackers_, tracker_id));
    dirty_trackers_.erase(tracker_id);
    demoted_dirty_trackers_.erase(tracker_id);
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
