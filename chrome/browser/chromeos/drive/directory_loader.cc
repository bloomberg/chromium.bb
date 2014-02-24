// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/directory_loader.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/event_logger.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace drive {
namespace internal {

namespace {

// Minimum changestamp gap required to start loading directory.
const int kMinimumChangestampGap = 50;

FileError RefreshMyDriveIfNeeded(
    ResourceMetadata* resource_metadata,
    const google_apis::AboutResource& about_resource) {
  ResourceEntry entry;
  FileError error = resource_metadata->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &entry);
  if (error != FILE_ERROR_OK || !entry.resource_id().empty())
    return error;

  entry.set_resource_id(about_resource.root_folder_id());
  return resource_metadata->RefreshEntry(entry);
}

}  // namespace

// Fetches the resource entries in the directory with |directory_resource_id|.
class DirectoryLoader::FeedFetcher {
 public:
  typedef base::Callback<void(FileError, ScopedVector<ChangeList>)>
      FeedFetcherCallback;

  FeedFetcher(JobScheduler* scheduler,
              DriveServiceInterface* drive_service,
              const std::string& directory_resource_id,
              const std::string& root_folder_id)
      : scheduler_(scheduler),
        drive_service_(drive_service),
        directory_resource_id_(directory_resource_id),
        root_folder_id_(root_folder_id),
        weak_ptr_factory_(this) {
  }

  ~FeedFetcher() {
  }

  void Run(const FeedFetcherCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback.is_null());
    DCHECK(!directory_resource_id_.empty());

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    // We use WAPI's GetResourceListInDirectory even if Drive API v2 is
    // enabled. This is the short term work around of the performance
    // regression.
    // TODO(hashimoto): Remove this. crbug.com/340931.

    std::string resource_id = directory_resource_id_;
    if (directory_resource_id_ == root_folder_id_) {
      // GData WAPI doesn't accept the root directory id which is used in Drive
      // API v2. So it is necessary to translate it here.
      resource_id = util::kWapiRootDirectoryResourceId;
    }

    scheduler_->GetResourceListInDirectoryByWapi(
        resource_id,
        base::Bind(&FeedFetcher::OnResourceListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnResourceListFetched(
      const FeedFetcherCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceList> resource_list) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback.is_null());

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error, ScopedVector<ChangeList>());
      return;
    }

    // Add the current change list to the list of collected lists.
    DCHECK(resource_list);
    ChangeList* change_list = new ChangeList(*resource_list);
    FixResourceIdInChangeList(change_list);
    change_lists_.push_back(change_list);

    GURL next_url;
    if (resource_list->GetNextFeedURL(&next_url) && !next_url.is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingResourceList(
          next_url,
          base::Bind(&FeedFetcher::OnResourceListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    UMA_HISTOGRAM_TIMES("Drive.DirectoryFeedLoadTime",
                        base::TimeTicks::Now() - start_time_);

    // Note: The fetcher is managed by DirectoryLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, change_lists_.Pass());
  }

  // Fixes resource IDs in |change_list| into the format that |drive_service_|
  // can understand. Note that |change_list| contains IDs in GData WAPI format
  // since currently we always use WAPI for fast fetch, regardless of the flag.
  void FixResourceIdInChangeList(ChangeList* change_list) {
    std::vector<ResourceEntry>* entries = change_list->mutable_entries();
    std::vector<std::string>* parent_resource_ids =
        change_list->mutable_parent_resource_ids();
    for (size_t i = 0; i < entries->size(); ++i) {
      ResourceEntry* entry = &(*entries)[i];
      if (entry->has_resource_id())
        entry->set_resource_id(FixResourceId(entry->resource_id()));

      (*parent_resource_ids)[i] = FixResourceId((*parent_resource_ids)[i]);
    }
  }

  std::string FixResourceId(const std::string& resource_id) {
    if (resource_id == util::kWapiRootDirectoryResourceId)
      return root_folder_id_;
    return drive_service_->GetResourceIdCanonicalizer().Run(resource_id);
  }

  JobScheduler* scheduler_;
  DriveServiceInterface* drive_service_;
  std::string directory_resource_id_;
  std::string root_folder_id_;
  ScopedVector<ChangeList> change_lists_;
  base::TimeTicks start_time_;
  base::WeakPtrFactory<FeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FeedFetcher);
};

