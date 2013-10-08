// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

namespace drive {
namespace internal {

namespace {

class ChangeListToEntryMapUMAStats {
 public:
  ChangeListToEntryMapUMAStats()
    : num_regular_files_(0),
      num_hosted_documents_(0),
      num_shared_with_me_entries_(0) {
  }

  // Increments number of files.
  void IncrementNumFiles(bool is_hosted_document) {
    is_hosted_document ? num_hosted_documents_++ : num_regular_files_++;
  }

  // Increments number of shared-with-me entries.
  void IncrementNumSharedWithMeEntries() {
    num_shared_with_me_entries_++;
  }

  // Updates UMA histograms with file counts.
  void UpdateFileCountUmaHistograms() {
    const int num_total_files = num_hosted_documents_ + num_regular_files_;
    UMA_HISTOGRAM_COUNTS("Drive.NumberOfRegularFiles", num_regular_files_);
    UMA_HISTOGRAM_COUNTS("Drive.NumberOfHostedDocuments",
                         num_hosted_documents_);
    UMA_HISTOGRAM_COUNTS("Drive.NumberOfTotalFiles", num_total_files);
    UMA_HISTOGRAM_COUNTS("Drive.NumberOfSharedWithMeEntries",
                         num_shared_with_me_entries_);
  }

 private:
  int num_regular_files_;
  int num_hosted_documents_;
  int num_shared_with_me_entries_;
};

}  // namespace

std::string DirectoryFetchInfo::ToString() const {
  return ("resource_id: " + resource_id_ +
          ", changestamp: " + base::Int64ToString(changestamp_));
}

ChangeList::ChangeList() {}

ChangeList::ChangeList(const google_apis::ResourceList& resource_list)
    : largest_changestamp_(resource_list.largest_changestamp()) {
  resource_list.GetNextFeedURL(&next_url_);

  entries_.resize(resource_list.entries().size());
  parent_resource_ids_.resize(resource_list.entries().size());
  size_t entries_index = 0;
  for (size_t i = 0; i < resource_list.entries().size(); ++i) {
    if (ConvertToResourceEntry(*resource_list.entries()[i],
                               &entries_[entries_index],
                               &parent_resource_ids_[entries_index]))
      ++entries_index;
  }
  entries_.resize(entries_index);
  parent_resource_ids_.resize(entries_index);
}

ChangeList::~ChangeList() {}

ChangeListProcessor::ChangeListProcessor(ResourceMetadata* resource_metadata)
  : resource_metadata_(resource_metadata) {
}

ChangeListProcessor::~ChangeListProcessor() {
}

FileError ChangeListProcessor::Apply(
    scoped_ptr<google_apis::AboutResource> about_resource,
    ScopedVector<ChangeList> change_lists,
    bool is_delta_update) {
  DCHECK(about_resource);

  int64 largest_changestamp = 0;
  if (is_delta_update) {
    if (!change_lists.empty()) {
      // The changestamp appears in the first page of the change list.
      // The changestamp does not appear in the full resource list.
      largest_changestamp = change_lists[0]->largest_changestamp();
      DCHECK_GE(change_lists[0]->largest_changestamp(), 0);
    }
  } else {
    largest_changestamp = about_resource->largest_change_id();

    DVLOG(1) << "Root folder ID is " << about_resource->root_folder_id();
    DCHECK(!about_resource->root_folder_id().empty());
  }

  // Convert ChangeList to map.
  ChangeListToEntryMapUMAStats uma_stats;
  for (size_t i = 0; i < change_lists.size(); ++i) {
    ChangeList* change_list = change_lists[i];

    std::vector<ResourceEntry>* entries = change_list->mutable_entries();
    for (size_t i = 0; i < entries->size(); ++i) {
      ResourceEntry* entry = &(*entries)[i];

      // Count the number of files.
      if (!entry->file_info().is_directory()) {
        uma_stats.IncrementNumFiles(
            entry->file_specific_info().is_hosted_document());
        if (entry->shared_with_me())
          uma_stats.IncrementNumSharedWithMeEntries();
      }
      parent_resource_id_map_[entry->resource_id()] =
          change_list->parent_resource_ids()[i];
      entry_map_[entry->resource_id()].Swap(entry);
      LOG_IF(WARNING, !entry->resource_id().empty())
          << "Found duplicated file: " << entry->base_name();
    }
  }

  // Add the largest changestamp for directories.
  for (ResourceEntryMap::iterator it = entry_map_.begin();
       it != entry_map_.end(); ++it) {
    if (it->second.file_info().is_directory()) {
      it->second.mutable_directory_specific_info()->set_changestamp(
          largest_changestamp);
    }
  }

  FileError error = ApplyEntryMap(is_delta_update,
                                  largest_changestamp,
                                  about_resource.Pass());
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "ApplyEntryMap failed: " << FileErrorToString(error);
    return error;
  }

