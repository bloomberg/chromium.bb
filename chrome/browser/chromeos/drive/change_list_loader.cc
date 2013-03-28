// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include <set>

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace drive {

namespace {

// Update the fetch progress UI per every this number of feeds.
const int kFetchUiUpdateStep = 10;

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
  ScopedVector<ChangeList> change_lists;
  scoped_ptr<GetResourceListUiState> ui_state;
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

ChangeListLoader::ChangeListLoader(DriveResourceMetadata* resource_metadata,
                                   DriveScheduler* scheduler,
                                   DriveWebAppsRegistry* webapps_registry)
    : resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      webapps_registry_(webapps_registry),
      refreshing_(false),
      last_known_remote_changestamp_(0),
      loaded_(false),
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
  DCHECK_EQ(util::GDataToDriveFileError(status) == DRIVE_FILE_OK,
            about_resource.get() != NULL);

  if (util::GDataToDriveFileError(status) == DRIVE_FILE_OK) {
    DCHECK(about_resource);
    last_known_remote_changestamp_ = about_resource->largest_change_id();
  }

  resource_metadata_->GetLargestChangestamp(
      base::Bind(&ChangeListLoader::CompareChangestampsAndLoadIfNeeded,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback,
                 base::Passed(&about_resource)));
}

void ChangeListLoader::CompareChangestampsAndLoadIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  int64 remote_changestamp =
      about_resource ? about_resource->largest_change_id() : 0;
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
  if (start_changestamp == 0 && !about_resource.get()) {
    // Full update needs AboutResource. If this is a full update, we should just
    // give up. Note that to exit from the feed loading, we always have to flush
    // the pending callback tasks via OnChangeListLoadComplete.
    OnChangeListLoadComplete(callback, DRIVE_FILE_ERROR_FAILED);
    return;
  }

  if (directory_fetch_info.empty()) {
    // If the caller is not interested in a particular directory, just start
    // loading the change list.
    LoadChangeListFromServer(about_resource.Pass(),
                             start_changestamp,
                             callback);
  } else if (directory_fetch_info.changestamp() < remote_changestamp) {
    // If the caller is interested in a particular directory, and the
    // directory changestamp is older than server's, start loading the
    // directory first.
    DVLOG(1) << "Fast-fetching directory: " << directory_fetch_info.ToString()
             << "; remote_changestamp: " << remote_changestamp;
    const DirectoryFetchInfo new_directory_fetch_info(
        directory_fetch_info.resource_id(), remote_changestamp);
    DoLoadDirectoryFromServer(
        new_directory_fetch_info,
        base::Bind(&ChangeListLoader::StartLoadChangeListFromServer,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info,
                   base::Passed(&about_resource),
                   start_changestamp,
                   callback));
  } else {
    // The directory is up-to-date, but not the case for other parts.
    // Proceed to change list loading. StartLoadChangeListFromServer will
    // run |callback| for notifying the directory is ready before feed load.
    StartLoadChangeListFromServer(directory_fetch_info,
                                  about_resource.Pass(),
                                  start_changestamp,
                                  callback,
                                  DRIVE_FILE_OK);
  }
}

void ChangeListLoader::LoadChangeListFromServer(
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 start_changestamp,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  scoped_ptr<LoadFeedParams> load_params(new LoadFeedParams);
  load_params->start_changestamp = start_changestamp;
  LoadFromServer(
      load_params.Pass(),
      base::Bind(&ChangeListLoader::UpdateMetadataFromFeedAfterLoadFromServer,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&about_resource),
                 start_changestamp != 0,  // is_delta_feed
                 callback));
}

