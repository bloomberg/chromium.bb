// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/change_list_loader.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/drive_file_util.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/chromeos/root_folder_id_loader.h"
#include "components/drive/chromeos/start_page_token_loader.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_change.h"
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
  virtual ~FeedFetcher() = default;
  virtual void Run(const FeedFetcherCallback& callback) = 0;
};

namespace {

constexpr char kDefaultCorpusMsg[] = "default corpus";

// Fetches all the (currently available) resource entries from the server.
class FullFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  FullFeedFetcher(JobScheduler* scheduler, const std::string& team_drive_id)
      : scheduler_(scheduler),
        team_drive_id_(team_drive_id),
        weak_ptr_factory_(this) {}

  ~FullFeedFetcher() override = default;

  void Run(const FeedFetcherCallback& callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();
    // This is full resource list fetch.
    //
    // NOTE: Because we already know the largest change ID, here we can use
    // files.list instead of changes.list for speed. crbug.com/287602
    scheduler_->GetAllFileList(
        team_drive_id_, base::Bind(&FullFeedFetcher::OnFileListFetched,
                                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnFileListFetched(const FeedFetcherCallback& callback,
                         google_apis::DriveApiErrorCode status,
                         std::unique_ptr<google_apis::FileList> file_list) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error, std::vector<std::unique_ptr<ChangeList>>());
      return;
    }

    DCHECK(file_list);
    change_lists_.push_back(std::make_unique<ChangeList>(*file_list));

    if (!file_list->next_link().is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingFileList(
          file_list->next_link(),
          base::Bind(&FullFeedFetcher::OnFileListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    if (team_drive_id_.empty()) {
      UMA_HISTOGRAM_LONG_TIMES("Drive.FullFeedLoadTime", duration);
    } else {
      UMA_HISTOGRAM_LONG_TIMES("Drive.FullFeedLoadTime.TeamDrives", duration);
    }

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, std::move(change_lists_));
  }

  JobScheduler* scheduler_;
  const std::string team_drive_id_;
  std::vector<std::unique_ptr<ChangeList>> change_lists_;
  base::TimeTicks start_time_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<FullFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FullFeedFetcher);
};

// Fetches the delta changes since |start_change_id|.
class DeltaFeedFetcher : public ChangeListLoader::FeedFetcher {
 public:
  DeltaFeedFetcher(JobScheduler* scheduler,
                   const std::string& team_drive_id,
                   const std::string& start_page_token)
      : scheduler_(scheduler),
        team_drive_id_(team_drive_id),
        start_page_token_(start_page_token),
        weak_ptr_factory_(this) {}

  ~DeltaFeedFetcher() override = default;

  void Run(const FeedFetcherCallback& callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    scheduler_->GetChangeList(
        team_drive_id_, start_page_token_,
        base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnChangeListFetched(
      const FeedFetcherCallback& callback,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::ChangeList> change_list) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error, std::vector<std::unique_ptr<ChangeList>>());
      return;
    }

    DCHECK(change_list);
    change_lists_.push_back(std::make_unique<ChangeList>(*change_list));

