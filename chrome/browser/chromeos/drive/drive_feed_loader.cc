// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_feed_loader.h"

#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_feed_loader_observer.h"
#include "chrome/browser/chromeos/drive/drive_feed_processor.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {

const FilePath::CharType kFilesystemProtoFile[] =
    FILE_PATH_LITERAL("file_system.pb");
const FilePath::CharType kResourceMetadataDBFile[] =
    FILE_PATH_LITERAL("resource_metadata.db");

// Update the fetch progress UI per every this number of feeds.
const int kFetchUiUpdateStep = 10;

// Schedule for dumping root file system proto buffers to disk depending its
// total protobuffer size in MB.
typedef struct {
  double size;
  int timeout;
} SerializationTimetable;

SerializationTimetable kSerializeTimetable[] = {
#ifndef NDEBUG
    {0.5, 0},    // Less than 0.5MB, dump immediately.
    {-1,  1},    // Any size, dump if older than 1 minute.
#else
    {0.5, 0},    // Less than 0.5MB, dump immediately.
    {1.0, 15},   // Less than 1.0MB, dump after 15 minutes.
    {2.0, 30},
    {4.0, 60},
    {-1,  120},  // Any size, dump if older than 120 minutes.
#endif
};

// Loads the file at |path| into the string |serialized_proto| on a blocking
// thread.
DriveFileError LoadProtoOnBlockingPool(const FilePath& path,
                                       base::Time* last_modified,
                                       std::string* serialized_proto) {
  base::PlatformFileInfo info;
  if (!file_util::GetFileInfo(path, &info))
    return DRIVE_FILE_ERROR_NOT_FOUND;
  *last_modified = info.last_modified;
  if (!file_util::ReadFileToString(path, serialized_proto)) {
    LOG(WARNING) << "Proto file not found at " << path.value();
    return DRIVE_FILE_ERROR_NOT_FOUND;
  }
  return DRIVE_FILE_OK;
}

// Returns true if file system is due to be serialized on disk based on it
// |serialized_size| and |last_serialized| timestamp.
bool ShouldSerializeFileSystemNow(size_t serialized_size,
                                  const base::Time& last_serialized) {
  const double size_in_mb = serialized_size / 1048576.0;
  const int last_proto_dump_in_min =
      (base::Time::Now() - last_serialized).InMinutes();
  for (size_t i = 0; i < arraysize(kSerializeTimetable); i++) {
    if ((size_in_mb < kSerializeTimetable[i].size ||
         kSerializeTimetable[i].size == -1) &&
        last_proto_dump_in_min >= kSerializeTimetable[i].timeout) {
      return true;
    }
  }
  return false;
}

// Saves the string |serialized_proto| to a file at |path| on a blocking thread.
void SaveProtoOnBlockingPool(const FilePath& path,
                             scoped_ptr<std::string> serialized_proto) {
  const int file_size = static_cast<int>(serialized_proto->length());
  if (file_util::WriteFile(path, serialized_proto->data(), file_size) !=
      file_size) {
    LOG(WARNING) << "Drive proto file can't be stored at "
                 << path.value();
    if (!file_util::Delete(path, true)) {
      LOG(WARNING) << "Drive proto file can't be deleted at "
                   << path.value();
    }
  }
}

bool UseLevelDB() {
  // TODO(achuith): Re-enable this.
  return false;
}

// Parses a google_apis::ResourceList from |data|.
scoped_ptr<google_apis::ResourceList> ParseFeedOnBlockingPool(
    scoped_ptr<base::Value> data) {
  return google_apis::ResourceList::ExtractAndParse(*data);
}

}  // namespace

// Set of parameters sent to LoadFromServer.
//
// Value of |start_changestamp| determines the type of feed to load - 0 means
// root feed, every other value would trigger delta feed.
//
// Loaded feed may be partial due to size limit on a single feed. In that case,
// the loaded feed will have next feed url set. Iff |load_subsequent_feeds|
// parameter is set, feed loader will load all subsequent feeds.
//
// If invoked as a part of content search, query will be set in |search_query|.
// If |feed_to_load| is set, this is feed url that will be used to load feed.
//
// When all feeds are loaded, |feed_load_callback| is invoked with the retrieved
// feeds. |feed_load_callback| must not be null.
struct DriveFeedLoader::LoadFeedParams {
  explicit LoadFeedParams(const LoadFeedListCallback& feed_load_callback)
      : start_changestamp(0),
        shared_with_me(false),
        load_subsequent_feeds(true),
        feed_load_callback(feed_load_callback) {}

