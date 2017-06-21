// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/change_list_loader.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "google_apis/drive/drive_api_parser.h"
#include "url/gurl.h"

namespace drive {
namespace internal {

typedef base::Callback<void(FileError,
                            std::vector<std::unique_ptr<ChangeList>>)>
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
  FullFeedFetcher(
      JobScheduler* scheduler,
      google_apis::TeamDrivesIntegrationStatus team_drives_integration)
      : scheduler_(scheduler),
        is_team_drive_enabled_(team_drives_integration ==
                               google_apis::TEAM_DRIVES_INTEGRATION_ENABLED),
        weak_ptr_factory_(this) {}

  ~FullFeedFetcher() override {}

  void Run(const FeedFetcherCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    if (!is_team_drive_enabled_) {
      StartFetchFileList(callback);
      return;
    }

    // Fetch Team Drive before File list, so that files can be stored under
    // root directories of each Team Drive like /team_drive/My Team Drive/.
    scheduler_->GetAllTeamDriveList(
        base::Bind(&FullFeedFetcher::OnTeamDriveListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnTeamDriveListFetched(
      const FeedFetcherCallback& callback,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::TeamDriveList> team_drives) {
    DCHECK(is_team_drive_enabled_);
    change_lists_.push_back(base::MakeUnique<ChangeList>(*team_drives));

    if (!team_drives->next_page_token().empty()) {
      scheduler_->GetRemainingTeamDriveList(
          team_drives->next_page_token(),
          base::Bind(&FullFeedFetcher::OnTeamDriveListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }
    StartFetchFileList(callback);
  }

  void StartFetchFileList(const FeedFetcherCallback& callback) {
    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();
    // This is full resource list fetch.
    //
    // NOTE: Because we already know the largest change ID, here we can use
    // files.list instead of changes.list for speed. crbug.com/287602
    scheduler_->GetAllFileList(
        base::Bind(&FullFeedFetcher::OnFileListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void OnFileListFetched(const FeedFetcherCallback& callback,
                         google_apis::DriveApiErrorCode status,
                         std::unique_ptr<google_apis::FileList> file_list) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error, std::vector<std::unique_ptr<ChangeList>>());
      return;
    }

    DCHECK(file_list);
    change_lists_.push_back(base::MakeUnique<ChangeList>(*file_list));

    if (!file_list->next_link().is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingFileList(
          file_list->next_link(),
          base::Bind(&FullFeedFetcher::OnFileListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES("Drive.FullFeedLoadTime",
                             base::TimeTicks::Now() - start_time_);

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, std::move(change_lists_));
  }

  JobScheduler* scheduler_;
  std::vector<std::unique_ptr<ChangeList>> change_lists_;
  base::TimeTicks start_time_;
  base::ThreadChecker thread_checker_;
  bool is_team_drive_enabled_;
  base::WeakPtrFactory<FullFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FullFeedFetcher);
};

// Fetches the delta changes since |start_change_id|.
class DeltaFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  DeltaFeedFetcher(JobScheduler* scheduler, int64_t start_change_id)
      : scheduler_(scheduler),
        start_change_id_(start_change_id),
        weak_ptr_factory_(this) {}

  ~DeltaFeedFetcher() override {}

  void Run(const FeedFetcherCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    scheduler_->GetChangeList(
        start_change_id_,
        base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnChangeListFetched(
      const FeedFetcherCallback& callback,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::ChangeList> change_list) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!callback.is_null());

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error, std::vector<std::unique_ptr<ChangeList>>());
      return;
    }

    DCHECK(change_list);
    change_lists_.push_back(base::MakeUnique<ChangeList>(*change_list));

    if (!change_list->next_link().is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingChangeList(
          change_list->next_link(),
          base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, std::move(change_lists_));
  }

  JobScheduler* scheduler_;
  int64_t start_change_id_;
  std::vector<std::unique_ptr<ChangeList>> change_lists_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DeltaFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DeltaFeedFetcher);
};

}  // namespace

LoaderController::LoaderController()
    : lock_count_(0),
      weak_ptr_factory_(this) {
}

LoaderController::~LoaderController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<base::ScopedClosureRunner> LoaderController::GetLock() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ++lock_count_;
  return base::MakeUnique<base::ScopedClosureRunner>(
      base::Bind(&LoaderController::Unlock, weak_ptr_factory_.GetWeakPtr()));
}

void LoaderController::ScheduleRun(const base::Closure& task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!task.is_null());

  if (lock_count_ > 0) {
    pending_tasks_.push_back(task);
  } else {
    task.Run();
  }
}

void LoaderController::Unlock() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
      current_update_task_id_(-1),
      weak_ptr_factory_(this) {
}

AboutResourceLoader::~AboutResourceLoader() {}

void AboutResourceLoader::GetAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  // If the latest UpdateAboutResource task is still running. Wait for it,
  if (pending_callbacks_.count(current_update_task_id_)) {
    pending_callbacks_[current_update_task_id_].push_back(callback);
    return;
  }