  // Update changestamp.
  error = resource_metadata_->SetLargestChangestamp(largest_changestamp);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "SetLargestChangeStamp failed: " << FileErrorToString(error);
    return error;
  }

  // Shouldn't record histograms when processing delta update.
  if (!is_delta_update)
    uma_stats.UpdateFileCountUmaHistograms();

  return FILE_ERROR_OK;
}

FileError ChangeListProcessor::ApplyEntryMap(
    bool is_delta_update,
    int64 changestamp,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(about_resource);

  // Create the entry for "My Drive" folder with the latest changestamp.
  ResourceEntry root =
      util::CreateMyDriveRootEntry(about_resource->root_folder_id());
  root.mutable_directory_specific_info()->set_changestamp(changestamp);

  if (!is_delta_update) {  // Full update.
    FileError error = resource_metadata_->Reset();
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to reset: " << FileErrorToString(error);
      return error;
    }

    changed_dirs_.insert(util::GetDriveGrandRootPath());
    changed_dirs_.insert(util::GetDriveMyDriveRootPath());

    // Add the My Drive root directory.
    std::string local_id;
    error = resource_metadata_->AddEntry(root, &local_id);
    if (error != FILE_ERROR_OK)
      return error;
  } else {
    // Refresh the existing root entry.
    std::string root_local_id;
    FileError error = resource_metadata_->GetIdByResourceId(root.resource_id(),
                                                            &root_local_id);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to get root entry: " << FileErrorToString(error);
      return error;
    }
    root.set_local_id(root_local_id);
    error = resource_metadata_->RefreshEntry(root);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to update root entry: " << FileErrorToString(error);
      return error;
    }
  }

  // Gather the set of changes in the old path.
  // Note that we want to notify the change in both old and new paths (suppose
  // /a/b/c is moved to /x/y/c. We want to notify both "/a/b" and "/x/y".)
  // The old paths must be calculated before we apply any actual changes.
  // The new paths are calculated after each change is applied. It correctly
  // sets the new path because we apply changes in such an order (see below).
  for (ResourceEntryMap::iterator it = entry_map_.begin();
       it != entry_map_.end(); ++it) {
    UpdateChangedDirs(it->second);
  }

  // Apply all entries except deleted ones to the metadata.
  std::vector<ResourceEntry> deleted_entries;
  deleted_entries.reserve(entry_map_.size());
  while (!entry_map_.empty()) {
    ResourceEntryMap::iterator it = entry_map_.begin();

    // Process deleted entries later to avoid deleting moved entries under it.
    if (it->second.deleted()) {
      deleted_entries.push_back(ResourceEntry());
      deleted_entries.back().Swap(&it->second);
      entry_map_.erase(it);
      continue;
    }

    // Start from entry_map_.begin() and traverse ancestors using the
    // parent-child relationships in the result (after this apply) tree.
    // Then apply the topmost change first.
    //
    // By doing this, assuming the result tree does not contain any cycles, we
    // can guarantee that no cycle is made during this apply (i.e. no entry gets
    // moved under any of its descendants) because the following conditions are
    // always satisfied in any move:
    // - The new parent entry is not a descendant of the moved entry.
    // - The new parent and its ancestors will no longer move during this apply.
    std::vector<ResourceEntryMap::iterator> entries;
    for (ResourceEntryMap::iterator it = entry_map_.begin();
         it != entry_map_.end();) {
      entries.push_back(it);

      const std::string& parent_resource_id =
          parent_resource_id_map_[it->first];
      DCHECK(!parent_resource_id.empty()) << it->first;

      ResourceEntryMap::iterator it_parent =
          entry_map_.find(parent_resource_id);
      if (it_parent == entry_map_.end()) {
        // Current entry's parent is already updated or not going to be updated,
        // get the parent from the local tree.
        std::string parent_local_id;
        FileError error = resource_metadata_->GetIdByResourceId(
            parent_resource_id, &parent_local_id);
        if (error != FILE_ERROR_OK) {
          LOG(ERROR) << "Failed to get local ID: " << parent_resource_id
                     << ", error = " << FileErrorToString(error);
          break;
        }
        ResourceEntry parent_entry;
        while (it_parent == entry_map_.end() && !parent_local_id.empty()) {
          error = resource_metadata_->GetResourceEntryById(
              parent_local_id, &parent_entry);
          if (error != FILE_ERROR_OK) {
            LOG(ERROR) << "Failed to get local entry: "
                       << FileErrorToString(error);
            break;
          }
          it_parent = entry_map_.find(parent_entry.resource_id());
          parent_local_id = parent_entry.parent_local_id();
        }
      }
      it = it_parent;
    }

    // Apply the parent first.
    std::reverse(entries.begin(), entries.end());
    for (size_t i = 0; i < entries.size(); ++i) {
      // Skip root entry in the change list. We don't expect servers to send
      // root entry, but we should better be defensive (see crbug.com/297259).
      ResourceEntryMap::iterator it = entries[i];
      if (it->first != root.resource_id()) {
        // TODO(hashimoto): Handle ApplyEntry errors correctly.
        FileError error = ApplyEntry(it->second);
        DLOG_IF(WARNING, error != FILE_ERROR_OK)
            << "ApplyEntry failed: " << FileErrorToString(error)
            << ", title = " << it->second.title();
      }
      entry_map_.erase(it);
    }
  }

  // Apply deleted entries.
  for (size_t i = 0; i < deleted_entries.size(); ++i) {
    // TODO(hashimoto): Handle ApplyEntry errors correctly.
    FileError error = ApplyEntry(deleted_entries[i]);
    DLOG_IF(WARNING, error != FILE_ERROR_OK)
        << "ApplyEntry failed: " << FileErrorToString(error)
        << ", title = " << deleted_entries[i].title();
  }

  return FILE_ERROR_OK;
}

