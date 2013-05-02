// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include <set>

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace drive {

ChangeListLoader::ChangeListLoader(
    internal::ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    DriveWebAppsRegistry* webapps_registry)
    : resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      webapps_registry_(webapps_registry),
      last_known_remote_changestamp_(0),
      loaded_(false),
      weak_ptr_factory_(this) {
}

ChangeListLoader::~ChangeListLoader() {
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

  if (loaded_ && !IsRefreshing()) {
    util::Log("Checking for updates");
    Load(DirectoryFetchInfo(), callback);
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
  if (loaded_ && (directory_fetch_info.empty() || !IsRefreshing())) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, FILE_ERROR_OK));
    return;
  }
  Load(directory_fetch_info, callback);
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

void ChangeListLoader::SearchFromServer(
    const std::string& search_query,
    const GURL& next_feed,
    const LoadFeedListCallback& callback) {
  DCHECK(!callback.is_null());

  if (next_feed.is_empty()) {
    // This is first request for the |search_query|.
    scheduler_->Search(
        search_query,
        base::Bind(&ChangeListLoader::SearchFromServerAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    // There is the remaining result so fetch it.
    scheduler_->ContinueGetResourceList(
        next_feed,
        base::Bind(&ChangeListLoader::SearchFromServerAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }
}

void ChangeListLoader::UpdateFromFeed(
    scoped_ptr<google_apis::AboutResource> about_resource,
    ScopedVector<ChangeList> change_lists,
    bool is_delta_feed,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  change_list_processor_.reset(new ChangeListProcessor(resource_metadata_));
  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_changed_directories = is_delta_feed;

  util::Log("Apply change lists (is delta: %d)", is_delta_feed);
  change_list_processor_->ApplyFeeds(
      about_resource.Pass(),
      change_lists.Pass(),
      is_delta_feed,
      base::Bind(&ChangeListLoader::NotifyDirectoryChangedAfterApplyFeed,
                 weak_ptr_factory_.GetWeakPtr(),
                 should_notify_changed_directories,
                 base::Time::Now(),
                 callback));
}

void ChangeListLoader::Load(const DirectoryFetchInfo& directory_fetch_info,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Check if this is the first time this ChangeListLoader do loading.
  // Note: IsRefreshing() depends on pending_load_callback_ so check in advance.
  const bool is_initial_load = (!loaded_ && !IsRefreshing());

  // Register the callback function to be called when it is loaded.
  const std::string& resource_id = directory_fetch_info.resource_id();
  pending_load_callback_[resource_id].push_back(callback);

  // If loading task for |resource_id| is already running, do nothing.
  if (pending_load_callback_[resource_id].size() > 1)
    return;

  // For initial loading, even for directory fetching, we do load the full
  // feed from the server to sync up. So we register a dummy callback to
  // indicate that update for full hierarchy is running.
  if (is_initial_load && !resource_id.empty()) {
    pending_load_callback_[""].push_back(
        base::Bind(&util::EmptyFileOperationCallback));
  }

  // Check the current status of local metadata, and start loading if needed.
  resource_metadata_->GetLargestChangestamp(
      base::Bind(is_initial_load ? &ChangeListLoader::DoInitialLoad
                                 : &ChangeListLoader::DoUpdateLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info));
}

void ChangeListLoader::DoInitialLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (local_changestamp > 0) {
    // The local data is usable. Flush callbacks to tell loading was successful.
    OnChangeListLoadComplete(FILE_ERROR_OK);

    // Continues to load from server in background.
    // Put dummy callbacks to indicate that fetching is still continuing.
    pending_load_callback_[directory_fetch_info.resource_id()].push_back(
        base::Bind(&util::EmptyFileOperationCallback));
    if (!directory_fetch_info.empty()) {
      pending_load_callback_[""].push_back(
          base::Bind(&util::EmptyFileOperationCallback));
    }
  }
  LoadFromServerIfNeeded(directory_fetch_info, local_changestamp);
}

void ChangeListLoader::DoUpdateLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (directory_fetch_info.empty()) {
    LoadFromServerIfNeeded(directory_fetch_info, local_changestamp);
  } else {
    // Note: CheckChangestampAndLoadDirectoryIfNeeded regards
    // last_know_remote_changestamp_ as the remote changestamp. To be precise,
    // we need to call GetAboutResource() here, as we do in other places like
    // LoadFromServerIfNeeded or LoadFromDirectory. However,
    // - It is costly to do GetAboutResource HTTP request every time.
    // - The chance using an old value is small; it only happens when
    //   LoadIfNeeded is called during one GetAboutResource roundtrip time
    //   of a feed fetching.
    // - Even if the value is old, it just marks the directory as older. It may
    //   trigger one future unnecessary re-fetch, but it'll never lose data.
    CheckChangestampAndLoadDirectoryIfNeeed(
        directory_fetch_info,
        local_changestamp,
        base::Bind(&ChangeListLoader::OnDirectoryLoadComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_fetch_info));
  }
}

void ChangeListLoader::OnChangeListLoadComplete(FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!loaded_ && error == FILE_ERROR_OK) {
    loaded_ = true;
    FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                      observers_,
                      OnInitialFeedLoaded());
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
}

void ChangeListLoader::OnDirectoryLoadComplete(
    const DirectoryFetchInfo& directory_fetch_info,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  util::Log("Fast-fetch complete: %s => %s",
            directory_fetch_info.ToString().c_str(),
            FileErrorToString(error).c_str());
  const std::string& resource_id = directory_fetch_info.resource_id();
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

void ChangeListLoader::LoadFromServerIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
                 local_changestamp));
}