DirectoryLoader::DirectoryLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    DriveServiceInterface* drive_service,
    AboutResourceLoader* about_resource_loader,
    LoaderController* loader_controller)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      drive_service_(drive_service),
      about_resource_loader_(about_resource_loader),
      loader_controller_(loader_controller),
      weak_ptr_factory_(this) {
}

DirectoryLoader::~DirectoryLoader() {
  STLDeleteElements(&fast_fetch_feed_fetcher_set_);
}

void DirectoryLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DirectoryLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DirectoryLoader::LoadDirectoryIfNeeded(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_),
                 directory_path,
                 entry),
      base::Bind(&DirectoryLoader::LoadDirectoryIfNeededAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback,
                 true,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::LoadDirectoryIfNeededAfterGetEntry(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    bool should_try_loading_parent,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Should load grand root first if My Drive's resource ID is not set.
  const bool mydrive_resource_id_is_empty =
      directory_path == util::GetDriveMyDriveRootPath() &&
      error == FILE_ERROR_OK &&
      entry->resource_id().empty();
  if ((error == FILE_ERROR_NOT_FOUND || mydrive_resource_id_is_empty) &&
      should_try_loading_parent &&
      util::GetDriveGrandRootPath().IsParent(directory_path)) {
    // This entry may be found after loading the parent.
    LoadDirectoryIfNeeded(
        directory_path.DirName(),
        base::Bind(&DirectoryLoader::LoadDirectoryIfNeededAfterLoadParent,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_path,
                   callback));
    return;
  }
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  if (!entry->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // This entry does not exist on the server. Grand root is excluded as it's
  // specially handled in LoadDirectoryFromServer().
  if (entry->resource_id().empty() &&
      entry->local_id() != util::kDriveGrandRootLocalId) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  Load(DirectoryFetchInfo(entry->local_id(),
                          entry->resource_id(),
                          entry->directory_specific_info().changestamp()),
       callback);
}

void DirectoryLoader::LoadDirectoryIfNeededAfterLoadParent(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
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
      base::Bind(&DirectoryLoader::LoadDirectoryIfNeededAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback,
                 false,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::Load(const DirectoryFetchInfo& directory_fetch_info,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Register the callback function to be called when it is loaded.
  const std::string& local_id = directory_fetch_info.local_id();
  pending_load_callback_[local_id].push_back(callback);

  // If loading task for |local_id| is already running, do nothing.
  if (pending_load_callback_[local_id].size() > 1)
    return;

  // Check the current status of local metadata, and start loading if needed.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetLargestChangestamp,
                 base::Unretained(resource_metadata_)),
      base::Bind(&DirectoryLoader::LoadAfterGetLargestChangestamp,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info));
}

void DirectoryLoader::LoadAfterGetLargestChangestamp(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Note: To be precise, we need to call UpdateAboutResource() here. However,
  // - It is costly to do GetAboutResource HTTP request every time.
  // - The chance using an old value is small; it only happens when
  //   LoadIfNeeded is called during one GetAboutResource roundtrip time
  //   of a change list fetching.
  // - Even if the value is old, it just marks the directory as older. It may
  //   trigger one future unnecessary re-fetch, but it'll never lose data.
  about_resource_loader_->GetAboutResource(
      base::Bind(&DirectoryLoader::LoadAfterGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 local_changestamp));
}

void DirectoryLoader::LoadAfterGetAboutResource(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(directory_fetch_info, error);
    return;
  }

  DCHECK(about_resource);

  int64 remote_changestamp = about_resource->largest_change_id();

  // If the caller is interested in a particular directory, start loading the
  // directory.
  int64 directory_changestamp = std::max(directory_fetch_info.changestamp(),
                                         local_changestamp);

  // We may not fetch from the server at all if the local metadata is new
  // enough, but we log this message here, so "Fast-fetch start" and
  // "Fast-fetch complete" always match.
  // TODO(satorux): Distinguish the "not fetching at all" case.
  logger_->Log(logging::LOG_INFO,
               "Fast-fetch start: %s; Server changestamp: %s",
               directory_fetch_info.ToString().c_str(),
               base::Int64ToString(remote_changestamp).c_str());

  // If the directory's changestamp is new enough, just schedule to run the
  // callback, as there is no need to fetch the directory.
  if (directory_changestamp + kMinimumChangestampGap > remote_changestamp) {
    OnDirectoryLoadComplete(directory_fetch_info, FILE_ERROR_OK);
  } else {
    // Start fetching the directory content, and mark it with the changestamp
    // |remote_changestamp|.
    DirectoryFetchInfo new_directory_fetch_info(
        directory_fetch_info.local_id(), directory_fetch_info.resource_id(),
        remote_changestamp);
    LoadDirectoryFromServer(new_directory_fetch_info);
  }
}

void DirectoryLoader::OnDirectoryLoadComplete(
    const DirectoryFetchInfo& directory_fetch_info,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  logger_->Log(logging::LOG_INFO,
               "Fast-fetch complete: %s => %s",
               directory_fetch_info.ToString().c_str(),
               FileErrorToString(error).c_str());
  const std::string& local_id = directory_fetch_info.local_id();
  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  if (it != pending_load_callback_.end()) {
    DVLOG(1) << "Running callback for " << local_id;
    const std::vector<FileOperationCallback>& callbacks = it->second;
    for (size_t i = 0; i < callbacks.size(); ++i) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(callbacks[i], error));
    }
    pending_load_callback_.erase(it);
  }
}

