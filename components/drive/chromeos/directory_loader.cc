// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/directory_loader.h"

#include <stddef.h>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/drive/chromeos/change_list_loader.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "google_apis/drive/drive_api_parser.h"
#include "url/gurl.h"

namespace drive {
namespace internal {

namespace {

// Minimum changestamp gap required to start loading directory.
const int kMinimumChangestampGap = 50;

FileError CheckLocalState(ResourceMetadata* resource_metadata,
                          const google_apis::AboutResource& about_resource,
                          const std::string& local_id,
                          ResourceEntry* entry,
                          int64_t* local_changestamp) {
  // Fill My Drive resource ID.
  ResourceEntry mydrive;
  FileError error = resource_metadata->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &mydrive);
  if (error != FILE_ERROR_OK)
    return error;

  if (mydrive.resource_id().empty()) {
    mydrive.set_resource_id(about_resource.root_folder_id());
    error = resource_metadata->RefreshEntry(mydrive);
    if (error != FILE_ERROR_OK)
      return error;
  }

  // Get entry.
  error = resource_metadata->GetResourceEntryById(local_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  // Get the local changestamp.
  return resource_metadata->GetLargestChangestamp(local_changestamp);
}

FileError UpdateChangestamp(ResourceMetadata* resource_metadata,
                            const DirectoryFetchInfo& directory_fetch_info,
                            base::FilePath* directory_path) {
  // Update the directory changestamp.
  ResourceEntry directory;
  FileError error = resource_metadata->GetResourceEntryById(
      directory_fetch_info.local_id(), &directory);
  if (error != FILE_ERROR_OK)
    return error;

  if (!directory.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  directory.mutable_directory_specific_info()->set_changestamp(
      directory_fetch_info.changestamp());
  error = resource_metadata->RefreshEntry(directory);
  if (error != FILE_ERROR_OK)
    return error;

  // Get the directory path.
  return resource_metadata->GetFilePath(directory_fetch_info.local_id(),
                                        directory_path);
}

}  // namespace

struct DirectoryLoader::ReadDirectoryCallbackState {
  ReadDirectoryEntriesCallback entries_callback;
  FileOperationCallback completion_callback;
  std::set<std::string> sent_entry_names;
};

// Fetches the resource entries in the directory with |directory_resource_id|.
class DirectoryLoader::FeedFetcher {
 public:
  FeedFetcher(DirectoryLoader* loader,
              const DirectoryFetchInfo& directory_fetch_info,
              const std::string& root_folder_id)
      : loader_(loader),
        directory_fetch_info_(directory_fetch_info),
        root_folder_id_(root_folder_id),
        weak_ptr_factory_(this) {
  }

  ~FeedFetcher() {
  }

  void Run(const FileOperationCallback& callback) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());
    DCHECK(!directory_fetch_info_.resource_id().empty());

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    loader_->scheduler_->GetFileListInDirectory(
        directory_fetch_info_.resource_id(),
        base::Bind(&FeedFetcher::OnFileListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnFileListFetched(const FileOperationCallback& callback,
                         google_apis::DriveApiErrorCode status,
                         std::unique_ptr<google_apis::FileList> file_list) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error);
      return;
    }

    DCHECK(file_list);
    std::unique_ptr<ChangeList> change_list(new ChangeList(*file_list));
    GURL next_url = file_list->next_link();

    ResourceEntryVector* entries = new ResourceEntryVector;
    loader_->loader_controller_->ScheduleRun(base::Bind(
        base::IgnoreResult(
            &base::PostTaskAndReplyWithResult<FileError, FileError>),
        base::RetainedRef(loader_->blocking_task_runner_), FROM_HERE,
        base::Bind(&ChangeListProcessor::RefreshDirectory,
                   loader_->resource_metadata_, directory_fetch_info_,
                   base::Passed(&change_list), entries),
        base::Bind(&FeedFetcher::OnDirectoryRefreshed,
                   weak_ptr_factory_.GetWeakPtr(), callback, next_url,
                   base::Owned(entries))));
  }

  void OnDirectoryRefreshed(
      const FileOperationCallback& callback,
      const GURL& next_url,
      const std::vector<ResourceEntry>* refreshed_entries,
      FileError error) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    if (error != FILE_ERROR_OK) {
      callback.Run(error);
      return;
    }

    loader_->SendEntries(directory_fetch_info_.local_id(), *refreshed_entries);