    if (!change_list->next_link().is_empty()) {
      // There is the remaining result so fetch it.
      scheduler_->GetRemainingChangeList(
          change_list->next_link(),
          base::Bind(&DeltaFeedFetcher::OnChangeListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    if (team_drive_id_.empty()) {
      UMA_HISTOGRAM_LONG_TIMES("Drive.DeltaFeedLoadTime", duration);
    } else {
      UMA_HISTOGRAM_LONG_TIMES("Drive.DeltaFeedLoadTime.TeamDrives", duration);
    }

    // Note: The fetcher is managed by ChangeListLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK, std::move(change_lists_));
  }

  JobScheduler* scheduler_;
  const std::string team_drive_id_;
  const std::string start_page_token_;
  std::vector<std::unique_ptr<ChangeList>> change_lists_;
  base::TimeTicks start_time_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<DeltaFeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DeltaFeedFetcher);
};

}  // namespace

ChangeListLoader::ChangeListLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    RootFolderIdLoader* root_folder_id_loader,
    StartPageTokenLoader* start_page_token_loader,
    LoaderController* loader_controller,
    const std::string& team_drive_id,
    const base::FilePath& root_entry_path)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      in_shutdown_(new base::AtomicFlag),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      root_folder_id_loader_(root_folder_id_loader),
      start_page_token_loader_(start_page_token_loader),
      loader_controller_(loader_controller),
      loaded_(false),
      team_drive_id_(team_drive_id),
      team_drive_msg_(team_drive_id_.empty()
                          ? kDefaultCorpusMsg
                          : base::StrCat({"team drive id: ", team_drive_id_})),
      root_entry_path_(root_entry_path),
      weak_ptr_factory_(this) {}

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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void ChangeListLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void ChangeListLoader::CheckForUpdates(const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  // We only start to check for updates iff the load is done.
  // I.e., we ignore checking updates if not loaded to avoid starting the
  // load without user's explicit interaction (such as opening Drive).
  if (!loaded_ && !IsRefreshing())
    return;

  // For each CheckForUpdates() request, always refresh the start_page_token.
  start_page_token_loader_->UpdateStartPageToken(
      base::Bind(&ChangeListLoader::OnStartPageTokenLoaderUpdated,
                 weak_ptr_factory_.GetWeakPtr()));

  if (IsRefreshing()) {
    // There is in-flight loading. So keep the callback here, and check for
    // updates when the in-flight loading is completed.
    pending_update_check_callback_ = callback;
    return;
  }

  DCHECK(loaded_);
  logger_->Log(logging::LOG_INFO, "Checking for updates (%s)",
               team_drive_msg_.c_str());
  Load(callback);
}

void ChangeListLoader::LoadIfNeeded(const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  // If the metadata is not yet loaded, start loading.
  if (!loaded_ && !IsRefreshing())
    Load(callback);
  else
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, FILE_ERROR_OK));
}

void ChangeListLoader::Load(const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  // Check if this is the first time this ChangeListLoader do loading.
  // Note: IsRefreshing() depends on pending_load_callback_ so check in advance.
  const bool is_initial_load = (!loaded_ && !IsRefreshing());

  // Register the callback function to be called when it is loaded.
  pending_load_callback_.push_back(callback);

  // If loading task is already running, do nothing.
  if (pending_load_callback_.size() > 1)
    return;

  // Check the current status of local metadata, and start loading if needed.
  std::string* start_page_token = new std::string();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&GetStartPageToken, base::Unretained(resource_metadata_),
                 team_drive_id_, start_page_token),
      base::Bind(&ChangeListLoader::LoadAfterGetLocalStartPageToken,
                 weak_ptr_factory_.GetWeakPtr(), is_initial_load,
                 base::Owned(start_page_token)));
}

void ChangeListLoader::LoadAfterGetLocalStartPageToken(
    bool is_initial_load,
    const std::string* local_start_page_token,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_start_page_token);

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  if (is_initial_load && !local_start_page_token->empty()) {
    // The local data is usable. Flush callbacks to tell loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);

    // Continues to load from server in background.
    // Put dummy callbacks to indicate that fetching is still continuing.
    pending_load_callback_.push_back(base::DoNothing());
  }

  root_folder_id_loader_->GetRootFolderId(
      base::Bind(&ChangeListLoader::LoadAfterGetRootFolderId,
                 weak_ptr_factory_.GetWeakPtr(), *local_start_page_token));
}

void ChangeListLoader::LoadAfterGetRootFolderId(
    const std::string& local_start_page_token,
    FileError error,
    base::Optional<std::string> root_folder_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!change_feed_fetcher_);

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  DCHECK(root_folder_id);

  start_page_token_loader_->GetStartPageToken(
      base::Bind(&ChangeListLoader::LoadAfterGetStartPageToken,
                 weak_ptr_factory_.GetWeakPtr(), local_start_page_token,
                 std::move(root_folder_id.value())));
}

void ChangeListLoader::LoadAfterGetStartPageToken(
    const std::string& local_start_page_token,
    const std::string& root_folder_id,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::StartPageToken> start_page_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  DCHECK(start_page_token);

  LoadChangeListFromServer(start_page_token->start_page_token(),
                           local_start_page_token, root_folder_id);
}