void ChangeListLoader::LoadFromServerIfNeededAfterGetAbout(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(util::GDataToFileError(status) == FILE_ERROR_OK,
            about_resource.get() != NULL);

  if (util::GDataToFileError(status) == FILE_ERROR_OK) {
    DCHECK(about_resource);
    last_known_remote_changestamp_ = about_resource->largest_change_id();
  }

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
    OnChangeListLoadComplete(FILE_ERROR_OK);
    return;
  }

  int64 start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  if (start_changestamp == 0 && !about_resource.get()) {
    // Full update needs AboutResource. If this is a full update, we should just
    // give up. Note that to exit from the feed loading, we always have to flush
    // the pending callback tasks via OnChangeListLoadComplete.
    OnChangeListLoadComplete(FILE_ERROR_FAILED);
    return;
  }

  if (directory_fetch_info.empty()) {
    // If the caller is not interested in a particular directory, just start
    // loading the change list.
    LoadChangeListFromServer(about_resource.Pass(), start_changestamp);
  } else {
    // If the caller is interested in a particular directory, start loading the
    // directory first.
    CheckChangestampAndLoadDirectoryIfNeeed(
        directory_fetch_info,
        local_changestamp,
        base::Bind(
            &ChangeListLoader::LoadChangeListFromServerAfterLoadDirectory,
            weak_ptr_factory_.GetWeakPtr(),
            directory_fetch_info,
            base::Passed(&about_resource),
            start_changestamp));
  }
}

void ChangeListLoader::LoadChangeListFromServerAfterLoadDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 start_changestamp,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == FILE_ERROR_OK) {
    // The directory fast-fetch succeeded. Runs the callbacks waiting for the
    // directory loading. If failed, do not flush so they're run after the
    // change list loading is complete.
    OnDirectoryLoadComplete(directory_fetch_info, FILE_ERROR_OK);
  }
  LoadChangeListFromServer(about_resource.Pass(), start_changestamp);
}

void ChangeListLoader::LoadChangeListFromServer(
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 start_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool is_delta_feed = start_changestamp != 0;
  const LoadFeedListCallback& completion_callback =
      base::Bind(&ChangeListLoader::UpdateMetadataFromFeedAfterLoadFromServer,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&about_resource),
                 is_delta_feed);
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (is_delta_feed) {
    scheduler_->GetChangeList(
        start_changestamp,
        base::Bind(&ChangeListLoader::OnGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(ScopedVector<ChangeList>()),
                   completion_callback,
                   start_time));
  } else {
    // This is full feed fetch.
    scheduler_->GetAllResourceList(
        base::Bind(&ChangeListLoader::OnGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(ScopedVector<ChangeList>()),
                   completion_callback,
                   start_time));
  }
}

