// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_loader.h"

#include <set>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_processor.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

const FilePath::CharType kAccountMetadataFile[] =
    FILE_PATH_LITERAL("account_metadata.json");
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
void LoadProtoOnBlockingPool(const FilePath& path,
                             LoadRootFeedParams* params) {
  base::PlatformFileInfo info;
  if (!file_util::GetFileInfo(path, &info)) {
    params->load_error = DRIVE_FILE_ERROR_NOT_FOUND;
    return;
  }
  params->last_modified = info.last_modified;
  if (!file_util::ReadFileToString(path, &params->proto)) {
    LOG(WARNING) << "Proto file not found at " << path.value();
    params->load_error = DRIVE_FILE_ERROR_NOT_FOUND;
    return;
  }
  params->load_error = DRIVE_FILE_OK;
}

// Saves json file content content in |feed| to |file_pathname| on blocking
// pool. Used for debugging.
void SaveFeedOnBlockingPoolForDebugging(
    const FilePath& file_path,
    scoped_ptr<base::Value> feed) {
  std::string json;
  base::JSONWriter::WriteWithOptions(feed.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);

  int file_size = static_cast<int>(json.length());
  if (file_util::WriteFile(file_path, json.data(), file_size) != file_size) {
    LOG(WARNING) << "GData metadata file can't be stored at "
                 << file_path.value();
    if (!file_util::Delete(file_path, true)) {
      LOG(WARNING) << "GData metadata file can't be deleted at "
                   << file_path.value();
      return;
    }
  }
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
    LOG(WARNING) << "GData proto file can't be stored at "
                 << path.value();
    if (!file_util::Delete(path, true)) {
      LOG(WARNING) << "GData proto file can't be deleted at "
                   << path.value();
    }
  }
}

bool UseLevelDB() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseLevelDBForGData);
}

// Run params->feed_load_callback with |error|.
void RunFeedLoadCallback(scoped_ptr<LoadFeedParams> params,
                         DriveFileError error) {
  // Need a reference before calling Pass().
  const LoadDocumentFeedCallback& feed_load_callback =
      params->feed_load_callback;
  feed_load_callback.Run(params.Pass(), error);
}

}  // namespace

LoadFeedParams::LoadFeedParams(
    ContentOrigin initial_origin,
    const LoadDocumentFeedCallback& feed_load_callback)
    : initial_origin(initial_origin),
      start_changestamp(0),
      root_feed_changestamp(0),
      feed_load_callback(feed_load_callback) {
  DCHECK(!feed_load_callback.is_null());
}

LoadFeedParams::~LoadFeedParams() {
}

LoadRootFeedParams::LoadRootFeedParams(
    bool should_load_from_server,
    const FileOperationCallback& callback)
    : should_load_from_server(should_load_from_server),
      load_error(DRIVE_FILE_OK),
      load_start_time(base::Time::Now()),
      callback(callback) {
}

LoadRootFeedParams::~LoadRootFeedParams() {
}

// Defines set of parameters sent to callback OnNotifyDocumentFeedFetched().
// This is a trick to update the number of fetched documents frequently on
// UI. Due to performance reason, we need to fetch a number of files at
// a time. However, it'll take long time, and a user has no way to know
// the current update state. In order to make users confortable,
// we increment the number of fetched documents with more frequent but smaller
// steps than actual fetching.
struct GetDocumentsUiState {
  explicit GetDocumentsUiState(base::TimeTicks start_time)
      : num_fetched_documents(0),
        num_showing_documents(0),
        start_time(start_time),
        weak_ptr_factory(this) {
  }

  // The number of fetched documents.
  int num_fetched_documents;

  // The number documents shown on UI.
  int num_showing_documents;

  // When the UI update has started.
  base::TimeTicks start_time;

  // Time elapsed since the feed fetching was started.
  base::TimeDelta feed_fetching_elapsed_time;

  base::WeakPtrFactory<GetDocumentsUiState> weak_ptr_factory;
};