  // Runs this->feed_load_callback with |error|.
  void RunFeedLoadCallback(DriveFileError error) {
    feed_load_callback.Run(feed_list, error);
  }

  // Changestamps are positive numbers in increasing order. The difference
  // between two changestamps is proportional equal to number of items in
  // delta feed between them - bigger the difference, more likely bigger
  // number of items in delta feeds.
  int64 start_changestamp;
  std::string search_query;
  bool shared_with_me;
  std::string directory_resource_id;
  GURL feed_to_load;
  bool load_subsequent_feeds;
  const LoadFeedListCallback feed_load_callback;
  ScopedVector<google_apis::ResourceList> feed_list;
  scoped_ptr<GetResourceListUiState> ui_state;
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct DriveFeedLoader::LoadRootFeedParams {
  explicit LoadRootFeedParams(const FileOperationCallback& callback)
      : load_start_time(base::Time::Now()),
        callback(callback) {}

  std::string proto;
  base::Time last_modified;
  // Time when filesystem began to be loaded from disk.
  base::Time load_start_time;
  const FileOperationCallback callback;
};

// Defines parameters sent to UpdateMetadataFromFeedAfterLoadFromServer().
//
// In the case of loading the root feed we use |root_feed_changestamp| as its
// initial changestamp value since it does not come with that info.
//
// On initial feed load for Drive API, remember root ID for
// DriveResourceData initialization later in UpdateFromFeed().
struct DriveFeedLoader::UpdateMetadataParams {
  UpdateMetadataParams(bool is_delta_feed,
                       int64 feed_changestamp,
                       const FileOperationCallback& callback)
      : is_delta_feed(is_delta_feed),
        feed_changestamp(feed_changestamp),
        callback(callback) {}

  const bool is_delta_feed;
  const int64 feed_changestamp;
  const FileOperationCallback callback;
};

// Defines set of parameters sent to callback OnNotifyResourceListFetched().
// This is a trick to update the number of fetched documents frequently on
// UI. Due to performance reason, we need to fetch a number of files at
// a time. However, it'll take long time, and a user has no way to know
// the current update state. In order to make users comfortable,
// we increment the number of fetched documents with more frequent but smaller
// steps than actual fetching.
struct DriveFeedLoader::GetResourceListUiState {
  explicit GetResourceListUiState(base::TimeTicks start_time)
      : num_fetched_documents(0),
        num_showing_documents(0),
        start_time(start_time),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory(this)) {
  }

  // The number of fetched documents.
  int num_fetched_documents;

  // The number documents shown on UI.
  int num_showing_documents;

  // When the UI update has started.
  base::TimeTicks start_time;

  // Time elapsed since the feed fetching was started.
  base::TimeDelta feed_fetching_elapsed_time;

  base::WeakPtrFactory<GetResourceListUiState> weak_ptr_factory;
};