void ChangeListLoader::StartLoadChangeListFromServer(
    const DirectoryFetchInfo& directory_fetch_info,
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 start_changestamp,
    const FileOperationCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  if (error == DRIVE_FILE_OK) {
    OnDirectoryLoadComplete(directory_fetch_info, callback, DRIVE_FILE_OK);
    DVLOG(1) << "Fast-fetch was successful: " << directory_fetch_info.ToString()
             << "; Start loading the change list";
    // Stop passing |callback| as it's just consumed.
    LoadChangeListFromServer(
        about_resource.Pass(),
        start_changestamp,
        base::Bind(&util::EmptyFileOperationCallback));
  } else {
    // The directory fast-fetch failed, but the change list loading may
    // succeed. Keep passing |callback| so it's run after the change list
    // loading is complete.
    LoadChangeListFromServer(
        about_resource.Pass(), start_changestamp, callback);
  }
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
  DVLOG(1) << "Start loading directory: " << directory_fetch_info.ToString();

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
    ScopedVector<ChangeList> change_lists,
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
  change_list_processor.FeedToEntryProtoMap(change_lists.Pass(), NULL, NULL);
  resource_metadata_->RefreshDirectory(
      directory_fetch_info,
      change_list_processor.entry_proto_map(),
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    DriveFileError error,
    const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
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
    scoped_ptr<google_apis::AboutResource> about_resource,
    bool is_delta_feed,
    const FileOperationCallback& callback,
    ScopedVector<ChangeList> change_lists,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  if (error != DRIVE_FILE_OK) {
    OnChangeListLoadComplete(callback, error);
    return;
  }

  UpdateFromFeed(about_resource.Pass(),
                 change_lists.Pass(),
                 is_delta_feed,
                 base::Bind(&ChangeListLoader::OnUpdateFromFeed,
                            weak_ptr_factory_.GetWeakPtr(),
                            !loaded(),  // is_initial_load
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

  if (params->change_lists.empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(params->change_lists.Pass(), error);
    return;
  }
  DCHECK(resource_list);

  GURL next_feed_url;
  const bool has_next_feed_url =
      params->load_subsequent_feeds &&
      resource_list->GetNextFeedURL(&next_feed_url);

  // Add the current feed to the list of collected feeds for this directory.
  params->change_lists.push_back(new ChangeList(*resource_list));

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->change_lists.size(); ++i)
    num_accumulated_entries += params->change_lists[i]->entries().size();

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
  callback.Run(params->change_lists.Pass(), DRIVE_FILE_OK);
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

void ChangeListLoader::LoadIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // If feed has already been loaded, for normal feed fetch (= empty
  // directory_fetch_info), we have nothing to do. For "fast fetch", we need to
  // schedule a fetching if a feed refresh is currently running, because we
  // don't want to wait a possibly large delta feed to arrive.
  if (loaded() && (directory_fetch_info.empty() || !refreshing())) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DRIVE_FILE_OK));
    return;
  }

  // At this point, it is either !loaded() or refreshing().
  // If the change list loading is in progress, schedule the callback to
  // run when it's ready (i.e. when the entire resource list is loaded, or
  // the directory contents are available per "fast fetch").
  if (refreshing()) {
    ScheduleRun(directory_fetch_info, callback);
    return;
  }

  if (!directory_fetch_info.empty()) {
    // Add a dummy task to so ScheduleRun() can check that the directory is
    // being fetched. This will be cleared either in
    // ProcessPendingLoadCallbackForDirectory() or FlushPendingLoadCallback().
    pending_load_callback_[directory_fetch_info.resource_id()].push_back(
        base::Bind(&util::EmptyFileOperationCallback));
  }

  // First start loading from the cache.
  LoadFromCache(base::Bind(&ChangeListLoader::LoadAfterLoadFromCache,
                           weak_ptr_factory_.GetWeakPtr(),
                           directory_fetch_info,
                           callback));
}

