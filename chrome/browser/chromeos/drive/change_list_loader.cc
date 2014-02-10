// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include <set>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
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

typedef base::Callback<void(FileError, ScopedVector<ChangeList>)>
    FeedFetcherCallback;

class ChangeListLoader::FeedFetcher {
 public:
  virtual ~FeedFetcher() {}
  virtual void Run(const FeedFetcherCallback& callback) = 0;
};

namespace {

// Fetches all the (currently available) resource entries from the server.
class FullFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  explicit FullFeedFetcher(JobScheduler* scheduler)
      : scheduler_(scheduler),
        weak_ptr_factory_(this) {
  }

  virtual ~FullFeedFetcher() {
  }

  virtual void Run(const FeedFetcherCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback.is_null());

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    // This is full resource list fetch.
    scheduler_->GetAllResourceList(
        base::Bind(&FullFeedFetcher::OnFileListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnFileListFetched(
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

    DCHECK(resource_list);
    change_lists_.push_back(new ChangeList(*resource_list));

    GURL next_url;
    if (resource_list->GetNextFeedURL(&next_url) && !next_url.is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingFileList(
          next_url,
          base::Bind(&FullFeedFetcher::OnFileListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES("Drive.FullFeedLoadTime",
                             base::TimeTicks::Now() - start_time_);

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, change_lists_.Pass());
  }

  JobScheduler* scheduler_;
  ScopedVector<ChangeList> change_lists_;
  base::TimeTicks start_time_;
  base::WeakPtrFactory<FullFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FullFeedFetcher);
};

// Fetches the delta changes since |start_change_id|.
class DeltaFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  DeltaFeedFetcher(JobScheduler* scheduler, int64 start_change_id)
      : scheduler_(scheduler),
        start_change_id_(start_change_id),
        weak_ptr_factory_(this) {
  }

  virtual ~DeltaFeedFetcher() {
  }

  virtual void Run(const FeedFetcherCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback.is_null());

    scheduler_->GetChangeList(
        start_change_id_,
        base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnChangeListFetched(
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

    DCHECK(resource_list);
    change_lists_.push_back(new ChangeList(*resource_list));

    GURL next_url;
    if (resource_list->GetNextFeedURL(&next_url) && !next_url.is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingChangeList(
          next_url,
          base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, change_lists_.Pass());
  }

  JobScheduler* scheduler_;
  int64 start_change_id_;
  ScopedVector<ChangeList> change_lists_;
  base::WeakPtrFactory<DeltaFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DeltaFeedFetcher);
};

// Fetches the resource entries in the directory with |directory_resource_id|.
class FastFetchFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  FastFetchFeedFetcher(JobScheduler* scheduler,
                       DriveServiceInterface* drive_service,
                       const std::string& directory_resource_id,
                       const std::string& root_folder_id)
      : scheduler_(scheduler),
        drive_service_(drive_service),
        directory_resource_id_(directory_resource_id),
        root_folder_id_(root_folder_id),
        weak_ptr_factory_(this) {
  }

  virtual ~FastFetchFeedFetcher() {
  }

  virtual void Run(const FeedFetcherCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback.is_null());
    DCHECK(!directory_resource_id_.empty());
    DCHECK(util::IsDriveV2ApiEnabled() || !root_folder_id_.empty());

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    // We use WAPI's GetResourceListInDirectory even if Drive API v2 is
    // enabled. This is the short term work around of the performance
    // regression.

    std::string resource_id = directory_resource_id_;
    if (util::IsDriveV2ApiEnabled() &&
        directory_resource_id_ == root_folder_id_) {
      // GData WAPI doesn't accept the root directory id which is used in Drive
      // API v2. So it is necessary to translate it here.
      resource_id = util::kWapiRootDirectoryResourceId;
    }

    scheduler_->GetResourceListInDirectoryByWapi(
        resource_id,
        base::Bind(&FastFetchFeedFetcher::OnResourceListFetched,
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
    if (util::IsDriveV2ApiEnabled())
      FixResourceIdInChangeList(change_list);
    change_lists_.push_back(change_list);

    GURL next_url;
    if (resource_list->GetNextFeedURL(&next_url) && !next_url.is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingResourceList(
          next_url,
          base::Bind(&FastFetchFeedFetcher::OnResourceListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    UMA_HISTOGRAM_TIMES("Drive.DirectoryFeedLoadTime",
                        base::TimeTicks::Now() - start_time_);

    // Note: The fetcher is managed by ChangeListLoader, and the instance
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
  base::WeakPtrFactory<FastFetchFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FastFetchFeedFetcher);
};

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

LoaderController::LoaderController()
    : lock_count_(0),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

LoaderController::~LoaderController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

scoped_ptr<base::ScopedClosureRunner> LoaderController::GetLock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++lock_count_;
  return make_scoped_ptr(new base::ScopedClosureRunner(
      base::Bind(&LoaderController::Unlock,
                 weak_ptr_factory_.GetWeakPtr())));
}

void LoaderController::ScheduleRun(const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!task.is_null());

  if (lock_count_ > 0) {
    pending_tasks_.push_back(task);
  } else {
    task.Run();
  }
}

void LoaderController::Unlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_LT(0, lock_count_);

  if (--lock_count_ > 0)
    return;

  std::vector<base::Closure> tasks;
  tasks.swap(pending_tasks_);
  for (size_t i = 0; i < tasks.size(); ++i)
    tasks[i].Run();
}

AboutResourceLoader::AboutResourceLoader(JobScheduler* scheduler)
    : scheduler_(scheduler),
      weak_ptr_factory_(this) {
}

AboutResourceLoader::~AboutResourceLoader() {}

void AboutResourceLoader::GetAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (cached_about_resource_) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback,
            google_apis::HTTP_NO_CONTENT,
            base::Passed(scoped_ptr<google_apis::AboutResource>(
                new google_apis::AboutResource(*cached_about_resource_)))));
  } else {
    UpdateAboutResource(callback);
  }
}