DriveFeedLoader::DriveFeedLoader(
    DriveResourceMetadata* resource_metadata,
    DriveScheduler* scheduler,
    DriveWebAppsRegistry* webapps_registry,
    DriveCache* cache,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      webapps_registry_(webapps_registry),
      cache_(cache),
      blocking_task_runner_(blocking_task_runner),
      refreshing_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DriveFeedLoader::~DriveFeedLoader() {
}

void DriveFeedLoader::AddObserver(DriveFeedLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DriveFeedLoader::RemoveObserver(DriveFeedLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DriveFeedLoader::ReloadFromServerIfNeeded(
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Sets the refreshing flag, so that the caller does not send refresh requests
  // in parallel (see DriveFileSystem::CheckForUpdates). Corresponding
  // "refresh_ = false" is in OnGetAccountMetadata when the cached feed is up to
  // date, or in OnFeedFromServerLoaded called back from LoadFromServer().
  refreshing_ = true;

  if (google_apis::util::IsDriveV2ApiEnabled()) {
    // Drive v2 needs a separate application list fetch operation.
    // TODO(haruki): Application list rarely changes and is not necessarily
    // refreshed as often as files.
    scheduler_->GetApplicationInfo(
        base::Bind(&DriveFeedLoader::OnGetApplicationList,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // First fetch the latest changestamp to see if there were any new changes
  // there at all.
  scheduler_->GetAccountMetadata(
      base::Bind(&DriveFeedLoader::OnGetAccountMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveFeedLoader::OnGetAccountMetadata(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AccountMetadataFeed> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  int64 remote_changestamp = 0;
  // When account metadata successfully fetched, parse the latest changestamp.
  if (util::GDataToDriveFileError(status) == DRIVE_FILE_OK) {
    DCHECK(account_metadata);
    webapps_registry_->UpdateFromFeed(*account_metadata);
    remote_changestamp = account_metadata->largest_changestamp();
  }

  resource_metadata_->GetLargestChangestamp(
      base::Bind(&DriveFeedLoader::CompareChangestampsAndLoadIfNeeded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 remote_changestamp));
}

void DriveFeedLoader::CompareChangestampsAndLoadIfNeeded(
    const FileOperationCallback& callback,
    int64 remote_changestamp,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  if (remote_changestamp > 0 && local_changestamp >= remote_changestamp) {
    if (local_changestamp > remote_changestamp) {
      LOG(WARNING) << "Cached client feed is fresher than server, client = "
                   << local_changestamp
                   << ", server = "
                   << remote_changestamp;
    }

    // No changes detected, tell the client that the loading was successful.
    refreshing_ = false;
    callback.Run(DRIVE_FILE_OK);
    return;
  }

  // Load changes from the server.
  int64 start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  scoped_ptr<LoadFeedParams> load_params(new LoadFeedParams(
      base::Bind(&DriveFeedLoader::UpdateMetadataFromFeedAfterLoadFromServer,
                 weak_ptr_factory_.GetWeakPtr(),
                 UpdateMetadataParams(start_changestamp != 0,  // is_delta_feed
                                      remote_changestamp,
                                      callback))));
  load_params->start_changestamp = start_changestamp;
  LoadFromServer(load_params.Pass());
}

void DriveFeedLoader::OnGetApplicationList(google_apis::GDataErrorCode status,
                                           scoped_ptr<base::Value> json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK)
    return;

  if (json.get()) {
    scoped_ptr<google_apis::AppList> applist(
        google_apis::AppList::CreateFrom(*json));
    if (applist.get())
      webapps_registry_->UpdateFromApplicationList(*applist.get());
  }
}

void DriveFeedLoader::LoadFromServer(scoped_ptr<LoadFeedParams> params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // base::Passed() may get evaluated first, so get a pointer to params.
  LoadFeedParams* params_ptr = params.get();
  scheduler_->GetResourceList(
      params_ptr->feed_to_load,
      params_ptr->start_changestamp,
      params_ptr->search_query,
      params_ptr->shared_with_me,
      params_ptr->directory_resource_id,
      base::Bind(&DriveFeedLoader::OnGetResourceList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 start_time));
}

void DriveFeedLoader::LoadDirectoryFromServer(
    const std::string& directory_resource_id,
    const LoadFeedListCallback& feed_load_callback) {
  DCHECK(!feed_load_callback.is_null());

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(feed_load_callback));
  params->directory_resource_id = directory_resource_id;
  LoadFromServer(params.Pass());
}

void DriveFeedLoader::SearchFromServer(
    const std::string& search_query,
    bool shared_with_me,
    const GURL& next_feed,
    const LoadFeedListCallback& feed_load_callback) {
  DCHECK(!feed_load_callback.is_null());

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(feed_load_callback));
  params->search_query = search_query;
  params->shared_with_me = shared_with_me;
  params->feed_to_load = next_feed;
  params->load_subsequent_feeds = false;
  LoadFromServer(params.Pass());
}

void DriveFeedLoader::UpdateMetadataFromFeedAfterLoadFromServer(
    const UpdateMetadataParams& params,
    const ScopedVector<google_apis::ResourceList>& feed_list,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());
  DCHECK(refreshing_);

  if (error != DRIVE_FILE_OK) {
    refreshing_ = false;
    params.callback.Run(error);
    return;
  }

  UpdateFromFeed(feed_list,
                 params.is_delta_feed,
                 params.feed_changestamp,
                 base::Bind(&DriveFeedLoader::OnUpdateFromFeed,
                            weak_ptr_factory_.GetWeakPtr(),
                            params.callback));
}

void DriveFeedLoader::OnGetResourceList(
    scoped_ptr<LoadFeedParams> params,
    base::TimeTicks start_time,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params->feed_list.empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    params->RunFeedLoadCallback(error);
    return;
  }
  DCHECK(resource_list);

  GURL next_feed_url;
  const bool has_next_feed_url =
      params->load_subsequent_feeds &&
      resource_list->GetNextFeedURL(&next_feed_url);

  // Add the current feed to the list of collected feeds for this directory.
  params->feed_list.push_back(resource_list.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list.size(); ++i)
    num_accumulated_entries += params->feed_list[i]->entries().size();

  // Check if we need to collect more data to complete the directory list.
  if (has_next_feed_url && !next_feed_url.is_empty()) {
    // Post an UI update event to make the UI smoother.
    GetResourceListUiState* ui_state = params->ui_state.get();
    if (ui_state == NULL) {
      ui_state = new GetResourceListUiState(base::TimeTicks::Now());
      params->ui_state.reset(ui_state);
    }
    DCHECK(ui_state);

    if ((ui_state->num_fetched_documents - ui_state->num_showing_documents)
        < kFetchUiUpdateStep) {
      // Currently the UI update is stopped. Start UI periodic callback.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(&DriveFeedLoader::OnNotifyResourceListFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()));
    }
    ui_state->num_fetched_documents = num_accumulated_entries;
    ui_state->feed_fetching_elapsed_time = base::TimeTicks::Now() - start_time;

    // |params| will be passed to the callback and thus nulled. Extract the
    // pointer so we can use it bellow.
    LoadFeedParams* params_ptr = params.get();
    // Kick off the remaining part of the feeds.
    scheduler_->GetResourceList(
        next_feed_url,
        params_ptr->start_changestamp,
        params_ptr->search_query,
        params_ptr->shared_with_me,
        params_ptr->directory_resource_id,
        base::Bind(&DriveFeedLoader::OnGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(DriveFeedLoaderObserver, observers_,
                    OnResourceListFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Drive.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  params->RunFeedLoadCallback(DRIVE_FILE_OK);
}

void DriveFeedLoader::OnNotifyResourceListFetched(
    base::WeakPtr<GetResourceListUiState> ui_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ui_state) {
    // The ui state instance is already released, which means the fetching
    // is done and we don't need to update any more.
    return;
  }

  base::TimeDelta ui_elapsed_time =
      base::TimeTicks::Now() - ui_state->start_time;

  if (ui_state->num_showing_documents + kFetchUiUpdateStep <=
      ui_state->num_fetched_documents) {
    ui_state->num_showing_documents += kFetchUiUpdateStep;
    FOR_EACH_OBSERVER(DriveFeedLoaderObserver, observers_,
                      OnResourceListFetched(ui_state->num_showing_documents));

    int num_remaining_ui_updates =
        (ui_state->num_fetched_documents - ui_state->num_showing_documents)
        / kFetchUiUpdateStep;
    if (num_remaining_ui_updates > 0) {
      // Heuristically, we use fetched time duration to calculate the next
      // UI update timing.
      base::TimeDelta remaining_duration =
          ui_state->feed_fetching_elapsed_time - ui_elapsed_time;
      base::TimeDelta interval = remaining_duration / num_remaining_ui_updates;
      // If UI update is slow for some reason, the interval can be
      // negative, or very small. This rarely happens but should be handled.
      const int kMinIntervalMs = 10;
      if (interval.InMilliseconds() < kMinIntervalMs)
        interval = base::TimeDelta::FromMilliseconds(kMinIntervalMs);

      base::MessageLoopProxy::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&DriveFeedLoader::OnNotifyResourceListFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()),
          interval);
    }
  }
}

void DriveFeedLoader::LoadFromCache(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!resource_metadata_->loaded());

  // Sets the refreshing flag, so that the caller does not send refresh requests
  // in parallel (see DriveFileSystem::LoadFeedIfNeeded).
  //
  // Corresponding unset is in ContinueWithInitializedResourceMetadata, where
  // all the control paths reach.
  refreshing_ = true;

  LoadRootFeedParams* params = new LoadRootFeedParams(callback);
  FilePath path = cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META);
  if (UseLevelDB()) {
    path = path.Append(kResourceMetadataDBFile);
    resource_metadata_->InitFromDB(path, blocking_task_runner_,
        base::Bind(
            &DriveFeedLoader::ContinueWithInitializedResourceMetadata,
            weak_ptr_factory_.GetWeakPtr(),
            base::Owned(params)));
  } else {
    path = path.Append(kFilesystemProtoFile);
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&LoadProtoOnBlockingPool,
                   path, &params->last_modified, &params->proto),
        base::Bind(&DriveFeedLoader::OnProtoLoaded,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(params)));
  }
}

void DriveFeedLoader::OnProtoLoaded(LoadRootFeedParams* params,
                                    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(refreshing_);

  // Update directory structure only if everything is OK and we haven't yet
  // received the feed from the server yet.
  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "ParseFromString";
    if (resource_metadata_->ParseFromString(params->proto)) {
      resource_metadata_->set_last_serialized(params->last_modified);
      resource_metadata_->set_serialized_size(params->proto.size());
    } else {
      error = DRIVE_FILE_ERROR_FAILED;
      LOG(WARNING) << "Parse of cached proto file failed";
    }
  }

  ContinueWithInitializedResourceMetadata(params, error);
}

