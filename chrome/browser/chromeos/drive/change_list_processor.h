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
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "googleurl/src/gurl.h"

namespace google_apis {
class ResourceList;
}  // google_apis

namespace drive {

class ResourceEntry;

namespace internal {
class ResourceMetadata;
}  // namespace internal

// Class to represent a change list.
class ChangeList {
 public:
  explicit ChangeList(const google_apis::ResourceList& resource_list);
  ~ChangeList();

  const std::vector<ResourceEntry>& entries() const { return entries_; }
  std::vector<ResourceEntry>* mutable_entries() { return &entries_; }
  const GURL& next_url() const { return next_url_; }
  int64 largest_changestamp() const { return largest_changestamp_; }

 private:
  std::vector<ResourceEntry> entries_;
  GURL next_url_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(ChangeList);
};

// ChangeListProcessor is used to process feeds from WAPI (codename for
// Documents List API) or Google Drive API.
class ChangeListProcessor {
 public:
  typedef std::map<std::string /* resource_id */, ResourceEntry>
      ResourceEntryMap;

  // Class used to record UMA stats with FeedToEntryProtoMap().
  class ChangeListToEntryProtoMapUMAStats;

  explicit ChangeListProcessor(internal::ResourceMetadata* resource_metadata);
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
  void ApplyFeeds(scoped_ptr<google_apis::AboutResource> about_resource,
                  ScopedVector<ChangeList> change_lists,
                  bool is_delta_feed,
                  const base::Closure& on_complete_callback);

  // Converts list of document feeds from collected feeds into a
  // ResourceEntryMap. |feed_changestamp| and/or |uma_stats| may be NULL.
  // entry_map_ is updated as side effects.
  void FeedToEntryProtoMap(ScopedVector<ChangeList> change_lists,
                           int64* feed_changestamp,
                           ChangeListToEntryProtoMapUMAStats* uma_stats);

  // A map of ResourceEntry's representing a feed.
  const ResourceEntryMap& entry_map() const { return entry_map_; }

  // The set of changed directories as a result of feed processing.
  const std::set<base::FilePath>& changed_dirs() const { return changed_dirs_; }

 private:
  // Applies the pre-processed feed from entry_map_ onto the filesystem.
  // If this is not delta feed update (i.e. |is_delta_feed| is false),
  // |about_resource| must not be null.
  void ApplyEntryProtoMap(
      bool is_delta_feed,
      scoped_ptr<google_apis::AboutResource> about_resource);
  void ApplyEntryProtoMapAfterReset(
      scoped_ptr<google_apis::AboutResource> about_resource,
      FileError error);

  // Apply the next item from entry_map_ to the file system. The async
  // version posts to the message loop to avoid recursive stack-overflow.
  void ApplyNextEntryProto();
  void ApplyNextEntryProtoAsync();

  // Apply |entry| to resource_metadata_.
  void ApplyEntryProto(const ResourceEntry& entry);

  // Continue ApplyEntryProto. This is a callback for
  // ResourceMetadata::GetEntryInfoByResourceId.
  void ContinueApplyEntryProto(
      const ResourceEntry& entry,
      FileError error,
      const base::FilePath& file_path,
      scoped_ptr<ResourceEntry> old_entry);

  // Apply the ResourceEntry pointed to by |it| to resource_metadata_.
  void ApplyNextByIterator(ResourceEntryMap::iterator it);

  // Helper function to add |entry| to its parent. Updates changed_dirs_
  // as a side effect.
  void AddEntry(const ResourceEntry& entry);

  // Callback for ResourceMetadata::AddEntry.
  void NotifyForAddEntry(bool is_directory,
                         FileError error,
                         const base::FilePath& file_path);

  // Removes entry pointed to by |resource_id| from its parent. Updates
  // changed_dirs_ as a side effect.
  void RemoveEntryFromParent(
      const ResourceEntry& entry,
      const base::FilePath& file_path);

  // Continues RemoveEntryFromParent after
  // ResourceMetadata::GetChildDirectories.
  void OnGetChildrenForRemove(
      const ResourceEntry& entry,
      const base::FilePath& file_path,
      const std::set<base::FilePath>& child_directories);

  // Callback for ResourceMetadata::RemoveEntryFromParent.
  void NotifyForRemoveEntryFromParent(
      bool is_directory,
      const base::FilePath& file_path,
      const std::set<base::FilePath>& child_directories,
      FileError error,
      const base::FilePath& parent_path);

  // Refreshes ResourceMetadata entry that has the same resource_id as
  // |entry| with |entry|. Updates changed_dirs_ as a side effect.
  void RefreshEntry(const ResourceEntry& entry,
                    const base::FilePath& file_path);

  // Callback for ResourceMetadata::RefreshEntry.
  void NotifyForRefreshEntry(
      const base::FilePath& old_file_path,
      FileError error,
      const base::FilePath& file_path,
      scoped_ptr<ResourceEntry> entry);

  // Updates the root directory entry. changestamp will be updated. Calls
  // |closure| upon completion regardless of whether the update was successful
  // or not.  |closure| must not be null.
  void UpdateRootEntry(const base::Closure& closure);

  // Part of UpdateRootEntry(). Called after
  // ResourceMetadata::GetEntryInfoByPath is complete. Updates the root
  // proto, and refreshes the root entry with the proto.
  void UpdateRootEntryAfterGetEntry(const base::Closure& closure,
                                    FileError error,
                                    scoped_ptr<ResourceEntry> root_proto);

  // Part of UpdateRootEntry(). Called after
  // ResourceMetadata::RefreshEntry() is complete. Calls OnComplete() to
  // finish the change list processing.
  void UpdateRootEntryAfterRefreshEntry(const base::Closure& closure,
                                        FileError error,
                                        const base::FilePath& root_path,
                                        scoped_ptr<ResourceEntry> root_proto);

  // Runs after all entries have been processed.
  void OnComplete();

  // Reset the state of this object.
  void Clear();

  internal::ResourceMetadata* resource_metadata_;  // Not owned.

  ResourceEntryMap entry_map_;
  std::set<base::FilePath> changed_dirs_;
  int64 largest_changestamp_;
  base::Closure on_complete_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListProcessor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListProcessor);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_