void AboutResourceLoader::UpdateAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->GetAboutResource(
      base::Bind(&AboutResourceLoader::UpdateAboutResourceAfterGetAbout,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void AboutResourceLoader::UpdateAboutResourceAfterGetAbout(
    const google_apis::AboutResourceCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  FileError error = GDataToFileError(status);

  if (error == FILE_ERROR_OK) {
    if (cached_about_resource_ &&
        cached_about_resource_->largest_change_id() >
        about_resource->largest_change_id()) {
      LOG(WARNING) << "Local cached about resource is fresher than server, "
                   << "local = " << cached_about_resource_->largest_change_id()
                   << ", server = " << about_resource->largest_change_id();
    }

    cached_about_resource_.reset(
        new google_apis::AboutResource(*about_resource));
  }

  callback.Run(status, about_resource.Pass());
}

ChangeListLoader::ChangeListLoader(
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
      loaded_(false),
      weak_ptr_factory_(this) {
}

ChangeListLoader::~ChangeListLoader() {
  STLDeleteElements(&fast_fetch_feed_fetcher_set_);
}

bool ChangeListLoader::IsRefreshing() const {
  // Callback for change list loading is stored in pending_load_callback_[""].
  // It is non-empty if and only if there is an in-flight loading operation.
  return pending_load_callback_.find("") != pending_load_callback_.end();
}

void ChangeListLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void ChangeListLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void ChangeListLoader::CheckForUpdates(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (IsRefreshing()) {
    // There is in-flight loading. So keep the callback here, and check for
    // updates when the in-flight loading is completed.
    pending_update_check_callback_ = callback;
    return;
  }

  if (loaded_) {
    // We only start to check for updates iff the load is done.
    // I.e., we ignore checking updates if not loaded to avoid starting the
    // load without user's explicit interaction (such as opening Drive).
    logger_->Log(logging::LOG_INFO, "Checking for updates");
    Load(DirectoryFetchInfo(), callback);
  }
}

void ChangeListLoader::LoadDirectoryIfNeeded(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // If the resource metadata has been already loaded and not refreshing, it
  // means the local metadata is up to date.
  if (loaded_ && !IsRefreshing()) {
    callback.Run(FILE_ERROR_OK);
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
      base::Bind(&ChangeListLoader::LoadDirectoryIfNeededAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback,
                 true,  // should_try_loading_parent
                 base::Owned(entry)));
}

void ChangeListLoader::LoadForTesting(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  Load(DirectoryFetchInfo(), callback);
}

void ChangeListLoader::LoadDirectoryIfNeededAfterGetEntry(
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
        base::Bind(&ChangeListLoader::LoadDirectoryIfNeededAfterLoadParent,
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

void ChangeListLoader::LoadDirectoryIfNeededAfterLoadParent(
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
      base::Bind(&ChangeListLoader::LoadDirectoryIfNeededAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback,
                 false,  // should_try_loading_parent
                 base::Owned(entry)));
}

void ChangeListLoader::Load(const DirectoryFetchInfo& directory_fetch_info,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Check if this is the first time this ChangeListLoader do loading.
  // Note: IsRefreshing() depends on pending_load_callback_ so check in advance.
  const bool is_initial_load = (!loaded_ && !IsRefreshing());

  // Register the callback function to be called when it is loaded.
  const std::string& local_id = directory_fetch_info.local_id();
  pending_load_callback_[local_id].push_back(callback);

  // If loading task for |resource_id| is already running, do nothing.
  if (pending_load_callback_[local_id].size() > 1)
    return;

  // For initial loading, even for directory fetching, we do load the full
  // resource list from the server to sync up. So we register a dummy
  // callback to indicate that update for full hierarchy is running.
  if (is_initial_load && !directory_fetch_info.empty()) {
    pending_load_callback_[""].push_back(
        base::Bind(&util::EmptyFileOperationCallback));
  }

  // Check the current status of local metadata, and start loading if needed.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetLargestChangestamp,
                 base::Unretained(resource_metadata_)),
      base::Bind(&ChangeListLoader::LoadAfterGetLargestChangestamp,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 is_initial_load));
}

void ChangeListLoader::LoadAfterGetLargestChangestamp(
    const DirectoryFetchInfo& directory_fetch_info,
    bool is_initial_load,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_initial_load && local_changestamp > 0) {
    // The local data is usable. Flush callbacks to tell loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);

    // Continues to load from server in background.
    // Put dummy callbacks to indicate that fetching is still continuing.
    pending_load_callback_[directory_fetch_info.local_id()].push_back(
        base::Bind(&util::EmptyFileOperationCallback));
    if (!directory_fetch_info.empty()) {
      pending_load_callback_[""].push_back(
          base::Bind(&util::EmptyFileOperationCallback));
    }
  }

  if (directory_fetch_info.empty()) {
    about_resource_loader_->UpdateAboutResource(
        base::Bind(&ChangeListLoader::LoadAfterGetAboutResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   is_initial_load,
                   local_changestamp));
  } else {
    // Note: To be precise, we need to call UpdateAboutResource() here. However,
    // - It is costly to do GetAboutResource HTTP request every time.
    // - The chance using an old value is small; it only happens when
    //   LoadIfNeeded is called during one GetAboutResource roundtrip time
    //   of a change list fetching.
    // - Even if the value is old, it just marks the directory as older. It may
    //   trigger one future unnecessary re-fetch, but it'll never lose data.
    about_resource_loader_->GetAboutResource(
        base::Bind(&ChangeListLoader::LoadAfterGetAboutResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   is_initial_load,
                   local_changestamp));
  }
}

void ChangeListLoader::LoadAfterGetAboutResource(
    const DirectoryFetchInfo& directory_fetch_info,
    bool is_initial_load,
    int64 local_changestamp,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    if (directory_fetch_info.empty() || is_initial_load)
      OnChangeListLoadComplete(error);
    else
      OnDirectoryLoadComplete(directory_fetch_info, error);
    return;
  }

  DCHECK(about_resource);

  int64 remote_changestamp = about_resource->largest_change_id();
  int64 start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  if (directory_fetch_info.empty()) {
    if (local_changestamp >= remote_changestamp) {
      if (local_changestamp > remote_changestamp) {
        LOG(WARNING) << "Local resource metadata is fresher than server, "
                     << "local = " << local_changestamp
                     << ", server = " << remote_changestamp;
      }

      // No changes detected, tell the client that the loading was successful.
      OnChangeListLoadComplete(FILE_ERROR_OK);
      return;
    }

    // If the caller is not interested in a particular directory, just start
    // loading the change list.
    LoadChangeListFromServer(start_changestamp);
  } else {
    // If the caller is interested in a particular directory, start loading the
    // directory first.
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

    // If the directory's changestamp is up-to-date, just schedule to run the
    // callback, as there is no need to fetch the directory.
    if (directory_changestamp >= remote_changestamp) {
      LoadAfterLoadDirectory(directory_fetch_info, is_initial_load,
                             start_changestamp, FILE_ERROR_OK);
      return;
    }

    // Start fetching the directory content, and mark it with the changestamp
    // |remote_changestamp|.
    DirectoryFetchInfo new_directory_fetch_info(
        directory_fetch_info.local_id(), directory_fetch_info.resource_id(),
        remote_changestamp);
    LoadDirectoryFromServer(
        new_directory_fetch_info,
        base::Bind(&ChangeListLoader::LoadAfterLoadDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   is_initial_load,
                   start_changestamp));
  }
}