GDataWapiFeedLoader::GDataWapiFeedLoader(
    DriveResourceMetadata* resource_metadata,
    DriveServiceInterface* drive_service,
    DriveWebAppsRegistryInterface* webapps_registry,
    DriveCache* cache,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : resource_metadata_(resource_metadata),
      drive_service_(drive_service),
      webapps_registry_(webapps_registry),
      cache_(cache),
      blocking_task_runner_(blocking_task_runner),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

GDataWapiFeedLoader::~GDataWapiFeedLoader() {
}

void GDataWapiFeedLoader::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void GDataWapiFeedLoader::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void GDataWapiFeedLoader::ReloadFromServerIfNeeded(
    ContentOrigin initial_origin,
    int64 local_changestamp,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "ReloadFeedFromServerIfNeeded local_changestamp="
           << local_changestamp << ", initial_origin=" << initial_origin;

  // First fetch the latest changestamp to see if there were any new changes
  // there at all.
  if (gdata::util::IsDriveV2ApiEnabled()) {
    drive_service_->GetAccountMetadata(
        base::Bind(&GDataWapiFeedLoader::OnGetAboutResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   initial_origin,
                   local_changestamp,
                   callback));
    // Drive v2 needs a separate application list fetch operation.
    // TODO(kochi): Application list rarely changes and do not necessarily
    // refresed as often as files.
    drive_service_->GetApplicationInfo(
        base::Bind(&GDataWapiFeedLoader::OnGetApplicationList,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  drive_service_->GetAccountMetadata(
      base::Bind(&GDataWapiFeedLoader::OnGetAccountMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 initial_origin,
                 local_changestamp,
                 callback));
}

void GDataWapiFeedLoader::OnGetAccountMetadata(
    ContentOrigin initial_origin,
    int64 local_changestamp,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> feed_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(
      initial_origin,
      base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                 weak_ptr_factory_.GetWeakPtr())));
  params->start_changestamp = local_changestamp + 1;
  params->load_finished_callback = callback;

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    // Get changes starting from the next changestamp from what we have locally.
    LoadFromServer(params.Pass());
    return;
  }

  scoped_ptr<AccountMetadataFeed> account_metadata;
  if (feed_data.get()) {
    account_metadata = AccountMetadataFeed::CreateFrom(*feed_data);
#ifndef NDEBUG
    // Save account metadata feed for analysis.
    const FilePath path =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META).Append(
            kAccountMetadataFile);
    util::PostBlockingPoolSequencedTask(
        FROM_HERE,
        blocking_task_runner_,
        base::Bind(&SaveFeedOnBlockingPoolForDebugging,
                   path, base::Passed(&feed_data)));
#endif
  }

  if (!account_metadata.get()) {
    LoadFromServer(params.Pass());
    return;
  }

  webapps_registry_->UpdateFromFeed(*account_metadata.get());

  bool changes_detected = true;
  if (local_changestamp >= account_metadata->largest_changestamp()) {
    if (local_changestamp > account_metadata->largest_changestamp()) {
      LOG(WARNING) << "Cached client feed is fresher than server, client = "
                   << local_changestamp
                   << ", server = "
                   << account_metadata->largest_changestamp();
    }
    // If our cache holds the latest state from the server, change the
    // state to FROM_SERVER.
    resource_metadata_->set_origin(
        initial_origin == FROM_CACHE ? FROM_SERVER : initial_origin);
    changes_detected = false;
  }

  // No changes detected, tell the client that the loading was successful.
  if (!changes_detected) {
    if (!callback.is_null())
      callback.Run(DRIVE_FILE_OK);
    return;
  }

  // Load changes from the server.
  params->start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  params->root_feed_changestamp = account_metadata->largest_changestamp();
  LoadFromServer(params.Pass());
}