void ChangeListLoader::LoadAfterLoadFromCache(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == DRIVE_FILE_OK) {
    loaded_ = true;

    // The loading from the cache file succeeded. Change the refreshing state
    // and tell the callback that the loading was successful.
    OnChangeListLoadComplete(callback, DRIVE_FILE_OK);
    FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                      observers_,
                      OnInitialFeedLoaded());

    // Load from server if needed (i.e. the cache is old). Note that we
    // should still propagate |directory_fetch_info| though the directory is
    // loaded first. This way, the UI can get notified via a directory change
    // event as soon as the current directory contents are fetched.
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
  DCHECK(!loaded_);

  // Sets the refreshing flag, so that the caller does not send refresh requests
  // in parallel (see DriveFileSystem::LoadFeedIfNeeded).
  //
  // The flag will be unset when loading from the cache is complete, or
  // loading from the server is complete.
  refreshing_ = true;

  resource_metadata_->Load(callback);
}

void ChangeListLoader::SaveFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  resource_metadata_->MaybeSave();
}

void ChangeListLoader::UpdateFromFeed(
    scoped_ptr<google_apis::AboutResource> about_resource,
    ScopedVector<ChangeList> change_lists,
    bool is_delta_feed,
    const base::Closure& update_finished_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!update_finished_callback.is_null());
  DVLOG(1) << "Updating directory with a feed";

  change_list_processor_.reset(new ChangeListProcessor(resource_metadata_));
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_feed;

  change_list_processor_->ApplyFeeds(
      about_resource.Pass(),
      change_lists.Pass(),
      is_delta_feed,
      base::Bind(&ChangeListLoader::NotifyDirectoryChangedAfterApplyFeed,
                 weak_ptr_factory_.GetWeakPtr(),
                 should_notify_changed_directories,
                 update_finished_callback));
}

void ChangeListLoader::ScheduleRun(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(refreshing_);

  if (directory_fetch_info.empty()) {
    // If the caller is not interested in a particular directory, just add the
    // callback to the pending list and return.
    pending_load_callback_[""].push_back(callback);
    return;
  }

  const std::string& resource_id = directory_fetch_info.resource_id();

  // If the directory of interest is already scheduled to be fetched, add the
  // callback to the pending list and return.
  LoadCallbackMap::iterator it = pending_load_callback_.find(resource_id);
  if (it != pending_load_callback_.end()) {
    it->second.push_back(callback);
    return;
  }

  // If the directory's changestamp is up-to-date, just schedule to run the
  // callback, as there is no need to fetch the directory.
  if (directory_fetch_info.changestamp() >= last_known_remote_changestamp_) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DRIVE_FILE_OK));
    return;
  }

  // The directory should be fetched. Add a dummy task to so ScheduleRun()
  // can check that the directory is being fetched.
  pending_load_callback_[resource_id].push_back(
      base::Bind(&util::EmptyFileOperationCallback));
  DoLoadDirectoryFromServer(
      directory_fetch_info,
      base::Bind(&ChangeListLoader::OnDirectoryLoadComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::NotifyDirectoryChangedAfterApplyFeed(
    bool should_notify_changed_directories,
    const base::Closure& update_finished_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(change_list_processor_.get());
  DCHECK(!update_finished_callback.is_null());

  loaded_ = true;

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
    bool is_inital_load,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  OnChangeListLoadComplete(callback, DRIVE_FILE_OK);
  if (is_inital_load) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                      observers_,
                      OnInitialFeedLoaded());
  }

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
  FlushPendingLoadCallback(error);
}

void ChangeListLoader::OnDirectoryLoadComplete(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  callback.Run(error);
  ProcessPendingLoadCallbackForDirectory(directory_fetch_info.resource_id(),
                                         error);
}

void ChangeListLoader::FlushPendingLoadCallback(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!refreshing_);

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
}

void ChangeListLoader::ProcessPendingLoadCallbackForDirectory(
    const std::string& resource_id,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LoadCallbackMap::iterator it = pending_load_callback_.find(resource_id);
  if (it != pending_load_callback_.end()) {
    DVLOG(1) << "Running callback for " << resource_id;
    const std::vector<FileOperationCallback>& callbacks = it->second;
    for (size_t i = 0; i < callbacks.size(); ++i) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(callbacks[i], error));
    }
    pending_load_callback_.erase(it);
  }
}

}  // namespace drive