void ChangeListLoader::OnGetResourceList(
    ScopedVector<ChangeList> change_lists,
    const LoadFeedListCallback& callback,
    base::TimeTicks start_time,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Looks the UMA stats we take here is useless as many methods use this
  // callback. crbug.com/229407
  if (change_lists.empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(ScopedVector<ChangeList>(), error);
    return;
  }

  // Add the current resource list to the list of collected feeds.
  DCHECK(resource_list);
  change_lists.push_back(new ChangeList(*resource_list));

  GURL next_feed_url;
  if (resource_list->GetNextFeedURL(&next_feed_url) &&
      !next_feed_url.is_empty()) {
    // There is the remaining result so fetch it.
    scheduler_->ContinueGetResourceList(
        next_feed_url,
        base::Bind(&ChangeListLoader::OnGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&change_lists),
                   callback,
                   start_time));
    return;
  }

  // This UMA stats looks also different from what we want. crbug.com/229407
  UMA_HISTOGRAM_TIMES("Drive.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  callback.Run(change_lists.Pass(), FILE_ERROR_OK);
}

void ChangeListLoader::UpdateMetadataFromFeedAfterLoadFromServer(
    scoped_ptr<google_apis::AboutResource> about_resource,
    bool is_delta_feed,
    ScopedVector<ChangeList> change_lists,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    OnChangeListLoadComplete(error);
    return;
  }

  UpdateFromFeed(about_resource.Pass(),
                 change_lists.Pass(),
                 is_delta_feed,
                 base::Bind(&ChangeListLoader::OnUpdateFromFeed,
                            weak_ptr_factory_.GetWeakPtr()));
}

void ChangeListLoader::OnUpdateFromFeed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OnChangeListLoadComplete(FILE_ERROR_OK);

  FOR_EACH_OBSERVER(ChangeListLoaderObserver,
                    observers_,
                    OnFeedFromServerLoaded());
}

void ChangeListLoader::LoadDirectoryFromServerAfterGetAbout(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (util::GDataToFileError(status) == FILE_ERROR_OK) {
    DCHECK(about_resource);
    last_known_remote_changestamp_ = about_resource->largest_change_id();
  }

  const DirectoryFetchInfo directory_fetch_info(
      directory_resource_id,
      last_known_remote_changestamp_);
  DoLoadDirectoryFromServer(directory_fetch_info, callback);
}

void ChangeListLoader::CheckChangestampAndLoadDirectoryIfNeeed(
    const DirectoryFetchInfo& directory_fetch_info,
    int64 local_changestamp,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_fetch_info.empty());

  int64 directory_changestamp = std::max(directory_fetch_info.changestamp(),
                                         local_changestamp);

  // We may not fetch from the server at all if the local metadata is new
  // enough, but we log this message here, so "Fast-fetch start" and
  // "Fast-fetch complete" always match.
  // TODO(satorux): Distinguish the "not fetching at all" case.
  util::Log("Fast-fetch start: %s; Server changestamp: %s",
            directory_fetch_info.ToString().c_str(),
            base::Int64ToString(last_known_remote_changestamp_).c_str());

  // If the directory's changestamp is up-to-date, just schedule to run the
  // callback, as there is no need to fetch the directory.
  // Note that |last_known_remote_changestamp_| is 0 when it is not received
  // yet. In that case we conservatively assume that we need to fetch.
  if (last_known_remote_changestamp_ > 0 &&
      directory_changestamp >= last_known_remote_changestamp_) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // Start fetching the directory content, and mark it with the changestamp
  // |last_known_remote_changestamp_|.
  DirectoryFetchInfo new_directory_fetch_info(
      directory_fetch_info.resource_id(),
      std::max(directory_changestamp, last_known_remote_changestamp_));
  DoLoadDirectoryFromServer(new_directory_fetch_info, callback);
}

void ChangeListLoader::DoLoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());
  DVLOG(1) << "Start loading directory: " << directory_fetch_info.ToString();

  if (directory_fetch_info.resource_id() ==
          util::kDriveOtherDirSpecialResourceId) {
    // Load for a <other> directory is meaningless in the server.
    // Let it succeed and use what we have locally.
    callback.Run(FILE_ERROR_OK);
    return;
  }

  if (directory_fetch_info.resource_id() ==
          util::kDriveGrandRootSpecialResourceId) {
    // Load for a grand root directory means slightly different from other
    // directories. It should have two directories; <other> and mydrive root.
    // <other> directory should always exist, but mydrive root should be
    // created by root resource id retrieved from the server.
    // Here, we check if mydrive root exists, and if not, create it.
    resource_metadata_->GetEntryInfoByPath(
        base::FilePath(util::kDriveMyDriveRootPath),
        base::Bind(
            &ChangeListLoader
                ::DoLoadGrandRootDirectoryFromServerAfterGetEntryInfoByPath,
            weak_ptr_factory_.GetWeakPtr(),
            directory_fetch_info,
            callback));
    return;
  }

  const LoadFeedListCallback& completion_callback =
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback);
  base::TimeTicks start_time = base::TimeTicks::Now();
  scheduler_->GetResourceListInDirectory(
      directory_fetch_info.resource_id(),
      base::Bind(&ChangeListLoader::OnGetResourceList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(ScopedVector<ChangeList>()),
                 completion_callback,
                 start_time));
}