FileError ChangeListProcessor::ApplyEntry(const ResourceEntry& entry) {
  // Lookup the entry.
  std::string local_id;
  FileError error = resource_metadata_->GetIdByResourceId(entry.resource_id(),
                                                          &local_id);
  ResourceEntry existing_entry;
  if (error == FILE_ERROR_OK)
    error = resource_metadata_->GetResourceEntryById(local_id, &existing_entry);

  const FileError get_existing_entry_result = error;
  if (entry.deleted()) {
    // Deleted file/directory.
    switch (get_existing_entry_result) {
      case FILE_ERROR_OK:
        error = resource_metadata_->RemoveEntry(local_id);
        break;
      case FILE_ERROR_NOT_FOUND:  // Already deleted.
        error = FILE_ERROR_OK;
        break;
      default:
        error = get_existing_entry_result;
    }
  } else {
    const std::string& parent_resource_id =
        parent_resource_id_map_[entry.resource_id()];
    DCHECK(!parent_resource_id.empty()) << entry.resource_id();

    std::string parent_local_id;
    error = resource_metadata_->GetIdByResourceId(
        parent_resource_id, &parent_local_id);
    if (error == FILE_ERROR_OK) {
      ResourceEntry new_entry(entry);
      new_entry.set_parent_local_id(parent_local_id);

      switch (get_existing_entry_result) {
        case FILE_ERROR_OK:  // Entry exists and needs to be refreshed.
          new_entry.set_local_id(local_id);
          error = resource_metadata_->RefreshEntry(new_entry);
          break;
        case FILE_ERROR_NOT_FOUND: {  // Adding a new entry.
          std::string local_id;
          error = resource_metadata_->AddEntry(new_entry, &local_id);
          break;
        }
        default:
          error = get_existing_entry_result;
      }

      if (error == FILE_ERROR_OK)
        UpdateChangedDirs(entry);
    }
  }

  return error;
}

