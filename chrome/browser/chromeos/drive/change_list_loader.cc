// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
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

const base::FilePath::CharType kFilesystemProtoFile[] =
    FILE_PATH_LITERAL("file_system.pb");

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
DriveFileError LoadProtoOnBlockingPool(const base::FilePath& path,
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
void SaveProtoOnBlockingPool(const base::FilePath& path,
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
struct ChangeListLoader::LoadFeedParams {
  LoadFeedParams()
      : start_changestamp(0),
        shared_with_me(false),
        load_subsequent_feeds(true) {}

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
  ScopedVector<google_apis::ResourceList> feed_list;
  scoped_ptr<GetResourceListUiState> ui_state;
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct ChangeListLoader::LoadRootFeedParams {
  explicit LoadRootFeedParams(const FileOperationCallback& callback)
      : load_start_time(base::Time::Now()),
        callback(callback) {}

  std::string proto;
  base::Time last_modified;
  // Time when filesystem began to be loaded from disk.
  base::Time load_start_time;
  const FileOperationCallback callback;
};

// Defines set of parameters sent to callback OnNotifyResourceListFetched().
// This is a trick to update the number of fetched documents frequently on
// UI. Due to performance reason, we need to fetch a number of files at
// a time. However, it'll take long time, and a user has no way to know
// the current update state. In order to make users comfortable,
// we increment the number of fetched documents with more frequent but smaller
// steps than actual fetching.
struct ChangeListLoader::GetResourceListUiState {
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

ChangeListLoader::ChangeListLoader(
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

ChangeListLoader::~ChangeListLoader() {
}

void ChangeListLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void ChangeListLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void ChangeListLoader::LoadFromServerIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Sets the refreshing flag, so that the caller does not send refresh requests
  // in parallel (see DriveFileSystem::CheckForUpdates). Corresponding
  // "refresh_ = false" is in OnGetAboutResource when the cached feed is up to
  // date, or in OnFeedFromServerLoaded called back from LoadFromServer().
  refreshing_ = true;

  // Drive v2 needs a separate application list fetch operation.
  // On GData WAPI, it is not necessary in theory, because the response
  // of account metadata can include both about account information (such as
  // quota) and an application list at once.
  // However, for Drive API v2 migration, we connect to the server twice
  // (one for about account information and another for an application list)
  // regardless of underlying API, so that we can simplify the code.
  // Note that the size of account metadata on GData WAPI seems small enough
  // and (by controlling the query parameter) the response for GetAboutResource
  // operation doesn't contain application list. Thus, the effect should be
  // small cost.
  // TODO(haruki): Application list rarely changes and is not necessarily
  // refreshed as often as files.
  scheduler_->GetAppList(
      base::Bind(&ChangeListLoader::OnGetAppList,
                 weak_ptr_factory_.GetWeakPtr()));

  // First fetch the latest changestamp to see if there were any new changes
  // there at all.
  scheduler_->GetAboutResource(
      base::Bind(&ChangeListLoader::LoadFromServerIfNeededAfterGetAbout,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::LoadFromServerIfNeededAfterGetAbout(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  int64 remote_changestamp = 0;
  if (util::GDataToDriveFileError(status) == DRIVE_FILE_OK) {
    DCHECK(about_resource);
    remote_changestamp = about_resource->largest_change_id();
  }

  resource_metadata_->GetLargestChangestamp(
      base::Bind(&ChangeListLoader::CompareChangestampsAndLoadIfNeeded,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback,
                 remote_changestamp));
}

void ChangeListLoader::CompareChangestampsAndLoadIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
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
    OnChangeListLoadComplete(callback, DRIVE_FILE_OK);
    return;
  }

  int64 start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;

  // TODO(satorux): Use directory_fetch_info to start "fast-fetch" here.
  LoadChangeListFromServer(start_changestamp,
                           remote_changestamp,
                           callback);
}

void ChangeListLoader::LoadChangeListFromServer(
    int64 start_changestamp,
    int64 remote_changestamp,
    const FileOperationCallback& callback) {
  scoped_ptr<LoadFeedParams> load_params(new LoadFeedParams);
  load_params->start_changestamp = start_changestamp;
  LoadFromServer(
      load_params.Pass(),
      base::Bind(&ChangeListLoader::UpdateMetadataFromFeedAfterLoadFromServer,
                 weak_ptr_factory_.GetWeakPtr(),
                 start_changestamp != 0,  // is_delta_feed
                 remote_changestamp,
                 callback));
}

void ChangeListLoader::OnGetAppList(google_apis::GDataErrorCode status,
                                   scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK)
    return;

  if (app_list.get()) {
    webapps_registry_->UpdateFromAppList(*app_list);
  }
}

void ChangeListLoader::LoadFromServer(scoped_ptr<LoadFeedParams> params,
                                      const LoadFeedListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // base::Passed() may get evaluated first, so get a pointer to params.
  LoadFeedParams* params_ptr = params.get();
  scheduler_->GetResourceList(
      params_ptr->feed_to_load,
      params_ptr->start_changestamp,
      params_ptr->search_query,
      params_ptr->shared_with_me,
      params_ptr->directory_resource_id,
      base::Bind(&ChangeListLoader::LoadFromServerAfterGetResourceList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 callback,
                 start_time));
}

void ChangeListLoader::LoadDirectoryFromServer(
    const std::string& directory_resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // First fetch the latest changestamp to see if this directory needs to be
  // updated.
  scheduler_->GetAboutResource(
      base::Bind(
          &ChangeListLoader::LoadDirectoryFromServerAfterGetAbout,
          weak_ptr_factory_.GetWeakPtr(),
          directory_resource_id,
          callback));
}

void ChangeListLoader::LoadDirectoryFromServerAfterGetAbout(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  int64 remote_changestamp = 0;
  if (util::GDataToDriveFileError(status) == DRIVE_FILE_OK) {
    DCHECK(about_resource);
    remote_changestamp = about_resource->largest_change_id();
  }

  const DirectoryFetchInfo directory_fetch_info(directory_resource_id,
                                                remote_changestamp);
  DoLoadDirectoryFromServer(directory_fetch_info, callback);
}

void ChangeListLoader::DoLoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams);
  params->directory_resource_id = directory_fetch_info.resource_id();
  LoadFromServer(
      params.Pass(),
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::DoLoadDirectoryFromServerAfterLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    const ScopedVector<google_apis::ResourceList>& resource_list,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  if (error != DRIVE_FILE_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.resource_id()
               << ": " << error;
    callback.Run(error);
    return;
  }

  // Do not use |change_list_processor_| as it may be in use for other
  // purposes.
  ChangeListProcessor change_list_processor(resource_metadata_);
  change_list_processor.FeedToEntryProtoMap(resource_list, NULL, NULL);
  resource_metadata_->RefreshDirectory(
      directory_fetch_info,
      change_list_processor.entry_proto_map(),
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh(
    const FileOperationCallback& callback,
    DriveFileError error,
    const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error);
  // Also notify the observers.
  if (error == DRIVE_FILE_OK) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                      OnDirectoryChanged(directory_path));
  }
}

void ChangeListLoader::SearchFromServer(
    const std::string& search_query,
    bool shared_with_me,
    const GURL& next_feed,
    const LoadFeedListCallback& feed_load_callback) {
  DCHECK(!feed_load_callback.is_null());

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams);
  params->search_query = search_query;
  params->shared_with_me = shared_with_me;
  params->feed_to_load = next_feed;
  params->load_subsequent_feeds = false;
  LoadFromServer(params.Pass(), feed_load_callback);
}

void ChangeListLoader::UpdateMetadataFromFeedAfterLoadFromServer(
    bool is_delta_feed,
    int64 feed_changestamp,
    const FileOperationCallback& callback,
    const ScopedVector<google_apis::ResourceList>& feed_list,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  if (error != DRIVE_FILE_OK) {
    OnChangeListLoadComplete(callback, error);
    return;
  }

  UpdateFromFeed(feed_list,
                 is_delta_feed,
                 feed_changestamp,
                 base::Bind(&ChangeListLoader::OnUpdateFromFeed,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback));
}

void ChangeListLoader::LoadFromServerAfterGetResourceList(
    scoped_ptr<LoadFeedParams> params,
    const LoadFeedListCallback& callback,
    base::TimeTicks start_time,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (params->feed_list.empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(params->feed_list, error);
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
          base::Bind(&ChangeListLoader::OnNotifyResourceListFetched,
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
        base::Bind(&ChangeListLoader::LoadFromServerAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   callback,
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                    OnResourceListFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Drive.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  callback.Run(params->feed_list, DRIVE_FILE_OK);
}

void ChangeListLoader::OnNotifyResourceListFetched(
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
    FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
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
          base::Bind(&ChangeListLoader::OnNotifyResourceListFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()),
          interval);
    }
  }
}

void ChangeListLoader::Load(const DirectoryFetchInfo directory_fetch_info,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // First start loading from the cache.
  LoadFromCache(base::Bind(&ChangeListLoader::LoadAfterLoadFromCache,
                           weak_ptr_factory_.GetWeakPtr(),
                           directory_fetch_info,
                           callback));
}

void ChangeListLoader::LoadAfterLoadFromCache(
    const DirectoryFetchInfo directory_fetch_info,
    const FileOperationCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == DRIVE_FILE_OK) {
    // The loading from the cache file succeeded. Change the refreshing state
    // and tell the callback that the loading was successful.
    OnChangeListLoadComplete(callback, DRIVE_FILE_OK);

    // Load from server if needed (i.e. the cache is old). Note that we
    // should still propagate |directory_fetch_info| though the directory is
    // loaded first. This way, the UI can get notified via a directory change
    // event as soons as the current directory contents are fetched.
    LoadFromServerIfNeeded(directory_fetch_info,
                           base::Bind(&util::EmptyFileOperationCallback));
  } else {
    // The loading from the cache file failed. Start loading from the
    // server. Though the function name ends with "IfNeeded", this function
    // should always start loading as the local changestamp is zero now.
    LoadFromServerIfNeeded(directory_fetch_info, callback);
  }
}