void GDataWapiFeedLoader::OnGetAboutResource(
    ContentOrigin initial_origin,
    int64 local_changestamp,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> feed_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(
      initial_origin,
      base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                 weak_ptr_factory_.GetWeakPtr())));
  params->load_finished_callback = callback;

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    // Get changes starting from the next changestamp from what we have locally.
    LoadFromServer(params.Pass());
    return;
  }

  scoped_ptr<AboutResource> about_resource;
  if (feed_data.get())
    about_resource = AboutResource::CreateFrom(*feed_data);

  if (!about_resource.get()) {
    LoadFromServer(params.Pass());
    return;
  }

  bool changes_detected = true;
  int64 largest_changestamp = about_resource->largest_change_id();
  resource_metadata_->InitializeRootEntry(about_resource->root_folder_id());

  if (local_changestamp >= largest_changestamp) {
    if (local_changestamp > largest_changestamp) {
      LOG(WARNING) << "Cached client feed is fresher than server, client = "
                   << local_changestamp
                   << ", server = "
                   << largest_changestamp;
    }
    // If our cache holds the latest state from the server, change the
    // state to FROM_SERVER.
    resource_metadata_->set_origin(
        initial_origin == FROM_CACHE ? FROM_SERVER : initial_origin);
    changes_detected = false;
  }

  // No changes detected, tell the client that the loading was successful.
  if (!changes_detected) {
    if (!callback.is_null())
      callback.Run(DRIVE_FILE_OK);
    return;
  }

  // Load changes from the server.
  params->start_changestamp = local_changestamp > 0 ? local_changestamp + 1 : 0;
  params->root_feed_changestamp = largest_changestamp;
  LoadFromServer(params.Pass());
}

void GDataWapiFeedLoader::OnGetApplicationList(
    GDataErrorCode status,
    scoped_ptr<base::Value> json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK)
    return;

  if (json.get()) {
    scoped_ptr<AppList> applist(AppList::CreateFrom(*json));
    if (applist.get()) {
      VLOG(1) << "applist get success";
      webapps_registry_->UpdateFromApplicationList(*applist.get());
    }
  }
}

void GDataWapiFeedLoader::LoadFromServer(scoped_ptr<LoadFeedParams> params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // base::Passed() may get evaluated first, so get a pointer to params.
  LoadFeedParams* params_ptr = params.get();
  if (gdata::util::IsDriveV2ApiEnabled()) {
    drive_service_->GetDocuments(
        params_ptr->feed_to_load,
        params_ptr->start_changestamp,
        std::string(),  // No search query.
        std::string(),  // No directory resource ID.
        base::Bind(&GDataWapiFeedLoader::OnGetChangelist,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   start_time));
  } else {
    drive_service_->GetDocuments(
        params_ptr->feed_to_load,
        params_ptr->start_changestamp,
        params_ptr->search_query,
        params_ptr->directory_resource_id,
        base::Bind(&GDataWapiFeedLoader::OnGetDocuments,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   start_time));
  }
}

void GDataWapiFeedLoader::LoadDirectoryFromServer(
    ContentOrigin initial_origin,
    const std::string& directory_resource_id,
    const LoadDocumentFeedCallback& feed_load_callback) {
  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(
      initial_origin, feed_load_callback));
  params->directory_resource_id = directory_resource_id;
  LoadFromServer(params.Pass());
}

void GDataWapiFeedLoader::SearchFromServer(
    ContentOrigin initial_origin,
    const std::string& search_query,
    const GURL& next_feed,
    const LoadDocumentFeedCallback& feed_load_callback) {
  DCHECK(!feed_load_callback.is_null());

  scoped_ptr<LoadFeedParams> params(new LoadFeedParams(
      initial_origin, feed_load_callback));
  params->search_query = search_query;
  params->feed_to_load = next_feed;
  LoadFromServer(params.Pass());
}