void DirectoryLoader::LoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_fetch_info.empty());
  DCHECK(about_resource_loader_->cached_about_resource());
  DVLOG(1) << "Start loading directory: " << directory_fetch_info.ToString();

  if (directory_fetch_info.local_id() == util::kDriveGrandRootLocalId) {
    // Load for a grand root directory means slightly different from other
    // directories. It means filling resource ID of mydrive root.
    base::FilePath* changed_directory_path = new base::FilePath;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&RefreshMyDriveIfNeeded,
                   resource_metadata_,
                   google_apis::AboutResource(
                       *about_resource_loader_->cached_about_resource())),
        base::Bind(&DirectoryLoader::LoadDirectoryFromServerAfterRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   base::Owned(changed_directory_path)));
    return;
  }

  FeedFetcher* fetcher = new FeedFetcher(
      scheduler_,
      drive_service_,
      directory_fetch_info.resource_id(),
      about_resource_loader_->cached_about_resource()->root_folder_id());
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
    FileError error,
    ScopedVector<ChangeList> change_lists) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_fetch_info.empty());

  // Delete the fetcher.
  fast_fetch_feed_fetcher_set_.erase(fetcher);
  delete fetcher;

  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.local_id()
               << ": " << FileErrorToString(error);
    OnDirectoryLoadComplete(directory_fetch_info, error);
    return;
  }

  base::FilePath* directory_path = new base::FilePath;
  loader_controller_->ScheduleRun(base::Bind(
      base::IgnoreResult(
          &base::PostTaskAndReplyWithResult<FileError, FileError>),
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ChangeListProcessor::RefreshDirectory,
                 resource_metadata_,
                 directory_fetch_info,
                 base::Passed(&change_lists),
                 directory_path),
      base::Bind(&DirectoryLoader::LoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 base::Owned(directory_path))));
}

void DirectoryLoader::LoadDirectoryFromServerAfterRefresh(
    const DirectoryFetchInfo& directory_fetch_info,
    const base::FilePath* directory_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
  OnDirectoryLoadComplete(directory_fetch_info, error);

  // Also notify the observers.
  if (error == FILE_ERROR_OK && !directory_path->empty()) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                      OnDirectoryChanged(*directory_path));
  }
}

}  // namespace internal
}  // namespace drive
