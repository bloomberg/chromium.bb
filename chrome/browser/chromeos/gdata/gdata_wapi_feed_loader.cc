// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_wapi_feed_loader.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
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
    params->load_error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }
  params->last_modified = info.last_modified;
  if (!file_util::ReadFileToString(path, &params->proto)) {
    LOG(WARNING) << "Proto file not found at " << path.value();
    params->load_error = GDATA_FILE_ERROR_NOT_FOUND;
    return;
  }
  params->load_error = GDATA_FILE_OK;
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

}  // namespace

LoadRootFeedParams::LoadRootFeedParams(
    bool should_load_from_server,
    const FileOperationCallback& callback)
    : should_load_from_server(should_load_from_server),
      load_error(GDATA_FILE_OK),
      load_start_time(base::Time::Now()),
      callback(callback) {
}

LoadRootFeedParams::~LoadRootFeedParams() {
}

GetDocumentsParams::GetDocumentsParams(
    int64 start_changestamp,
    int64 root_feed_changestamp,
    std::vector<DocumentFeed*>* feed_list,
    bool should_fetch_multiple_feeds,
    const std::string& search_query,
    const std::string& directory_resource_id,
    const FileOperationCallback& callback,
    GetDocumentsUiState* ui_state)
    : start_changestamp(start_changestamp),
      root_feed_changestamp(root_feed_changestamp),
      feed_list(feed_list),
      should_fetch_multiple_feeds(should_fetch_multiple_feeds),
      search_query(search_query),
      directory_resource_id(directory_resource_id),
      callback(callback),
      ui_state(ui_state) {
}