    if (!next_url.is_empty()) {
      // There is the remaining result so fetch it.
      loader_->scheduler_->GetRemainingFileList(
          next_url,
          base::Bind(&FeedFetcher::OnFileListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    UMA_HISTOGRAM_TIMES("Drive.DirectoryFeedLoadTime",
                        base::TimeTicks::Now() - start_time_);

    // Note: The fetcher is managed by DirectoryLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK);
  }

  DirectoryLoader* loader_;
  DirectoryFetchInfo directory_fetch_info_;
  std::string root_folder_id_;
  base::TimeTicks start_time_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<FeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FeedFetcher);
};

DirectoryLoader::DirectoryLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    AboutResourceLoader* about_resource_loader,
    LoaderController* loader_controller)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      about_resource_loader_(about_resource_loader),
      loader_controller_(loader_controller),
      weak_ptr_factory_(this) {
}

DirectoryLoader::~DirectoryLoader() {
  STLDeleteElements(&fast_fetch_feed_fetcher_set_);
}

void DirectoryLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void DirectoryLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void DirectoryLoader::ReadDirectory(
    const base::FilePath& directory_path,
    const ReadDirectoryEntriesCallback& entries_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!completion_callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_),
                 directory_path,
                 entry),
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 entries_callback,
                 completion_callback,
                 true,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::ReadDirectoryAfterGetEntry(
    const base::FilePath& directory_path,
    const ReadDirectoryEntriesCallback& entries_callback,
    const FileOperationCallback& completion_callback,
    bool should_try_loading_parent,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!completion_callback.is_null());

  if (error == FILE_ERROR_NOT_FOUND &&
      should_try_loading_parent &&
      util::GetDriveGrandRootPath().IsParent(directory_path)) {
    // This entry may be found after loading the parent.
    ReadDirectory(directory_path.DirName(),
                  ReadDirectoryEntriesCallback(),
                  base::Bind(&DirectoryLoader::ReadDirectoryAfterLoadParent,
                             weak_ptr_factory_.GetWeakPtr(),
                             directory_path,
                             entries_callback,
                             completion_callback));
    return;
  }
  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }

  if (!entry->file_info().is_directory()) {
    completion_callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  DirectoryFetchInfo directory_fetch_info(
      entry->local_id(),
      entry->resource_id(),
      entry->directory_specific_info().changestamp());

  // Register the callback function to be called when it is loaded.
  const std::string& local_id = directory_fetch_info.local_id();
  ReadDirectoryCallbackState callback_state;
  callback_state.entries_callback = entries_callback;
  callback_state.completion_callback = completion_callback;
  pending_load_callback_[local_id].push_back(callback_state);

  // If loading task for |local_id| is already running, do nothing.
  if (pending_load_callback_[local_id].size() > 1)
    return;

  about_resource_loader_->GetAboutResource(
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(), local_id));
}

void DirectoryLoader::ReadDirectoryAfterLoadParent(
    const base::FilePath& directory_path,
    const ReadDirectoryEntriesCallback& entries_callback,
    const FileOperationCallback& completion_callback,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!completion_callback.is_null());

  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_),
                 directory_path,
                 entry),
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 entries_callback,
                 completion_callback,
                 false,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::ReadDirectoryAfterGetAboutResource(
    const std::string& local_id,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(thread_checker_.CalledOnValidThread());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(local_id, error);
    return;
  }

  DCHECK(about_resource);

  // Check the current status of local metadata, and start loading if needed.
  google_apis::AboutResource* about_resource_ptr = about_resource.get();
  ResourceEntry* entry = new ResourceEntry;
  int64_t* local_changestamp = new int64_t;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckLocalState,
                 resource_metadata_,
                 *about_resource_ptr,
                 local_id,
                 entry,
                 local_changestamp),
      base::Bind(&DirectoryLoader::ReadDirectoryAfterCheckLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&about_resource),
                 local_id,
                 base::Owned(entry),
                 base::Owned(local_changestamp)));
}

void DirectoryLoader::ReadDirectoryAfterCheckLocalState(
    std::unique_ptr<google_apis::AboutResource> about_resource,
    const std::string& local_id,
    const ResourceEntry* entry,
    const int64_t* local_changestamp,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(about_resource);

  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(local_id, error);
    return;
  }
  // This entry does not exist on the server.
  if (entry->resource_id().empty()) {
    OnDirectoryLoadComplete(local_id, FILE_ERROR_OK);
    return;
  }

  int64_t remote_changestamp = about_resource->largest_change_id();

  // Start loading the directory.
  int64_t directory_changestamp = std::max(
      entry->directory_specific_info().changestamp(), *local_changestamp);

  DirectoryFetchInfo directory_fetch_info(
      local_id, entry->resource_id(), remote_changestamp);

  // If the directory's changestamp is up-to-date or the global changestamp of
  // the metadata DB is new enough (which means the normal changelist loading
  // should finish very soon), just schedule to run the callback, as there is no
  // need to fetch the directory.
  if (directory_changestamp >= remote_changestamp ||
      *local_changestamp + kMinimumChangestampGap > remote_changestamp) {
    OnDirectoryLoadComplete(local_id, FILE_ERROR_OK);
  } else {
    // Start fetching the directory content, and mark it with the changestamp
    // |remote_changestamp|.
    LoadDirectoryFromServer(directory_fetch_info);
  }
}