void
ChangeListLoader::DoLoadGrandRootDirectoryFromServerAfterGetEntryInfoByPath(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK_EQ(directory_fetch_info.resource_id(),
            util::kDriveGrandRootSpecialResourceId);

  if (error == FILE_ERROR_OK) {
    // MyDrive root already exists. Just return success.
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // Fetch root resource id from the server.
  scheduler_->GetAboutResource(
      base::Bind(
          &ChangeListLoader
              ::DoLoadGrandRootDirectoryFromServerAfterGetAboutResource,
          weak_ptr_factory_.GetWeakPtr(),
          directory_fetch_info,
          callback));
}

void ChangeListLoader::DoLoadGrandRootDirectoryFromServerAfterGetAboutResource(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK_EQ(directory_fetch_info.resource_id(),
            util::kDriveGrandRootSpecialResourceId);

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Build entry proto map for grand root directory, which has two entries;
  // "/drive/root" and "/drive/other".
  ResourceEntryMap grand_root_entry_map;
  const std::string& root_resource_id = about_resource->root_folder_id();
  grand_root_entry_map[root_resource_id] =
      util::CreateMyDriveRootEntry(root_resource_id);
  grand_root_entry_map[util::kDriveOtherDirSpecialResourceId] =
      util::CreateOtherDirEntry();
  resource_metadata_->RefreshDirectory(
      directory_fetch_info,
      grand_root_entry_map,
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::DoLoadDirectoryFromServerAfterLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    ScopedVector<ChangeList> change_lists,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!directory_fetch_info.empty());

  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.resource_id()
               << ": " << FileErrorToString(error);
    callback.Run(error);
    return;
  }

  // Do not use |change_list_processor_| as it may be in use for other
  // purposes.
  ChangeListProcessor change_list_processor(resource_metadata_);
  change_list_processor.FeedToEntryProtoMap(change_lists.Pass(), NULL, NULL);
  resource_metadata_->RefreshDirectory(
      directory_fetch_info,
      change_list_processor.entry_map(),
      base::Bind(&ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 callback));
}

void ChangeListLoader::DoLoadDirectoryFromServerAfterRefresh(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
  callback.Run(error);
  // Also notify the observers.
  if (error == FILE_ERROR_OK) {
    FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                      OnDirectoryChanged(directory_path));
  }
}

void ChangeListLoader::OnGetAppList(google_apis::GDataErrorCode status,
                                    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK)
    return;

  if (app_list.get())
    webapps_registry_->UpdateFromAppList(*app_list);
}

void ChangeListLoader::SearchFromServerAfterGetResourceList(
    const LoadFeedListCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(ScopedVector<ChangeList>(), error);
    return;
  }

  DCHECK(resource_list);

  ScopedVector<ChangeList> change_lists;
  change_lists.push_back(new ChangeList(*resource_list));
  callback.Run(change_lists.Pass(), FILE_ERROR_OK);
}

void ChangeListLoader::NotifyDirectoryChangedAfterApplyFeed(
    bool should_notify_changed_directories,
    base::Time start_time,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(change_list_processor_.get());
  DCHECK(!callback.is_null());

  const base::TimeDelta elapsed = base::Time::Now() - start_time;
  util::Log("Change lists applied (elapsed time: %sms)",
            base::Int64ToString(elapsed.InMilliseconds()).c_str());

  if (should_notify_changed_directories) {
    for (std::set<base::FilePath>::iterator dir_iter =
            change_list_processor_->changed_dirs().begin();
        dir_iter != change_list_processor_->changed_dirs().end();
        ++dir_iter) {
      FOR_EACH_OBSERVER(ChangeListLoaderObserver, observers_,
                        OnDirectoryChanged(*dir_iter));
    }
  }

  callback.Run();

  // Cannot delete change_list_processor_ yet because we are in
  // on_complete_callback_, which is owned by change_list_processor_.
}

}  // namespace drive