void ChangeListLoader::LoadFromCache(const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!resource_metadata_->loaded());

  // Sets the refreshing flag, so that the caller does not send refresh requests
  // in parallel (see DriveFileSystem::LoadFeedIfNeeded).
  //
  // The flag will be unset when loading from the cache is complete, or
  // loading from the server is complete.
  refreshing_ = true;

  LoadRootFeedParams* params = new LoadRootFeedParams(callback);
  base::FilePath path =
      cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META).Append(
          kFilesystemProtoFile);
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&LoadProtoOnBlockingPool,
                 path, &params->last_modified, &params->proto),
      base::Bind(&ChangeListLoader::LoadFromCacheAfterReadProto,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(params)));
}

void ChangeListLoader::LoadFromCacheAfterReadProto(LoadRootFeedParams* params,
                                                   DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());
  DCHECK(refreshing_);

  // Update directory structure only if everything is OK and we haven't yet
  // received the feed from the server yet.
  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "ParseFromString";
    if (resource_metadata_->ParseFromString(params->proto)) {
      resource_metadata_->set_last_serialized(params->last_modified);
      resource_metadata_->set_serialized_size(params->proto.size());
      base::TimeDelta elapsed = (base::Time::Now() - params->load_start_time);
      DVLOG(1) << "Time for loading from the cache: "
               << elapsed.InMilliseconds() << " milliseconds";
    } else {
      error = DRIVE_FILE_ERROR_FAILED;
      LOG(WARNING) << "Parse of cached proto file failed";
    }
  }

  params->callback.Run(error);
}