void DirectoryLoader::OnDirectoryLoadComplete(const std::string& local_id,
                                              FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  if (it == pending_load_callback_.end())
    return;

  // No need to read metadata when no one needs entries.
  bool needs_to_send_entries = false;
  for (size_t i = 0; i < it->second.size(); ++i) {
    const ReadDirectoryCallbackState& callback_state = it->second[i];
    if (!callback_state.entries_callback.is_null())
      needs_to_send_entries = true;
  }

  if (!needs_to_send_entries) {
    OnDirectoryLoadCompleteAfterRead(local_id, NULL, FILE_ERROR_OK);
    return;
  }

  ResourceEntryVector* entries = new ResourceEntryVector;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::ReadDirectoryById,
                 base::Unretained(resource_metadata_), local_id, entries),
      base::Bind(&DirectoryLoader::OnDirectoryLoadCompleteAfterRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id,
                 base::Owned(entries)));
}

void DirectoryLoader::OnDirectoryLoadCompleteAfterRead(
    const std::string& local_id,
    const ResourceEntryVector* entries,
    FileError error) {
  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  if (it != pending_load_callback_.end()) {
    DVLOG(1) << "Running callback for " << local_id;

    if (error == FILE_ERROR_OK && entries)
      SendEntries(local_id, *entries);

    for (size_t i = 0; i < it->second.size(); ++i) {
      const ReadDirectoryCallbackState& callback_state = it->second[i];
      callback_state.completion_callback.Run(error);
    }
    pending_load_callback_.erase(it);
  }
}

void DirectoryLoader::SendEntries(const std::string& local_id,
                                  const ResourceEntryVector& entries) {
  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  DCHECK(it != pending_load_callback_.end());

  for (size_t i = 0; i < it->second.size(); ++i) {
    ReadDirectoryCallbackState* callback_state = &it->second[i];
    if (callback_state->entries_callback.is_null())
      continue;

    // Filter out entries which were already sent.
    std::unique_ptr<ResourceEntryVector> entries_to_send(
        new ResourceEntryVector);
    for (size_t i = 0; i < entries.size(); ++i) {
      const ResourceEntry& entry = entries[i];
      if (!callback_state->sent_entry_names.count(entry.base_name())) {
        callback_state->sent_entry_names.insert(entry.base_name());
        entries_to_send->push_back(entry);
      }
    }
    callback_state->entries_callback.Run(std::move(entries_to_send));
  }
}

void DirectoryLoader::LoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!directory_fetch_info.empty());
  DVLOG(1) << "Start loading directory: " << directory_fetch_info.ToString();

  const google_apis::AboutResource* about_resource =
      about_resource_loader_->cached_about_resource();
  DCHECK(about_resource);

  logger_->Log(logging::LOG_INFO,
               "Fast-fetch start: %s; Server changestamp: %s",
               directory_fetch_info.ToString().c_str(),
               base::Int64ToString(
                   about_resource->largest_change_id()).c_str());

  FeedFetcher* fetcher = new FeedFetcher(this,
                                         directory_fetch_info,
                                         about_resource->root_folder_id());
  fast_fetch_feed_fetcher_set_.insert(fetcher);
  fetcher->Run(
      base::Bind(&DirectoryLoader::LoadDirectoryFromServerAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 fetcher));
}

void DirectoryLoader::LoadDirectoryFromServerAfterLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    FeedFetcher* fetcher,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!directory_fetch_info.empty());

  // Delete the fetcher.
  fast_fetch_feed_fetcher_set_.erase(fetcher);
  delete fetcher;

  logger_->Log(logging::LOG_INFO,
               "Fast-fetch complete: %s => %s",
               directory_fetch_info.ToString().c_str(),
               FileErrorToString(error).c_str());

  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.local_id()
               << ": " << FileErrorToString(error);
    OnDirectoryLoadComplete(directory_fetch_info.local_id(), error);
    return;
  }

  // Update changestamp and get the directory path.
  base::FilePath* directory_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateChangestamp,
                 resource_metadata_,
                 directory_fetch_info,
                 directory_path),
      base::Bind(
          &DirectoryLoader::LoadDirectoryFromServerAfterUpdateChangestamp,
          weak_ptr_factory_.GetWeakPtr(),
          directory_fetch_info,
          base::Owned(directory_path)));
}

void DirectoryLoader::LoadDirectoryFromServerAfterUpdateChangestamp(
    const DirectoryFetchInfo& directory_fetch_info,
    const base::FilePath* directory_path,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
  OnDirectoryLoadComplete(directory_fetch_info.local_id(), error);

  // Also notify the observers.
  if (error == FILE_ERROR_OK && !directory_path->empty()) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                      observers_,
                      OnDirectoryReloaded(*directory_path));
  }
}

}  // namespace internal
}  // namespace drive