void GDataWapiFeedLoader::OnFeedFromServerLoaded(
    scoped_ptr<LoadFeedParams> params,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    if (!params->load_finished_callback.is_null())
      params->load_finished_callback.Run(error);
    return;
  }

  error = UpdateFromFeed(params->feed_list,
                         params->start_changestamp,
                         params->root_feed_changestamp);

  if (error != DRIVE_FILE_OK) {
    if (!params->load_finished_callback.is_null())
      params->load_finished_callback.Run(error);
    return;
  }

  // Save file system metadata to disk.
  SaveFileSystem();

  // Tell the client that the loading was successful.
  if (!params->load_finished_callback.is_null())
    params->load_finished_callback.Run(DRIVE_FILE_OK);

  FOR_EACH_OBSERVER(Observer, observers_, OnFeedFromServerLoaded());
}

void GDataWapiFeedLoader::OnGetDocuments(
    scoped_ptr<LoadFeedParams> params,
    base::TimeTicks start_time,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params->feed_list.empty()) {
    UMA_HISTOGRAM_TIMES("Gdata.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error == DRIVE_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    error = DRIVE_FILE_ERROR_FAILED;
  }

  if (error != DRIVE_FILE_OK) {
    resource_metadata_->set_origin(params->initial_origin);
    RunFeedLoadCallback(params.Pass(), error);
    return;
  }

  GURL next_feed_url;
  scoped_ptr<DocumentFeed> current_feed(DocumentFeed::ExtractAndParse(*data));
  if (!current_feed.get()) {
    RunFeedLoadCallback(params.Pass(), DRIVE_FILE_ERROR_FAILED);
    return;
  }
  const bool has_next_feed_url = current_feed->GetNextFeedURL(&next_feed_url);

#ifndef NDEBUG
  // Save initial root feed for analysis.
  std::string file_name =
      base::StringPrintf("DEBUG_feed_%" PRId64 ".json",
                         params->start_changestamp);
  util::PostBlockingPoolSequencedTask(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&SaveFeedOnBlockingPoolForDebugging,
                 cache_->GetCacheDirectoryPath(
                     DriveCache::CACHE_TYPE_META).Append(file_name),
                 base::Passed(&data)));