GetDocumentsParams::~GetDocumentsParams() {
  STLDeleteElements(feed_list.get());
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
    GDataDirectoryService* directory_service,
    DocumentsServiceInterface* documents_service,
    DriveWebAppsRegistryInterface* webapps_registry,
    GDataCache* cache,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : directory_service_(directory_service),
      documents_service_(documents_service),
      webapps_registry_(webapps_registry),
      cache_(cache),
      blocking_task_runner_(blocking_task_runner),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
    documents_service_->GetAboutResource(
        base::Bind(&GDataWapiFeedLoader::OnGetAboutResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   initial_origin,
                   local_changestamp,
                   callback));
    // Drive v2 needs a separate application list fetch operation.
    // TODO(kochi): Application list rarely changes and do not necessarily
    // refresed as often as files.
    documents_service_->GetApplicationList(
        base::Bind(&GDataWapiFeedLoader::OnGetApplicationList,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  documents_service_->GetAccountMetadata(
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

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    // Get changes starting from the next changestamp from what we have locally.
    LoadFromServer(initial_origin,
                   local_changestamp + 1, 0,
                   true,  /* should_fetch_multiple_feeds */
                   std::string() /* no search query */,
                   GURL(), /* feed not explicitly set */
                   std::string() /* no directory resource ID */,
                   callback,
                   base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  scoped_ptr<AccountMetadataFeed> account_metadata;
  if (feed_data.get()) {
    account_metadata = AccountMetadataFeed::CreateFrom(*feed_data);
#ifndef NDEBUG
    // Save account metadata feed for analysis.
    const FilePath path =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_META).Append(
            kAccountMetadataFile);
    util::PostBlockingPoolSequencedTask(
        FROM_HERE,
        blocking_task_runner_,
        base::Bind(&SaveFeedOnBlockingPoolForDebugging,
                   path, base::Passed(&feed_data)));
#endif
  }

  if (!account_metadata.get()) {
    LoadFromServer(initial_origin,
                   local_changestamp + 1, 0,
                   true,  /* should_fetch_multiple_feeds */
                   std::string() /* no search query */,
                   GURL(), /* feed not explicitly set */
                   std::string() /* no directory resource ID */,
                   callback,
                   base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                              weak_ptr_factory_.GetWeakPtr()));
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
    directory_service_->set_origin(
        initial_origin == FROM_CACHE ? FROM_SERVER : initial_origin);
    changes_detected = false;
  }

  // No changes detected, tell the client that the loading was successful.
  if (!changes_detected) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK);
    return;
  }

  // Load changes from the server.
  LoadFromServer(initial_origin,
                 local_changestamp > 0 ? local_changestamp + 1 : 0,
                 account_metadata->largest_changestamp(),
                 true,  /* should_fetch_multiple_feeds */
                 std::string() /* no search query */,
                 GURL(), /* feed not explicitly set */
                 std::string() /* no directory resource ID */,
                 callback,
                 base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
}

void GDataWapiFeedLoader::OnGetAboutResource(
    ContentOrigin initial_origin,
    int64 local_changestamp,
    const FileOperationCallback& callback,
    GDataErrorCode status,
    scoped_ptr<base::Value> feed_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK) {
    // Get changes starting from the next changestamp from what we have locally.
    LoadFromServer(initial_origin,
                   local_changestamp + 1, 0,
                   true,  /* should_fetch_multiple_feeds */
                   std::string() /* no search query */,
                   GURL(), /* feed not explicitly set */
                   std::string() /* no directory resource ID */,
                   callback,
                   base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  scoped_ptr<AboutResource> about_resource;
  if (feed_data.get())
    about_resource = AboutResource::CreateFrom(*feed_data);

  if (!about_resource.get()) {
    LoadFromServer(initial_origin,
                   local_changestamp + 1, 0,
                   true,  /* should_fetch_multiple_feeds */
                   std::string() /* no search query */,
                   GURL(), /* feed not explicitly set */
                   std::string() /* no directory resource ID */,
                   callback,
                   base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  bool changes_detected = true;
  int64 largest_changestamp = about_resource->largest_change_id();
  directory_service_->InitializeRootEntry(about_resource->root_folder_id());

  if (local_changestamp >= largest_changestamp) {
    if (local_changestamp > largest_changestamp) {
      LOG(WARNING) << "Cached client feed is fresher than server, client = "
                   << local_changestamp
                   << ", server = "
                   << largest_changestamp;
    }
    // If our cache holds the latest state from the server, change the
    // state to FROM_SERVER.
    directory_service_->set_origin(
        initial_origin == FROM_CACHE ? FROM_SERVER : initial_origin);
    changes_detected = false;
  }

  // No changes detected, tell the client that the loading was successful.
  if (!changes_detected) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK);
    return;
  }

  // Load changes from the server.
  LoadFromServer(initial_origin,
                 local_changestamp > 0 ? local_changestamp + 1 : 0,
                 largest_changestamp,
                 true,  /* should_fetch_multiple_feeds */
                 std::string() /* no search query */,
                 GURL(), /* feed not explicitly set */
                 std::string() /* no directory resource ID */,
                 callback,
                 base::Bind(&GDataWapiFeedLoader::OnFeedFromServerLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
}

void GDataWapiFeedLoader::OnGetApplicationList(
    GDataErrorCode status,
    scoped_ptr<base::Value> json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error != GDATA_FILE_OK)
    return;

  if (json.get()) {
    scoped_ptr<AppList> applist(AppList::CreateFrom(*json));
    if (applist.get()) {
      VLOG(1) << "applist get success";
      webapps_registry_->UpdateFromApplicationList(*applist.get());
    }
  }
}

// TODO(kochi): Fix too many parameters.  http://crbug.com/141359
void GDataWapiFeedLoader::LoadFromServer(
    ContentOrigin initial_origin,
    int64 start_changestamp,
    int64 root_feed_changestamp,
    bool should_fetch_multiple_feeds,
    const std::string& search_query,
    const GURL& feed_to_load,
    const std::string& directory_resource_id,
    const FileOperationCallback& load_finished_callback,
    const LoadDocumentFeedCallback& feed_load_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!feed_load_callback.is_null());

  // |feed_list| will contain the list of all collected feed updates that
  // we will receive through calls of DocumentsService::GetDocuments().
  scoped_ptr<std::vector<DocumentFeed*> > feed_list(
      new std::vector<DocumentFeed*>);
  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (gdata::util::IsDriveV2ApiEnabled()) {
    documents_service_->GetChangelist(
        feed_to_load,
        start_changestamp,
        base::Bind(&GDataWapiFeedLoader::OnGetChangelist,
                   weak_ptr_factory_.GetWeakPtr(),
                   initial_origin,
                   feed_load_callback,
                   base::Owned(new GetDocumentsParams(
                       start_changestamp,
                       root_feed_changestamp,
                       feed_list.release(),
                       should_fetch_multiple_feeds,
                       search_query,
                       directory_resource_id,
                       load_finished_callback,
                       NULL)),
                   start_time));
    return;
  }

  documents_service_->GetDocuments(
      feed_to_load,
      start_changestamp,
      search_query,
      directory_resource_id,
      base::Bind(&GDataWapiFeedLoader::OnGetDocuments,
                 weak_ptr_factory_.GetWeakPtr(),
                 initial_origin,
                 feed_load_callback,
                 base::Owned(new GetDocumentsParams(start_changestamp,
                                                    root_feed_changestamp,
                                                    feed_list.release(),
                                                    should_fetch_multiple_feeds,
                                                    search_query,
                                                    directory_resource_id,
                                                    load_finished_callback,
                                                    NULL)),
                 start_time));
}

void GDataWapiFeedLoader::OnFeedFromServerLoaded(GetDocumentsParams* params,
                                                 GDataFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != GDATA_FILE_OK) {
    if (!params->callback.is_null())
      params->callback.Run(error);
    return;
  }

  error = UpdateFromFeed(*params->feed_list,
                         params->start_changestamp,
                         params->root_feed_changestamp);

  if (error != GDATA_FILE_OK) {
    if (!params->callback.is_null())
      params->callback.Run(error);

    return;
  }

  // Save file system metadata to disk.
  SaveFileSystem();

  // Tell the client that the loading was successful.
  if (!params->callback.is_null()) {
    params->callback.Run(GDATA_FILE_OK);
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnFeedFromServerLoaded());
}

void GDataWapiFeedLoader::OnGetDocuments(
    ContentOrigin initial_origin,
    const LoadDocumentFeedCallback& callback,
    GetDocumentsParams* params,
    base::TimeTicks start_time,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (params->feed_list->empty()) {
    UMA_HISTOGRAM_TIMES("Gdata.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error == GDATA_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    error = GDATA_FILE_ERROR_FAILED;
  }

  if (error != GDATA_FILE_OK) {
    directory_service_->set_origin(initial_origin);
    callback.Run(params, error);
    return;
  }

  GURL next_feed_url;
  scoped_ptr<DocumentFeed> current_feed(DocumentFeed::ExtractAndParse(*data));
  if (!current_feed.get()) {
    callback.Run(params, GDATA_FILE_ERROR_FAILED);
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
                     GDataCache::CACHE_TYPE_META).Append(file_name),
                 base::Passed(&data)));
#endif

  // Add the current feed to the list of collected feeds for this directory.
  params->feed_list->push_back(current_feed.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list->size(); ++i)
    num_accumulated_entries += params->feed_list->at(i)->entries().size();

  // Check if we need to collect more data to complete the directory list.
  if (params->should_fetch_multiple_feeds && has_next_feed_url &&
      !next_feed_url.is_empty()) {
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
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&GDataWapiFeedLoader::OnNotifyDocumentFeedFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()));
    }
    ui_state->num_fetched_documents = num_accumulated_entries;
    ui_state->feed_fetching_elapsed_time = base::TimeTicks::Now() - start_time;

    // Kick of the remaining part of the feeds.
    documents_service_->GetDocuments(
        next_feed_url,
        params->start_changestamp,
        params->search_query,
        params->directory_resource_id,
        base::Bind(&GDataWapiFeedLoader::OnGetDocuments,
                   weak_ptr_factory_.GetWeakPtr(),
                   initial_origin,
                   callback,
                   base::Owned(
                       new GetDocumentsParams(
                           params->start_changestamp,
                           params->root_feed_changestamp,
                           params->feed_list.release(),
                           params->should_fetch_multiple_feeds,
                           params->search_query,
                           params->directory_resource_id,
                           params->callback,
                           params->ui_state.release())),
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Gdata.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  callback.Run(params, error);
}

void GDataWapiFeedLoader::OnGetChangelist(
    ContentOrigin initial_origin,
    const LoadDocumentFeedCallback& callback,
    GetDocumentsParams* params,
    base::TimeTicks start_time,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (params->feed_list->empty()) {
    UMA_HISTOGRAM_TIMES("Drive.InitialFeedLoadTime",
                        base::TimeTicks::Now() - start_time);
  }

  GDataFileError error = util::GDataToGDataFileError(status);
  if (error == GDATA_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    error = GDATA_FILE_ERROR_FAILED;
  }

  if (error != GDATA_FILE_OK) {
    directory_service_->set_origin(initial_origin);
    callback.Run(params, error);
    return;
  }

  GURL next_feed_url;
  scoped_ptr<ChangeList> current_feed(ChangeList::CreateFrom(*data));
  if (!current_feed.get()) {
    callback.Run(params, GDATA_FILE_ERROR_FAILED);
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
                     GDataCache::CACHE_TYPE_META).Append(file_name),
                 base::Passed(&data)));