  if (cached_about_resource_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, google_apis::HTTP_NO_CONTENT,
            base::Passed(std::unique_ptr<google_apis::AboutResource>(
                new google_apis::AboutResource(*cached_about_resource_)))));
  } else {
    UpdateAboutResource(callback);
  }
}

void AboutResourceLoader::UpdateAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  ++current_update_task_id_;
  pending_callbacks_[current_update_task_id_].push_back(callback);

  scheduler_->GetAboutResource(
      base::Bind(&AboutResourceLoader::UpdateAboutResourceAfterGetAbout,
                 weak_ptr_factory_.GetWeakPtr(),
                 current_update_task_id_));
}

void AboutResourceLoader::UpdateAboutResourceAfterGetAbout(
    int task_id,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FileError error = GDataToFileError(status);

  const std::vector<google_apis::AboutResourceCallback> callbacks =
      pending_callbacks_[task_id];
  pending_callbacks_.erase(task_id);

  if (error != FILE_ERROR_OK) {
    for (size_t i = 0; i < callbacks.size(); ++i)
      callbacks[i].Run(status, std::unique_ptr<google_apis::AboutResource>());
    return;
  }

  // Updates the cache when the resource is successfully obtained.
  if (cached_about_resource_ &&
      cached_about_resource_->largest_change_id() >
      about_resource->largest_change_id()) {
    LOG(WARNING) << "Local cached about resource is fresher than server, "
                 << "local = " << cached_about_resource_->largest_change_id()
                 << ", server = " << about_resource->largest_change_id();
  }
  cached_about_resource_.reset(new google_apis::AboutResource(*about_resource));

  for (size_t i = 0; i < callbacks.size(); ++i) {
    callbacks[i].Run(
        status, base::MakeUnique<google_apis::AboutResource>(*about_resource));
  }
}

ChangeListLoader::ChangeListLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    AboutResourceLoader* about_resource_loader,
    LoaderController* loader_controller)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      in_shutdown_(new base::CancellationFlag),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      about_resource_loader_(about_resource_loader),
      loader_controller_(loader_controller),
      loaded_(false),
      weak_ptr_factory_(this) {
}

ChangeListLoader::~ChangeListLoader() {
  in_shutdown_->Set();
  // Delete |in_shutdown_| with the blocking task runner so that it gets deleted
  // after all active ChangeListProcessors.
  blocking_task_runner_->DeleteSoon(FROM_HERE, in_shutdown_.release());
}

bool ChangeListLoader::IsRefreshing() const {
  // Callback for change list loading is stored in pending_load_callback_.
  // It is non-empty if and only if there is an in-flight loading operation.
  return !pending_load_callback_.empty();
}

void ChangeListLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void ChangeListLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void ChangeListLoader::CheckForUpdates(const FileOperationCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  // We only start to check for updates iff the load is done.
  // I.e., we ignore checking updates if not loaded to avoid starting the
  // load without user's explicit interaction (such as opening Drive).
  if (!loaded_ && !IsRefreshing())
    return;

  // For each CheckForUpdates() request, always refresh the changestamp info.
  about_resource_loader_->UpdateAboutResource(
      base::Bind(&ChangeListLoader::OnAboutResourceUpdated,
                 weak_ptr_factory_.GetWeakPtr()));

  if (IsRefreshing()) {
    // There is in-flight loading. So keep the callback here, and check for
    // updates when the in-flight loading is completed.
    pending_update_check_callback_ = callback;
    return;
  }

  DCHECK(loaded_);
  logger_->Log(logging::LOG_INFO, "Checking for updates");
  Load(callback);
}

void ChangeListLoader::LoadIfNeeded(const FileOperationCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  // If the metadata is not yet loaded, start loading.
  if (!loaded_ && !IsRefreshing())
    Load(callback);
}

void ChangeListLoader::Load(const FileOperationCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  // Check if this is the first time this ChangeListLoader do loading.
  // Note: IsRefreshing() depends on pending_load_callback_ so check in advance.
  const bool is_initial_load = (!loaded_ && !IsRefreshing());

  // Register the callback function to be called when it is loaded.
  pending_load_callback_.push_back(callback);

  // If loading task is already running, do nothing.
  if (pending_load_callback_.size() > 1)
    return;

  // Check the current status of local metadata, and start loading if needed.
  int64_t* local_changestamp = new int64_t(0);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetLargestChangestamp,
                 base::Unretained(resource_metadata_),
                 local_changestamp),
      base::Bind(&ChangeListLoader::LoadAfterGetLargestChangestamp,
                 weak_ptr_factory_.GetWeakPtr(),
                 is_initial_load,
                 base::Owned(local_changestamp)));
}