#endif

  // Add the current feed to the list of collected feeds for this directory.
  params->feed_list.push_back(current_feed.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list.size(); ++i)
    num_accumulated_entries += params->feed_list[i]->entries().size();

  // Check if we need to collect more data to complete the directory list.
  if (has_next_feed_url && !next_feed_url.is_empty()) {
    // Post an UI update event to make the UI smoother.
    GetDocumentsUiState* ui_state = params->ui_state.get();
    if (ui_state == NULL) {
      ui_state = new GetDocumentsUiState(base::TimeTicks::Now());
      params->ui_state.reset(ui_state);
    }
    DCHECK(ui_state);

    if ((ui_state->num_fetched_documents - ui_state->num_showing_documents)
        < kFetchUiUpdateStep) {
      // Currently the UI update is stopped. Start UI periodic callback.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(&GDataWapiFeedLoader::OnNotifyDocumentFeedFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()));
    }
    ui_state->num_fetched_documents = num_accumulated_entries;
    ui_state->feed_fetching_elapsed_time = base::TimeTicks::Now() - start_time;

    // |params| will be passed to the callback and thus nulled. Extract the
    // pointer so we can use it bellow.
    LoadFeedParams* params_ptr = params.get();
    // Kick off the remaining part of the feeds.
    drive_service_->GetDocuments(
        next_feed_url,
        params_ptr->start_changestamp,
        params_ptr->search_query,
        params_ptr->directory_resource_id,
        base::Bind(&GDataWapiFeedLoader::OnGetDocuments,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Gdata.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  RunFeedLoadCallback(params.Pass(), error);
}

void GDataWapiFeedLoader::OnGetChangelist(
    scoped_ptr<LoadFeedParams> params,
    base::TimeTicks start_time,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params->feed_list.empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error == DRIVE_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    error = DRIVE_FILE_ERROR_FAILED;
  }

  if (error != DRIVE_FILE_OK) {
    resource_metadata_->set_origin(params->initial_origin);
    RunFeedLoadCallback(params.Pass(), error);
    return;
  }

  GURL next_feed_url;
  scoped_ptr<ChangeList> current_feed(ChangeList::CreateFrom(*data));
  if (!current_feed.get()) {
    RunFeedLoadCallback(params.Pass(), DRIVE_FILE_ERROR_FAILED);
    return;
  }
  const bool has_next_feed = !current_feed->next_page_token().empty();

#ifndef NDEBUG
  // Save initial root feed for analysis.
  std::string file_name =
      base::StringPrintf("DEBUG_changelist_%" PRId64 ".json",
                         params->start_changestamp);
  util::PostBlockingPoolSequencedTask(
      FROM_HERE,
      blocking_task_runner_,
      base::Bind(&SaveFeedOnBlockingPoolForDebugging,
                 cache_->GetCacheDirectoryPath(
                     DriveCache::CACHE_TYPE_META).Append(file_name),
                 base::Passed(&data)));
#endif

  // Add the current feed to the list of collected feeds for this directory.
  scoped_ptr<DocumentFeed> feed =
      DocumentFeed::CreateFromChangeList(*current_feed);
  params->feed_list.push_back(feed.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list.size(); ++i)
    num_accumulated_entries += params->feed_list[i]->entries().size();

  // Check if we need to collect more data to complete the directory list.
  if (has_next_feed) {
    // Post an UI update event to make the UI smoother.
    GetDocumentsUiState* ui_state = params->ui_state.get();
    if (ui_state == NULL) {
      ui_state = new GetDocumentsUiState(base::TimeTicks::Now());
      params->ui_state.reset(ui_state);
    }
    DCHECK(ui_state);

    if ((ui_state->num_fetched_documents - ui_state->num_showing_documents)
        < kFetchUiUpdateStep) {
      // Currently the UI update is stopped. Start UI periodic callback.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(&GDataWapiFeedLoader::OnNotifyDocumentFeedFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()));
    }
    ui_state->num_fetched_documents = num_accumulated_entries;
    ui_state->feed_fetching_elapsed_time = base::TimeTicks::Now() - start_time;

    // Kick off the remaining part of the feeds.
    drive_service_->GetDocuments(
        current_feed->next_link(),
        params->start_changestamp,
        std::string(),  // No search query.
        std::string(),  // No directory resource ID.
        base::Bind(&GDataWapiFeedLoader::OnGetChangelist,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&params),
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Drive.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  RunFeedLoadCallback(params.Pass(), error);
}

void GDataWapiFeedLoader::OnNotifyDocumentFeedFetched(
    base::WeakPtr<GetDocumentsUiState> ui_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ui_state) {
    // The ui state instance is already released, which means the fetching
    // is done and we don't need to update any more.
    return;
  }

  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - ui_state->start_time;

  if (ui_state->num_showing_documents + kFetchUiUpdateStep <=
      ui_state->num_fetched_documents) {
    ui_state->num_showing_documents += kFetchUiUpdateStep;
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnDocumentFeedFetched(ui_state->num_showing_documents));

    int num_remaining_ui_updates =
        (ui_state->num_fetched_documents - ui_state->num_showing_documents)
        / kFetchUiUpdateStep;
    if (num_remaining_ui_updates > 0) {
      // Heuristically, we use fetched time duration to calculate the next
      // UI update timing.
      base::TimeDelta remaining_duration =
          ui_state->feed_fetching_elapsed_time - elapsed_time;
      base::MessageLoopProxy::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&GDataWapiFeedLoader::OnNotifyDocumentFeedFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()),
          remaining_duration / num_remaining_ui_updates);
    }
  }
}

void GDataWapiFeedLoader::LoadFromCache(
    bool should_load_from_server,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LoadRootFeedParams* params = new LoadRootFeedParams(should_load_from_server,
                                                      callback);
  FilePath path = cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META);
  if (UseLevelDB()) {
    path = path.Append(kResourceMetadataDBFile);
    resource_metadata_->InitFromDB(path, blocking_task_runner_,
        base::Bind(
            &GDataWapiFeedLoader::ContinueWithInitializedDirectoryService,
            weak_ptr_factory_.GetWeakPtr(),
            base::Owned(params)));
  } else {
    path = path.Append(kFilesystemProtoFile);
    BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&LoadProtoOnBlockingPool, path, params),
        base::Bind(&GDataWapiFeedLoader::OnProtoLoaded,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(params)));
  }
}