void ChangeListLoader::OnChangeListLoadComplete(FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!loaded_ && error == FILE_ERROR_OK) {
    loaded_ = true;
    for (auto& observer : observers_)
      observer.OnInitialLoadComplete();
  }

  for (auto& callback : pending_load_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, error));
  }
  pending_load_callback_.clear();

  // If there is pending update check, try to load the change from the server
  // again, because there may exist an update during the completed loading.
  if (pending_update_check_callback_) {
    auto cb = std::move(pending_update_check_callback_);
    // TODO(dcheng): Rewrite this to use OnceCallback. Load() currently takes a
    // callback by const ref, so std::move() won't do anything. :(
    Load(cb);
  }
}

void ChangeListLoader::OnStartPageTokenLoaderUpdated(
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::StartPageToken> start_page_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (drive::GDataToFileError(error) != drive::FILE_ERROR_OK) {
    logger_->Log(logging::LOG_ERROR,
                 "Failed to update the start page token (%s) (Error: %s)",
                 team_drive_msg_.c_str(),
                 google_apis::DriveApiErrorCodeToString(error).c_str());
    return;
  }
  logger_->Log(logging::LOG_INFO, "Start page token updated (%s) (value: %s)",
               team_drive_msg_.c_str(),
               start_page_token->start_page_token().c_str());
}

void ChangeListLoader::LoadChangeListFromServer(
    const std::string& remote_start_page_token,
    const std::string& local_start_page_token,
    const std::string& root_resource_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (local_start_page_token == remote_start_page_token) {
    // No changes detected, tell the client that the loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);
    return;
  }

  // Set up feed fetcher.
  bool is_delta_update = !local_start_page_token.empty();
  if (is_delta_update) {
    change_feed_fetcher_ = std::make_unique<DeltaFeedFetcher>(
        scheduler_, team_drive_id_, local_start_page_token);
  } else {
    change_feed_fetcher_ =
        std::make_unique<FullFeedFetcher>(scheduler_, team_drive_id_);
  }

  change_feed_fetcher_->Run(
      base::Bind(&ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList,
                 weak_ptr_factory_.GetWeakPtr(), remote_start_page_token,
                 root_resource_id, is_delta_update));
}

void ChangeListLoader::LoadChangeListFromServerAfterLoadChangeList(
    const std::string& start_page_token,
    const std::string& root_resource_id,
    bool is_delta_update,
    FileError error,
    std::vector<std::unique_ptr<ChangeList>> change_lists) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Delete the fetcher first.
  change_feed_fetcher_.reset();

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  ChangeListProcessor* change_list_processor = new ChangeListProcessor(
      team_drive_id_, root_entry_path_, resource_metadata_, in_shutdown_.get());
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_update;

  logger_->Log(logging::LOG_INFO, "Apply change lists (%s) (is delta: %d)",
               team_drive_msg_.c_str(), is_delta_update);
  loader_controller_->ScheduleRun(base::BindOnce(
      &drive::util::RunAsyncTask, base::RetainedRef(blocking_task_runner_),
      FROM_HERE,
      base::BindOnce(&ChangeListProcessor::ApplyUserChangeList,
                     base::Unretained(change_list_processor), start_page_token,
                     root_resource_id, std::move(change_lists),
                     is_delta_update),
      base::BindOnce(&ChangeListLoader::LoadChangeListFromServerAfterUpdate,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(change_list_processor),
                     should_notify_changed_directories, base::Time::Now())));
}

void ChangeListLoader::LoadChangeListFromServerAfterUpdate(
    ChangeListProcessor* change_list_processor,
    bool should_notify_changed_directories,
    base::Time start_time,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const base::TimeDelta elapsed = base::Time::Now() - start_time;
  logger_->Log(logging::LOG_INFO,
               "Change lists applied (%s) (elapsed time: %sms)",
               team_drive_msg_.c_str(),
               base::NumberToString(elapsed.InMilliseconds()).c_str());

  if (should_notify_changed_directories) {
    for (auto& observer : observers_)
      observer.OnFileChanged(change_list_processor->changed_files());
  }

  if (!change_list_processor->changed_team_drives().empty()) {
    for (auto& observer : observers_) {
      observer.OnTeamDrivesChanged(
          change_list_processor->changed_team_drives());
    }
  }

  OnChangeListLoadComplete(error);

  for (auto& observer : observers_)
    observer.OnLoadFromServerComplete();
}

}  // namespace internal
}  // namespace drive
