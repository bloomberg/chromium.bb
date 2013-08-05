// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_processor.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

namespace drive {
namespace internal {

ChangeList::ChangeList(const google_apis::ResourceList& resource_list)
    : largest_changestamp_(resource_list.largest_changestamp()) {
  resource_list.GetNextFeedURL(&next_url_);

  entries_.resize(resource_list.entries().size());
  size_t entries_index = 0;
  for (size_t i = 0; i < resource_list.entries().size(); ++i) {
    if (ConvertToResourceEntry(*resource_list.entries()[i],
                               &entries_[entries_index]))
      ++entries_index;
  }
  entries_.resize(entries_index);
}

ChangeList::~ChangeList() {}

class ChangeListProcessor::ChangeListToEntryMapUMAStats {
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

ChangeListProcessor::ChangeListProcessor(ResourceMetadata* resource_metadata)
  : resource_metadata_(resource_metadata) {
}

ChangeListProcessor::~ChangeListProcessor() {
}

void ChangeListProcessor::Apply(
    scoped_ptr<google_apis::AboutResource> about_resource,
    ScopedVector<ChangeList> change_lists,
    bool is_delta_update) {
  DCHECK(is_delta_update || about_resource.get());

  int64 largest_changestamp = 0;
  if (is_delta_update) {
    if (!change_lists.empty()) {
      // The changestamp appears in the first page of the change list.
      // The changestamp does not appear in the full resource list.
      largest_changestamp = change_lists[0]->largest_changestamp();
      DCHECK_GE(change_lists[0]->largest_changestamp(), 0);
    }
  } else if (about_resource.get()) {
    largest_changestamp = about_resource->largest_change_id();

    DVLOG(1) << "Root folder ID is " << about_resource->root_folder_id();
    DCHECK(!about_resource->root_folder_id().empty());
  } else {
    // A full update without AboutResouce will have no effective changestamp.
    NOTREACHED();
  }

  ChangeListToEntryMapUMAStats uma_stats;
  ConvertToMap(change_lists.Pass(), &entry_map_, &uma_stats);

  // Add the largest changestamp for directories.
  for (ResourceEntryMap::iterator it = entry_map_.begin();
       it != entry_map_.end(); ++it) {
    if (it->second.file_info().is_directory()) {
      it->second.mutable_directory_specific_info()->set_changestamp(
          largest_changestamp);
    }
  }

  ApplyEntryMap(is_delta_update, about_resource.Pass());

  // Update the root entry and finish.
  UpdateRootEntry(largest_changestamp);

  // Update changestamp.
  FileError error = resource_metadata_->SetLargestChangestamp(
      largest_changestamp);
  DLOG_IF(ERROR, error != FILE_ERROR_OK) << "SetLargestChangeStamp failed: "
                                         << FileErrorToString(error);

  // Shouldn't record histograms when processing delta update.
  if (!is_delta_update)
    uma_stats.UpdateFileCountUmaHistograms();
}

void ChangeListProcessor::ApplyEntryMap(
    bool is_delta_update,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  if (!is_delta_update) {  // Full update.
    DCHECK(about_resource);

    FileError error = resource_metadata_->Reset();

    LOG_IF(ERROR, error != FILE_ERROR_OK) << "Failed to reset: "
                                          << FileErrorToString(error);

    changed_dirs_.insert(util::GetDriveGrandRootPath());
    changed_dirs_.insert(util::GetDriveMyDriveRootPath());

    // Create the MyDrive root directory.
    ApplyEntry(util::CreateMyDriveRootEntry(about_resource->root_folder_id()));
  }

  // Apply all entries to the metadata.
  while (!entry_map_.empty()) {
    // Start from entry_map_.begin() and traverse ancestors.
    std::vector<ResourceEntryMap::iterator> entries;
    for (ResourceEntryMap::iterator it = entry_map_.begin();
         it != entry_map_.end();
         it = entry_map_.find(it->second.parent_resource_id())) {
      DCHECK_EQ(it->first, it->second.resource_id());
      entries.push_back(it);
    }

    // Apply the parent first.
    std::reverse(entries.begin(), entries.end());
    for (size_t i = 0; i < entries.size(); ++i) {
      ResourceEntryMap::iterator it = entries[i];
      ApplyEntry(it->second);
      entry_map_.erase(it);
    }
  }
}

void ChangeListProcessor::ApplyEntry(const ResourceEntry& entry) {
  // Lookup the entry.
  ResourceEntry existing_entry;
  FileError error = resource_metadata_->GetResourceEntryById(
      entry.resource_id(), &existing_entry);

  if (error == FILE_ERROR_OK) {
    if (entry.deleted()) {
      // Deleted file/directory.
      RemoveEntry(entry);
    } else {
      // Entry exists and needs to be refreshed.
      RefreshEntry(entry);
    }
  } else if (error == FILE_ERROR_NOT_FOUND && !entry.deleted()) {
    // Adding a new entry.
    AddEntry(entry);
  }
}

void ChangeListProcessor::AddEntry(const ResourceEntry& entry) {
  FileError error = resource_metadata_->AddEntry(entry);

  if (error == FILE_ERROR_OK) {
    base::FilePath file_path =
        resource_metadata_->GetFilePath(entry.resource_id());
    // Notify if a directory has been created.
    if (entry.file_info().is_directory())
      changed_dirs_.insert(file_path);

    // Notify parent.
    changed_dirs_.insert(file_path.DirName());
  }
}

void ChangeListProcessor::RemoveEntry(const ResourceEntry& entry) {
  std::set<base::FilePath> child_directories;
  if (entry.file_info().is_directory()) {
    resource_metadata_->GetChildDirectories(entry.resource_id(),
                                            &child_directories);
  }

  base::FilePath file_path =
      resource_metadata_->GetFilePath(entry.resource_id());

  FileError error = resource_metadata_->RemoveEntry(entry.resource_id());

  if (error == FILE_ERROR_OK) {
    // Notify parent.
    changed_dirs_.insert(file_path.DirName());

    // Notify children, if any.
    changed_dirs_.insert(child_directories.begin(), child_directories.end());

    // If entry is a directory, notify self.
    if (entry.file_info().is_directory())
      changed_dirs_.insert(file_path);
  }
}

void ChangeListProcessor::RefreshEntry(const ResourceEntry& entry) {
  base::FilePath old_file_path =
      resource_metadata_->GetFilePath(entry.resource_id());

  FileError error = resource_metadata_->RefreshEntry(entry);

  if (error == FILE_ERROR_OK) {
    base::FilePath new_file_path =
        resource_metadata_->GetFilePath(entry.resource_id());

    // Notify old parent.
    changed_dirs_.insert(old_file_path.DirName());

    // Notify new parent.
    changed_dirs_.insert(new_file_path.DirName());

    // Notify self if entry is a directory.
    if (entry.file_info().is_directory()) {
      // Notify new self.
      changed_dirs_.insert(new_file_path);
      // Notify old self.
      changed_dirs_.insert(old_file_path);
    }
  }
}

// static
void ChangeListProcessor::ConvertToMap(
    ScopedVector<ChangeList> change_lists,
    ResourceEntryMap* entry_map,
    ChangeListToEntryMapUMAStats* uma_stats) {
  for (size_t i = 0; i < change_lists.size(); ++i) {
    ChangeList* change_list = change_lists[i];

    std::vector<ResourceEntry>* entries = change_list->mutable_entries();
    for (size_t i = 0; i < entries->size(); ++i) {
      ResourceEntry* entry = &(*entries)[i];
      // Some document entries don't map into files (i.e. sites).
      if (entry->resource_id().empty())
        continue;

      // Count the number of files.
      if (uma_stats) {
        if (!entry->file_info().is_directory()) {
          uma_stats->IncrementNumFiles(
              entry->file_specific_info().is_hosted_document());
        }
        if (entry->shared_with_me())
          uma_stats->IncrementNumSharedWithMeEntries();
      }

      (*entry_map)[entry->resource_id()].Swap(entry);
      LOG_IF(WARNING, !entry->resource_id().empty())
          << "Found duplicated file: " << entry->base_name();
    }
  }
}

void ChangeListProcessor::UpdateRootEntry(int64 largest_changestamp) {
  ResourceEntry root;
  FileError error = resource_metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root);

  if (error != FILE_ERROR_OK) {
    // TODO(satorux): Need to trigger recovery if root is corrupt.
    LOG(WARNING) << "Failed to get the entry for root directory";
    return;
  }

  // The changestamp should always be updated.
  root.mutable_directory_specific_info()->set_changestamp(largest_changestamp);

  error = resource_metadata_->RefreshEntry(root);

  LOG_IF(WARNING, error != FILE_ERROR_OK) << "Failed to refresh root directory";
}

}  // namespace internal
}  // namespace drive
