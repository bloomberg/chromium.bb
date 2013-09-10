// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/remote_change_handler.h"

#include "base/logging.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

RemoteChangeHandler::ChangeQueueItem::ChangeQueueItem()
    : changestamp(0) {}

RemoteChangeHandler::ChangeQueueItem::ChangeQueueItem(
    int64 changestamp,
    const fileapi::FileSystemURL& url)
    : changestamp(changestamp), url(url) {}

bool RemoteChangeHandler::ChangeQueueComparator::operator()(
    const ChangeQueueItem& left,
    const ChangeQueueItem& right) {
  if (left.changestamp != right.changestamp)
    return left.changestamp < right.changestamp;
  return fileapi::FileSystemURL::Comparator()(left.url, right.url);
}

RemoteChangeHandler::RemoteChange::RemoteChange()
    : changestamp(0),
      change(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_UNKNOWN) {}

RemoteChangeHandler::RemoteChange::RemoteChange(
    int64 changestamp,
    const std::string& resource_id,
    const std::string& md5_checksum,
    const base::Time& updated_time,
    const fileapi::FileSystemURL& url,
    const FileChange& change)
    : changestamp(changestamp),
      resource_id(resource_id),
      md5_checksum(md5_checksum),
      updated_time(updated_time),
      url(url),
      change(change) {}

RemoteChangeHandler::RemoteChange::~RemoteChange() {}

bool RemoteChangeHandler::RemoteChangeComparator::operator()(
    const RemoteChange& left,
    const RemoteChange& right) {
  // This should return true if |right| has higher priority than |left|.
  // Smaller changestamps have higher priorities (i.e. need to be processed
  // earlier).
  if (left.changestamp != right.changestamp)
    return left.changestamp > right.changestamp;
  return false;
}

RemoteChangeHandler::RemoteChangeHandler() {}

RemoteChangeHandler::~RemoteChangeHandler() {}

bool RemoteChangeHandler::GetChange(RemoteChange* remote_change) {
  const fileapi::FileSystemURL& url = changes_.begin()->url;
  const base::FilePath::StringType& normalized_path =
      fileapi::VirtualPath::GetNormalizedFilePath(url.path());
  DCHECK(ContainsKey(origin_to_changes_map_, url.origin()));
  PathToChangeMap* path_to_change = &origin_to_changes_map_[url.origin()];
  DCHECK(ContainsKey(*path_to_change, normalized_path));
  *remote_change = (*path_to_change)[normalized_path].remote_change;
  return true;
}

bool RemoteChangeHandler::GetChangeForURL(
    const fileapi::FileSystemURL& url,
    RemoteChange* remote_change) {
  OriginToChangesMap::iterator found_origin =
      origin_to_changes_map_.find(url.origin());
  if (found_origin == origin_to_changes_map_.end())
    return false;

  PathToChangeMap* path_to_change = &found_origin->second;
  PathToChangeMap::iterator found_change = path_to_change->find(
      fileapi::VirtualPath::GetNormalizedFilePath(url.path()));
  if (found_change == path_to_change->end())
    return false;

  *remote_change = found_change->second.remote_change;
  return true;
}

void RemoteChangeHandler::AppendChange(const RemoteChange& remote_change) {
  base::FilePath::StringType normalized_path =
      fileapi::VirtualPath::GetNormalizedFilePath(remote_change.url.path());

  PathToChangeMap* path_to_change =
      &origin_to_changes_map_[remote_change.url.origin()];
  PathToChangeMap::iterator found = path_to_change->find(normalized_path);

  // Remove an existing change for |remote_change.url.path()|.
  if (found != path_to_change->end())
    changes_.erase(found->second.position_in_queue);

  std::pair<PendingChangeQueue::iterator, bool> inserted_to_queue =
      changes_.insert(ChangeQueueItem(remote_change.changestamp,
                                      remote_change.url));
  DCHECK(inserted_to_queue.second);

  (*path_to_change)[normalized_path] =
      ChangeMapItem(remote_change, inserted_to_queue.first);
}

bool RemoteChangeHandler::RemoveChangeForURL(
    const fileapi::FileSystemURL& url) {
  OriginToChangesMap::iterator found_origin =
      origin_to_changes_map_.find(url.origin());
  if (found_origin == origin_to_changes_map_.end())
    return false;

  PathToChangeMap* path_to_change = &found_origin->second;
  PathToChangeMap::iterator found_change = path_to_change->find(
      fileapi::VirtualPath::GetNormalizedFilePath(url.path()));
  if (found_change == path_to_change->end())
    return false;

  changes_.erase(found_change->second.position_in_queue);
  path_to_change->erase(found_change);
  if (path_to_change->empty())
    origin_to_changes_map_.erase(found_origin);
  return true;
}

void RemoteChangeHandler::RemoveChangesForOrigin(const GURL& origin) {
  OriginToChangesMap::iterator found = origin_to_changes_map_.find(origin);
  if (found == origin_to_changes_map_.end())
    return;
  for (PathToChangeMap::iterator itr = found->second.begin();
       itr != found->second.end(); ++itr)
    changes_.erase(itr->second.position_in_queue);
  origin_to_changes_map_.erase(found);
}

bool RemoteChangeHandler::HasChangesForOrigin(const GURL& origin) const {
  return ContainsKey(origin_to_changes_map_, origin);
}

RemoteChangeHandler::ChangeMapItem::ChangeMapItem() {}

RemoteChangeHandler::ChangeMapItem::ChangeMapItem(
    const RemoteChange& remote_change,
    PendingChangeQueue::iterator position_in_queue)
  : remote_change(remote_change),
    position_in_queue(position_in_queue) {}

RemoteChangeHandler::ChangeMapItem::~ChangeMapItem() {}

}  // namespace sync_file_system