// static
FileError ChangeListProcessor::RefreshDirectory(
    ResourceMetadata* resource_metadata,
    const DirectoryFetchInfo& directory_fetch_info,
    ScopedVector<ChangeList> change_lists,
    base::FilePath* out_file_path) {
  DCHECK(!directory_fetch_info.empty());

  std::string directory_local_id;
  FileError error = resource_metadata->GetIdByResourceId(
      directory_fetch_info.resource_id(), &directory_local_id);
  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntry directory;
  error = resource_metadata->GetResourceEntryById(directory_local_id,
                                                  &directory);
  if (error != FILE_ERROR_OK)
    return error;

  if (!directory.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  for (size_t i = 0; i < change_lists.size(); ++i) {
    ChangeList* change_list = change_lists[i];
    std::vector<ResourceEntry>* entries = change_list->mutable_entries();
    for (size_t i = 0; i < entries->size(); ++i) {
      ResourceEntry* entry = &(*entries)[i];
      const std::string& parent_resource_id =
          change_list->parent_resource_ids()[i];

      // Skip if the parent resource ID does not match. This is needed to
      // handle entries with multiple parents. For such entries, the first
      // parent is picked and other parents are ignored, hence some entries may
      // have a parent resource ID which does not match the target directory's.
      if (parent_resource_id != directory_fetch_info.resource_id()) {
        DVLOG(1) << "Wrong-parent entry rejected: " << entry->resource_id();
        continue;
      }

      entry->set_parent_local_id(directory_local_id);

      std::string local_id;
      error = resource_metadata->GetIdByResourceId(entry->resource_id(),
                                                   &local_id);
      if (error == FILE_ERROR_OK) {
        entry->set_local_id(local_id);
        error = resource_metadata->RefreshEntry(*entry);
      }

      if (error == FILE_ERROR_NOT_FOUND) {  // If refreshing fails, try adding.
        std::string local_id;
        entry->clear_local_id();
        error = resource_metadata->AddEntry(*entry, &local_id);
      }

      if (error != FILE_ERROR_OK)
        return error;
    }
  }

  directory.mutable_directory_specific_info()->set_changestamp(
      directory_fetch_info.changestamp());
  error = resource_metadata->RefreshEntry(directory);
  if (error != FILE_ERROR_OK)
    return error;

  *out_file_path = resource_metadata->GetFilePath(directory_local_id);
  return FILE_ERROR_OK;
}

void ChangeListProcessor::UpdateChangedDirs(const ResourceEntry& entry) {
  DCHECK(!entry.resource_id().empty());

  std::string local_id;
  base::FilePath file_path;
  if (resource_metadata_->GetIdByResourceId(
          entry.resource_id(), &local_id) == FILE_ERROR_OK)
    file_path = resource_metadata_->GetFilePath(local_id);

  if (!file_path.empty()) {
    // Notify parent.
    changed_dirs_.insert(file_path.DirName());

    if (entry.file_info().is_directory()) {
      // Notify self if entry is a directory.
      changed_dirs_.insert(file_path);

      // Notify all descendants if it is a directory deletion.
      if (entry.deleted()) {
        std::set<base::FilePath> sub_directories;
        resource_metadata_->GetSubDirectoriesRecursively(local_id,
                                                         &sub_directories);
        changed_dirs_.insert(sub_directories.begin(), sub_directories.end());
      }
    }
  }
}

}  // namespace internal
}  // namespace drive