void ChangeListLoader::SaveFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ShouldSerializeFileSystemNow(resource_metadata_->serialized_size(),
                                    resource_metadata_->last_serialized())) {
    return;
  }

  const base::FilePath path =
      cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META).Append(
          kFilesystemProtoFile);
  scoped_ptr<std::string> serialized_proto(new std::string());
  resource_metadata_->SerializeToString(serialized_proto.get());
  resource_metadata_->set_last_serialized(base::Time::Now());
  resource_metadata_->set_serialized_size(serialized_proto->size());
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SaveProtoOnBlockingPool, path,
                 base::Passed(&serialized_proto)));
}

void ChangeListLoader::UpdateFromFeed(
    const ScopedVector<google_apis::ResourceList>& feed_list,
    bool is_delta_feed,
    int64 root_feed_changestamp,
    const base::Closure& update_finished_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!update_finished_callback.is_null());
  DVLOG(1) << "Updating directory with a feed";

  change_list_processor_.reset(new ChangeListProcessor(resource_metadata_));
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_feed;

  change_list_processor_->ApplyFeeds(
      feed_list,
      is_delta_feed,
      root_feed_changestamp,
      base::Bind(&ChangeListLoader::NotifyDirectoryChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 should_notify_changed_directories,
                 update_finished_callback));
}

void ChangeListLoader::ScheduleRun(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  queue_.push(std::make_pair(directory_fetch_info, callback));
}

void ChangeListLoader::NotifyDirectoryChanged(
    bool should_notify_changed_directories,
    const base::Closure& update_finished_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(change_list_processor_.get());
  DCHECK(!update_finished_callback.is_null());

  if (should_notify_changed_directories) {
    for (std::set<base::FilePath>::iterator dir_iter =
            change_list_processor_->changed_dirs().begin();
        dir_iter != change_list_processor_->changed_dirs().end();
        ++dir_iter) {
      FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                        OnDirectoryChanged(*dir_iter));
    }
  }

  update_finished_callback.Run();

  // Cannot delete change_list_processor_ yet because we are in
  // on_complete_callback_, which is owned by change_list_processor_.
}

void ChangeListLoader::OnUpdateFromFeed(
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  OnChangeListLoadComplete(callback, DRIVE_FILE_OK);

  // Save file system metadata to disk.
  SaveFileSystem();

  FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                    observers_,
                    OnFeedFromServerLoaded());
}

void ChangeListLoader::OnChangeListLoadComplete(
    const FileOperationCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  refreshing_ = false;
  callback.Run(error);

  FlushQueue(error);
}

void ChangeListLoader::FlushQueue(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!refreshing_);

  while (!queue_.empty()) {
    QueuedCallbackInfo info  = queue_.front();
    const FileOperationCallback& callback = info.second;
    queue_.pop();
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, error));
  }
}

}  // namespace drive