void DriveFeedLoader::ContinueWithInitializedResourceMetadata(
    LoadRootFeedParams* params,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());
  refreshing_ = false;

  DVLOG(1) << "Time elapsed to load resource metadata from disk="
           << (base::Time::Now() - params->load_start_time).InMilliseconds()
           << " milliseconds";

  params->callback.Run(error);
}

void DriveFeedLoader::SaveFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ShouldSerializeFileSystemNow(resource_metadata_->serialized_size(),
                                    resource_metadata_->last_serialized())) {
    return;
  }

  if (UseLevelDB()) {
    resource_metadata_->SaveToDB();
  } else {
    const FilePath path =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META).Append(
            kFilesystemProtoFile);
    scoped_ptr<std::string> serialized_proto(new std::string());
    resource_metadata_->SerializeToString(serialized_proto.get());
    resource_metadata_->set_last_serialized(base::Time::Now());
    resource_metadata_->set_serialized_size(serialized_proto->size());
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SaveProtoOnBlockingPool, path,
                   base::Passed(serialized_proto.Pass())));
  }
}

void DriveFeedLoader::UpdateFromFeed(
    const ScopedVector<google_apis::ResourceList>& feed_list,
    bool is_delta_feed,
    int64 root_feed_changestamp,
    const base::Closure& update_finished_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!update_finished_callback.is_null());
  DVLOG(1) << "Updating directory with a feed";

  feed_processor_.reset(new DriveFeedProcessor(resource_metadata_));
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_feed;

  feed_processor_->ApplyFeeds(
      feed_list,
      is_delta_feed,
      root_feed_changestamp,
      base::Bind(&DriveFeedLoader::NotifyDirectoryChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 should_notify_changed_directories,
                 update_finished_callback));
}