void ChangeListLoader::LoadAfterGetLargestChangestamp(
    bool is_initial_load,
    const int64_t* local_changestamp,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  if (is_initial_load && *local_changestamp > 0) {
    // The local data is usable. Flush callbacks to tell loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);

    // Continues to load from server in background.
    // Put dummy callbacks to indicate that fetching is still continuing.
    pending_load_callback_.push_back(
        base::Bind(&util::EmptyFileOperationCallback));
  }

  about_resource_loader_->GetAboutResource(
      base::Bind(&ChangeListLoader::LoadAfterGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 *local_changestamp));
}

void ChangeListLoader::LoadAfterGetAboutResource(
    int64_t local_changestamp,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(thread_checker_.CalledOnValidThread());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  DCHECK(about_resource);

  int64_t remote_changestamp = about_resource->largest_change_id();
  int64_t start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  if (local_changestamp >= remote_changestamp) {
    if (local_changestamp > remote_changestamp) {
      LOG(WARNING) << "Local resource metadata is fresher than server, "
                   << "local = " << local_changestamp
                   << ", server = " << remote_changestamp;
    }

    // No changes detected, tell the client that the loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);
  } else {
    // Start loading the change list.
    LoadChangeListFromServer(start_changestamp);
  }
}

void ChangeListLoader::OnChangeListLoadComplete(FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!loaded_ && error == FILE_ERROR_OK) {
    loaded_ = true;
    for (auto& observer : observers_)
      observer.OnInitialLoadComplete();
  }

  for (size_t i = 0; i < pending_load_callback_.size(); ++i) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(pending_load_callback_[i], error));
  }
  pending_load_callback_.clear();

  // If there is pending update check, try to load the change from the server
  // again, because there may exist an update during the completed loading.
  if (!pending_update_check_callback_.is_null()) {
    Load(base::ResetAndReturn(&pending_update_check_callback_));
  }
}

void ChangeListLoader::OnAboutResourceUpdated(
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::AboutResource> resource) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (drive::GDataToFileError(error) != drive::FILE_ERROR_OK) {
    logger_->Log(logging::LOG_ERROR,
                 "Failed to update the about resource: %s",
                 google_apis::DriveApiErrorCodeToString(error).c_str());
    return;
  }
  logger_->Log(logging::LOG_INFO,
               "About resource updated to: %s",
               base::Int64ToString(resource->largest_change_id()).c_str());
}

void ChangeListLoader::LoadChangeListFromServer(int64_t start_changestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!change_feed_fetcher_);
  DCHECK(about_resource_loader_->cached_about_resource());

  bool is_delta_update = start_changestamp != 0;

  // Set up feed fetcher.
  if (is_delta_update) {
    change_feed_fetcher_.reset(
        new DeltaFeedFetcher(scheduler_, start_changestamp));
  } else {
    change_feed_fetcher_.reset(new FullFeedFetcher(
        scheduler_, google_apis::GetTeamDrivesIntegrationSwitch()));
  }

  // Make a copy of cached_about_resource_ to remember at which changestamp we
  // are fetching change list.
  change_feed_fetcher_->Run(
      base::Bind(&ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(base::MakeUnique<google_apis::AboutResource>(
                     *about_resource_loader_->cached_about_resource())),
                 is_delta_update));
}

void ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList(
    std::unique_ptr<google_apis::AboutResource> about_resource,
    bool is_delta_update,
    FileError error,
    std::vector<std::unique_ptr<ChangeList>> change_lists) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(about_resource);

  // Delete the fetcher first.
  change_feed_fetcher_.reset();

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  ChangeListProcessor* change_list_processor =
      new ChangeListProcessor(resource_metadata_, in_shutdown_.get());
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_update;

  logger_->Log(logging::LOG_INFO,
               "Apply change lists (is delta: %d)",
               is_delta_update);
  loader_controller_->ScheduleRun(base::Bind(
      &drive::util::RunAsyncTask, base::RetainedRef(blocking_task_runner_),
      FROM_HERE,
      base::Bind(&ChangeListProcessor::Apply,
                 base::Unretained(change_list_processor),
                 base::Passed(&about_resource), base::Passed(&change_lists),
                 is_delta_update),
      base::Bind(&ChangeListLoader::LoadChangeListFromServerAfterUpdate,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(change_list_processor),
                 should_notify_changed_directories, base::Time::Now())));
}

void ChangeListLoader::LoadChangeListFromServerAfterUpdate(
    ChangeListProcessor* change_list_processor,
    bool should_notify_changed_directories,
    const base::Time& start_time,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::TimeDelta elapsed = base::Time::Now() - start_time;
  logger_->Log(logging::LOG_INFO,
               "Change lists applied (elapsed time: %sms)",
               base::Int64ToString(elapsed.InMilliseconds()).c_str());

  if (should_notify_changed_directories) {
    for (auto& observer : observers_)
      observer.OnFileChanged(change_list_processor->changed_files());
  }

  OnChangeListLoadComplete(error);

  for (auto& observer : observers_)
    observer.OnLoadFromServerComplete();
}

}  // namespace internal
}  // namespace drive