void GDataWapiFeedLoader::OnProtoLoaded(LoadRootFeedParams* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If we have already received updates from the server, bail out.
  if (resource_metadata_->origin() == FROM_SERVER)
    return;

  // Update directory structure only if everything is OK and we haven't yet
  // received the feed from the server yet.
  if (params->load_error == DRIVE_FILE_OK) {
    DVLOG(1) << "ParseFromString";
    if (resource_metadata_->ParseFromString(params->proto)) {
      resource_metadata_->set_last_serialized(params->last_modified);
      resource_metadata_->set_serialized_size(params->proto.size());
    } else {
      params->load_error = DRIVE_FILE_ERROR_FAILED;
      LOG(WARNING) << "Parse of cached proto file failed";
    }
  }

  ContinueWithInitializedDirectoryService(params, params->load_error);
}

void GDataWapiFeedLoader::ContinueWithInitializedDirectoryService(
    LoadRootFeedParams* params,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Time elapsed to load directory service from disk="
           << (base::Time::Now() - params->load_start_time).InMilliseconds()
           << " milliseconds";

  // TODO(satorux): Simplify the callback handling. crbug.com/142799
  FileOperationCallback callback = params->callback;
  // If we got feed content from cache, tell the client that the loading was
  // successful.
  if (error == DRIVE_FILE_OK && !callback.is_null()) {
    callback.Run(DRIVE_FILE_OK);
    // Reset the callback so we don't run the same callback once
    // ReloadFeedFromServerIfNeeded() is complete.
    callback.Reset();
  }

  if (!params->should_load_from_server)
    return;

  // Decide the |initial_origin| to pass to ReloadFromServerIfNeeded().
  // This is used to restore directory content origin to its initial value when
  // we fail to retrieve the feed from server.
  // By default, if directory content is not yet initialized, restore content
  // origin to UNINITIALIZED in case of failure.
  ContentOrigin initial_origin = UNINITIALIZED;
  if (resource_metadata_->origin() != INITIALIZING) {
    // If directory content is already initialized, restore content origin
    // to FROM_CACHE in case of failure.
    initial_origin = FROM_CACHE;
    resource_metadata_->set_origin(REFRESHING);
  }

  // Kick off the retrieval of the feed from server. If we have previously
  // |reported| to the original callback, then we just need to refresh the
  // content without continuing search upon operation completion.
  ReloadFromServerIfNeeded(initial_origin,
                           resource_metadata_->largest_changestamp(),
                           callback);
}

void GDataWapiFeedLoader::SaveFileSystem() {
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
    util::PostBlockingPoolSequencedTask(
        FROM_HERE,
        blocking_task_runner_,
        base::Bind(&SaveProtoOnBlockingPool, path,
                   base::Passed(serialized_proto.Pass())));
  }
}

DriveFileError GDataWapiFeedLoader::UpdateFromFeed(
    const ScopedVector<DocumentFeed>& feed_list,
    int64 start_changestamp,
    int64 root_feed_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Updating directory with a feed";

  std::set<FilePath> changed_dirs;

  GDataWapiFeedProcessor feed_processor(resource_metadata_);
  const DriveFileError error = feed_processor.ApplyFeeds(
      feed_list,
      start_changestamp,
      root_feed_changestamp,
      &changed_dirs);

  // Don't send directory content change notification while performing
  // the initial content retrieval.
  const bool should_notify_directory_changed = (start_changestamp != 0);
  if (should_notify_directory_changed) {
    for (std::set<FilePath>::iterator dir_iter = changed_dirs.begin();
        dir_iter != changed_dirs.end(); ++dir_iter) {
      FOR_EACH_OBSERVER(Observer, observers_,
                        OnDirectoryChanged(*dir_iter));
    }
  }

  return error;
}

}  // namespace gdata