void DriveFeedLoader::NotifyDirectoryChanged(
    bool should_notify_changed_directories,
    const base::Closure& update_finished_callback) {
  DCHECK(feed_processor_.get());
  DCHECK(!update_finished_callback.is_null());

  if (should_notify_changed_directories) {
    for (std::set<FilePath>::iterator dir_iter =
            feed_processor_->changed_dirs().begin();
        dir_iter != feed_processor_->changed_dirs().end();
        ++dir_iter) {
      FOR_EACH_OBSERVER(DriveFeedLoaderObserver, observers_,
                        OnDirectoryChanged(*dir_iter));
    }
  }

  update_finished_callback.Run();

  // Cannot delete feed_processor_ yet because we are in on_complete_callback_,
  // which is owned by feed_processor_.
}

void DriveFeedLoader::OnUpdateFromFeed(
    const FileOperationCallback& load_finished_callback) {
  DCHECK(!load_finished_callback.is_null());

  refreshing_ = false;

  // Save file system metadata to disk.
  SaveFileSystem();

  // Run the callback now that the filesystem is ready.
  load_finished_callback.Run(DRIVE_FILE_OK);

  FOR_EACH_OBSERVER(DriveFeedLoaderObserver,
                    observers_,
                    OnFeedFromServerLoaded());
}

}  // namespace drive