#endif

  // Add the current feed to the list of collected feeds for this directory.
  scoped_ptr<DocumentFeed> feed =
      DocumentFeed::CreateFromChangeList(*current_feed);
  params->feed_list->push_back(feed.release());

  // Compute and notify the number of entries fetched so far.
  int num_accumulated_entries = 0;
  for (size_t i = 0; i < params->feed_list->size(); ++i)
    num_accumulated_entries += params->feed_list->at(i)->entries().size();

  // Check if we need to collect more data to complete the directory list.
  if (params->should_fetch_multiple_feeds && has_next_feed) {

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
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&GDataWapiFeedLoader::OnNotifyDocumentFeedFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ui_state->weak_ptr_factory.GetWeakPtr()));
    }
    ui_state->num_fetched_documents = num_accumulated_entries;
    ui_state->feed_fetching_elapsed_time = base::TimeTicks::Now() - start_time;

    // Kick of the remaining part of the feeds.
    documents_service_->GetChangelist(
        current_feed->next_link(),
        params->start_changestamp,
        base::Bind(&GDataWapiFeedLoader::OnGetChangelist,
                   weak_ptr_factory_.GetWeakPtr(),
                   initial_origin,
                   callback,
                   base::Owned(
                       new GetDocumentsParams(
                           params->start_changestamp,
                           params->root_feed_changestamp,
                           params->feed_list.release(),
                           params->should_fetch_multiple_feeds,
                           params->search_query,
                           params->directory_resource_id,
                           params->callback,
                           NULL)),
                   start_time));
    return;
  }

  // Notify the observers that all document feeds are fetched.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDocumentFeedFetched(num_accumulated_entries));

  UMA_HISTOGRAM_TIMES("Drive.EntireFeedLoadTime",
                      base::TimeTicks::Now() - start_time);

  // Run the callback so the client can process the retrieved feeds.
  callback.Run(params, error);
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
      MessageLoop::current()->PostDelayedTask(
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
  FilePath path = cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_META);
  if (UseLevelDB()) {
    path = path.Append(kResourceMetadataDBFile);
    directory_service_->InitFromDB(path, blocking_task_runner_,
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
  if (directory_service_->origin() == FROM_SERVER)
    return;

  // Update directory structure only if everything is OK and we haven't yet
  // received the feed from the server yet.
  if (params->load_error == GDATA_FILE_OK) {
    DVLOG(1) << "ParseFromString";
    if (directory_service_->ParseFromString(params->proto)) {
      directory_service_->set_last_serialized(params->last_modified);
      directory_service_->set_serialized_size(params->proto.size());
    } else {
      params->load_error = GDATA_FILE_ERROR_FAILED;
      LOG(WARNING) << "Parse of cached proto file failed";
    }
  }

  ContinueWithInitializedDirectoryService(params, params->load_error);
}

void GDataWapiFeedLoader::ContinueWithInitializedDirectoryService(
    LoadRootFeedParams* params,
    GDataFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Time elapsed to load directory service from disk="
           << (base::Time::Now() - params->load_start_time).InMilliseconds()
           << " milliseconds";

  // TODO(satorux): Simplify the callback handling. crbug.com/142799
  FileOperationCallback callback = params->callback;
  // If we got feed content from cache, tell the client that the loading was
  // successful.
  if (error == GDATA_FILE_OK && !callback.is_null()) {
    callback.Run(GDATA_FILE_OK);
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
  if (directory_service_->origin() != INITIALIZING) {
    // If directory content is already initialized, restore content origin
    // to FROM_CACHE in case of failure.
    initial_origin = FROM_CACHE;
    directory_service_->set_origin(REFRESHING);
  }

  // Kick of the retrieval of the feed from server. If we have previously
  // |reported| to the original callback, then we just need to refresh the
  // content without continuing search upon operation completion.
  ReloadFromServerIfNeeded(initial_origin,
                           directory_service_->largest_changestamp(),
                           callback);
}

void GDataWapiFeedLoader::SaveFileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!ShouldSerializeFileSystemNow(directory_service_->serialized_size(),
                                    directory_service_->last_serialized())) {
    return;
  }

  if (UseLevelDB()) {
    directory_service_->SaveToDB();
  } else {
    const FilePath path =
        cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_META).Append(
            kFilesystemProtoFile);
    scoped_ptr<std::string> serialized_proto(new std::string());
    directory_service_->SerializeToString(serialized_proto.get());
    directory_service_->set_last_serialized(base::Time::Now());
    directory_service_->set_serialized_size(serialized_proto->size());
    util::PostBlockingPoolSequencedTask(
        FROM_HERE,
        blocking_task_runner_,
        base::Bind(&SaveProtoOnBlockingPool, path,
                   base::Passed(serialized_proto.Pass())));
  }
}

GDataFileError GDataWapiFeedLoader::UpdateFromFeed(
    const std::vector<DocumentFeed*>& feed_list,
    int64 start_changestamp,
    int64 root_feed_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Updating directory with a feed";

  std::set<FilePath> changed_dirs;

  GDataWapiFeedProcessor feed_processor(directory_service_);
  const GDataFileError error = feed_processor.ApplyFeeds(
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
