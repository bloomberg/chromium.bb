// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "googleurl/src/gurl.h"

namespace google_apis {
class ResourceList;
}  // google_apis

namespace drive {

class DriveEntryProto;
class DriveResourceMetadata;

// ChangeListProcessor is used to process feeds from WAPI (codename for
// Documents List API) or Google Drive API.
class ChangeListProcessor {
 public:
  typedef std::map<std::string /* resource_id */, DriveEntryProto>
      DriveEntryProtoMap;

  // Class used to record UMA stats with FeedToEntryProtoMap().
  class ChangeListToEntryProtoMapUMAStats;

  explicit ChangeListProcessor(DriveResourceMetadata* resource_metadata);
  ~ChangeListProcessor();

  // Applies the documents feeds to the file system using |resource_metadata_|.
  //
  // |is_delta_feed| determines the type of feed to process, whether it is a
  // root feed (false) or a delta feed (true).
  //
  // In the case of processing the root feeds |root_feed_changestamp| is used
  // as its initial changestamp value. The value comes from
  // google_apis::AccountMetadata.
  // |on_complete_callback| is run after the feed is applied.
  // |on_complete_callback| must not be null.
  // TODO(achuith): Change the type of on_complete_callback to
  // FileOperationCallback instead.
  void ApplyFeeds(
      scoped_ptr<google_apis::AboutResource> about_resource,
      const ScopedVector<google_apis::ResourceList>& feed_list,
      bool is_delta_feed,
      const base::Closure& on_complete_callback);

  // Converts list of document feeds from collected feeds into a
  // DriveEntryProtoMap. |feed_changestamp| and/or |uma_stats| may be NULL.
  // entry_proto_map_ and root_upload_url_ are updated as side effects.
  void FeedToEntryProtoMap(
      const ScopedVector<google_apis::ResourceList>& feed_list,
      int64* feed_changestamp,
      ChangeListToEntryProtoMapUMAStats* uma_stats);

  // A map of DriveEntryProto's representing a feed.
  const DriveEntryProtoMap& entry_proto_map() const { return entry_proto_map_; }

  // The set of changed directories as a result of feed processing.
  const std::set<base::FilePath>& changed_dirs() const { return changed_dirs_; }

 private:
  // Applies the pre-processed feed from entry_proto_map_ onto the filesystem.
  void ApplyEntryProtoMap(bool is_delta_feed);

  // Apply the next item from entry_proto_map_ to the file system. The async
  // version posts to the message loop to avoid recursive stack-overflow.
  void ApplyNextEntryProto();
  void ApplyNextEntryProtoAsync();

  // Apply |entry_proto| to resource_metadata_.
  void ApplyEntryProto(const DriveEntryProto& entry_proto);

  // Continue ApplyEntryProto. This is a callback for
  // DriveResourceMetadata::GetEntryInfoByResourceId.
  void ContinueApplyEntryProto(
      const DriveEntryProto& entry_proto,
      DriveFileError error,
      const base::FilePath& file_path,
      scoped_ptr<DriveEntryProto> old_entry_proto);

  // Apply the DriveEntryProto pointed to by |it| to resource_metadata_.
  void ApplyNextByIterator(DriveEntryProtoMap::iterator it);

  // Helper function to add |entry_proto| to its parent. Updates changed_dirs_
  // as a side effect.
  void AddEntry(const DriveEntryProto& entry_proto);

  // Callback for DriveResourceMetadata::AddEntry.
  void NotifyForAddEntry(bool is_directory,
                         DriveFileError error,
                         const base::FilePath& file_path);

  // Removes entry pointed to by |resource_id| from its parent. Updates
  // changed_dirs_ as a side effect.
  void RemoveEntryFromParent(
      const DriveEntryProto& entry_proto,
      const base::FilePath& file_path);

  // Continues RemoveEntryFromParent after
  // DriveResourceMetadata::GetChildDirectories.
  void OnGetChildrenForRemove(
      const DriveEntryProto& entry_proto,
      const base::FilePath& file_path,
      const std::set<base::FilePath>& child_directories);

  // Callback for DriveResourceMetadata::RemoveEntryFromParent.
  void NotifyForRemoveEntryFromParent(
      bool is_directory,
      const base::FilePath& file_path,
      const std::set<base::FilePath>& child_directories,
      DriveFileError error,
      const base::FilePath& parent_path);

  // Refreshes DriveResourceMetadata entry that has the same resource_id as
  // |entry_proto| with |entry_proto|. Updates changed_dirs_ as a side effect.
  void RefreshEntry(const DriveEntryProto& entry_proto,
                    const base::FilePath& file_path);

  // Callback for DriveResourceMetadata::RefreshEntry.
  void NotifyForRefreshEntry(
      const base::FilePath& old_file_path,
      DriveFileError error,
      const base::FilePath& file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Updates the root directory entry. upload_url and changestamp will be
  // updated. Calls |closure| upon completion regardless of whether the
  // update was successful or not.  |closure| must not be null.
  void UpdateRootEntry(const base::Closure& closure);

  // Part of UpdateRootEntry(). Called after
  // DriveResourceMetadata::GetEntryInfoByPath is complete. Updates the root
  // proto, and refreshes the root entry with the proto.
  void UpdateRootEntryAfterGetEntry(const base::Closure& closure,
                                    DriveFileError error,
                                    scoped_ptr<DriveEntryProto> root_proto);

  // Part of UpdateRootEntry(). Called after
  // DriveResourceMetadata::RefreshEntry() is complete. Calls OnComplete() to
  // finish the change list processing.
  void UpdateRootEntryAfterRefreshEntry(const base::Closure& closure,
                                        DriveFileError error,
                                        const base::FilePath& root_path,
                                        scoped_ptr<DriveEntryProto> root_proto);

  // Runs after all entries have been processed.
  void OnComplete();

  // Reset the state of this object.
  void Clear();

  DriveResourceMetadata* resource_metadata_;  // Not owned.

  DriveEntryProtoMap entry_proto_map_;
  std::set<base::FilePath> changed_dirs_;
  GURL root_upload_url_;
  int64 largest_changestamp_;
  base::Closure on_complete_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListProcessor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListProcessor);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_