void ChangeListLoader::LoadAfterLoadDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    bool is_initial_load,
    int64 start_changestamp,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OnDirectoryLoadComplete(directory_fetch_info, error);

  // Continue to load change list if this is the first load.
  if (is_initial_load)
    LoadChangeListFromServer(start_changestamp);
}

void ChangeListLoader::OnChangeListLoadComplete(FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!loaded_ && error == FILE_ERROR_OK) {
    loaded_ = true;
    FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                      observers_,
                      OnInitialLoadComplete());
  }

  for (LoadCallbackMap::iterator it = pending_load_callback_.begin();
       it != pending_load_callback_.end();  ++it) {
    const std::vector<FileOperationCallback>& callbacks = it->second;
    for (size_t i = 0; i < callbacks.size(); ++i) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(callbacks[i], error));
    }
  }
  pending_load_callback_.clear();

  // If there is pending update check, try to load the change from the server
  // again, because there may exist an update during the completed loading.
  if (!pending_update_check_callback_.is_null()) {
    Load(DirectoryFetchInfo(),
         base::ResetAndReturn(&pending_update_check_callback_));
  }
}

void ChangeListLoader::OnDirectoryLoadComplete(
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

void ChangeListLoader::LoadChangeListFromServer(int64 start_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!change_feed_fetcher_);
  DCHECK(about_resource_loader_->cached_about_resource());

  bool is_delta_update = start_changestamp != 0;

  // Set up feed fetcher.
  if (is_delta_update) {
    change_feed_fetcher_.reset(
        new DeltaFeedFetcher(scheduler_, start_changestamp));
  } else {
    change_feed_fetcher_.reset(new FullFeedFetcher(scheduler_));
  }

  // Make a copy of cached_about_resource_ to remember at which changestamp we
  // are fetching change list.
  change_feed_fetcher_->Run(
      base::Bind(&ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(make_scoped_ptr(new google_apis::AboutResource(
                     *about_resource_loader_->cached_about_resource()))),
                 is_delta_update));
}

void ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList(
    scoped_ptr<google_apis::AboutResource> about_resource,
    bool is_delta_update,
    FileError error,
    ScopedVector<ChangeList> change_lists) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(about_resource);

  // Delete the fetcher first.
  change_feed_fetcher_.reset();

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  ChangeListProcessor* change_list_processor =
      new ChangeListProcessor(resource_metadata_);
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_update;

  logger_->Log(logging::LOG_INFO,
               "Apply change lists (is delta: %d)",
               is_delta_update);
  loader_controller_->ScheduleRun(base::Bind(
      base::IgnoreResult(
          &base::PostTaskAndReplyWithResult<FileError, FileError>),
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ChangeListProcessor::Apply,
                 base::Unretained(change_list_processor),
                 base::Passed(&about_resource),
                 base::Passed(&change_lists),
                 is_delta_update),
      base::Bind(&ChangeListLoader::LoadChangeListFromServerAfterUpdate,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(change_list_processor),
                 should_notify_changed_directories,
                 base::Time::Now())));
}

void ChangeListLoader::LoadChangeListFromServerAfterUpdate(
    ChangeListProcessor* change_list_processor,
    bool should_notify_changed_directories,
    const base::Time& start_time,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::TimeDelta elapsed = base::Time::Now() - start_time;
  logger_->Log(logging::LOG_INFO,
               "Change lists applied (elapsed time: %sms)",
               base::Int64ToString(elapsed.InMilliseconds()).c_str());

  if (should_notify_changed_directories) {
    for (std::set<base::FilePath>::iterator dir_iter =
            change_list_processor->changed_dirs().begin();
        dir_iter != change_list_processor->changed_dirs().end();
        ++dir_iter) {
      FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                        OnDirectoryChanged(*dir_iter));
    }
  }

  OnChangeListLoadComplete(error);

  FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                    observers_,
                    OnLoadFromServerComplete());
}

void ChangeListLoader::LoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
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
        base::Bind(&ChangeListLoader::LoadDirectoryFromServerAfterRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   callback,
                   base::Owned(changed_directory_path)));
    return;
  }

  FastFetchFeedFetcher* fetcher = new FastFetchFeedFetcher(
      scheduler_,
      drive_service_,
      directory_fetch_info.resource_id(),
      about_resource_loader_->cached_about_resource()->root_folder_id());
  fast_fetch_feed_fetcher_set_.insert(fetcher);
  fetcher->Run(
      base::Bind(&ChangeListLoader::LoadDirectoryFromServerAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback,
                 fetcher));
}

void ChangeListLoader::LoadDirectoryFromServerAfterLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    FeedFetcher* fetcher,
    FileError error,
    ScopedVector<ChangeList> change_lists) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  // Delete the fetcher.
  fast_fetch_feed_fetcher_set_.erase(fetcher);
  delete fetcher;

  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.local_id()
               << ": " << FileErrorToString(error);
    callback.Run(error);
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
      base::Bind(&ChangeListLoader::LoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback,
                 base::Owned(directory_path))));
}

void ChangeListLoader::LoadDirectoryFromServerAfterRefresh(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    const base::FilePath* directory_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
  callback.Run(error);
  // Also notify the observers.
  if (error == FILE_ERROR_OK && !directory_path->empty()) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                      OnDirectoryChanged(*directory_path));
  }
}

}  // namespace internal
}  // namespace drive
