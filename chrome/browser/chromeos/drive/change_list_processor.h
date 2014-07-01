// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "url/gurl.h"

namespace google_apis {
class AboutResource;
class ChangeList;
class FileList;
}  // google_apis

namespace drive {

class FileChange;
class ResourceEntry;

namespace internal {

class ResourceMetadata;

// Holds information needed to fetch contents of a directory.
// This object is copyable.
class DirectoryFetchInfo {
 public:
  DirectoryFetchInfo() : changestamp_(0) {}
  DirectoryFetchInfo(const std::string& local_id,
                     const std::string& resource_id,
                     int64 changestamp)
      : local_id_(local_id),
        resource_id_(resource_id),
        changestamp_(changestamp) {
  }

  // Returns true if the object is empty.
  bool empty() const { return local_id_.empty(); }

  // Local ID of the directory.
  const std::string& local_id() const { return local_id_; }

  // Resource ID of the directory.
  const std::string& resource_id() const { return resource_id_; }

  // Changestamp of the directory. The changestamp is used to determine if
  // the directory contents should be fetched.
  int64 changestamp() const { return changestamp_; }

  // Returns a string representation of this object.
  std::string ToString() const;

 private:
  const std::string local_id_;
  const std::string resource_id_;
  const int64 changestamp_;
};

// Class to represent a change list.
class ChangeList {
 public:
  ChangeList();  // For tests.
  explicit ChangeList(const google_apis::ChangeList& change_list);
  explicit ChangeList(const google_apis::FileList& file_list);
  ~ChangeList();

  const std::vector<ResourceEntry>& entries() const { return entries_; }
  std::vector<ResourceEntry>* mutable_entries() { return &entries_; }
  const std::vector<std::string>& parent_resource_ids() const {
    return parent_resource_ids_;
  }
  std::vector<std::string>* mutable_parent_resource_ids() {
    return &parent_resource_ids_;
  }
  const GURL& next_url() const { return next_url_; }
  int64 largest_changestamp() const { return largest_changestamp_; }

  void set_largest_changestamp(int64 largest_changestamp) {
    largest_changestamp_ = largest_changestamp;
  }

 private:
  std::vector<ResourceEntry> entries_;
  std::vector<std::string> parent_resource_ids_;
  GURL next_url_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(ChangeList);
};

// ChangeListProcessor is used to process change lists, or full resource
// lists from WAPI (codename for Documents List API) or Google Drive API, and
// updates the resource metadata stored locally.
class ChangeListProcessor {
 public:
  explicit ChangeListProcessor(ResourceMetadata* resource_metadata);
  ~ChangeListProcessor();

  // Applies change lists or full resource lists to |resource_metadata_|.
  //
  // |is_delta_update| determines the type of input data to process, whether
  // it is full resource lists (false) or change lists (true).
  //
  // Must be run on the same task runner as |resource_metadata_| uses.
  FileError Apply(scoped_ptr<google_apis::AboutResource> about_resource,
                  ScopedVector<ChangeList> change_lists,
                  bool is_delta_update);

  // The set of changed files as a result of change list processing.
  const FileChange& changed_files() const { return *changed_files_; }

  // Adds or refreshes the child entries from |change_list| to the directory.
  static FileError RefreshDirectory(
      ResourceMetadata* resource_metadata,
      const DirectoryFetchInfo& directory_fetch_info,
      scoped_ptr<ChangeList> change_list,
      std::vector<ResourceEntry>* out_refreshed_entries);

  // Sets |entry|'s parent_local_id.
  static FileError SetParentLocalIdOfEntry(
      ResourceMetadata* resource_metadata,
      ResourceEntry* entry,
      const std::string& parent_resource_id);

 private:
  typedef std::map<std::string /* resource_id */, ResourceEntry>
      ResourceEntryMap;
  typedef std::map<std::string /* resource_id */,
                   std::string /* parent_resource_id*/> ParentResourceIdMap;

  // Applies the pre-processed metadata from entry_map_ onto the resource
  // metadata. |about_resource| must not be null.
  FileError ApplyEntryMap(
      int64 changestamp,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Apply |entry| to resource_metadata_.
  FileError ApplyEntry(const ResourceEntry& entry);

  // Adds the directories changed by the update on |entry| to |changed_dirs_|.
  void UpdateChangedDirs(const ResourceEntry& entry);

  ResourceMetadata* resource_metadata_;  // Not owned.

  ResourceEntryMap entry_map_;
  ParentResourceIdMap parent_resource_id_map_;
  scoped_ptr<FileChange> changed_files_;

  DISALLOW_COPY_AND_ASSIGN(ChangeListProcessor);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_PROCESSOR_H_
