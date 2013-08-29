// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"

#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive_app_registry.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::BrowserThread;

namespace file_manager {
namespace {

// List of connection types of drive.
// Keep this in sync with the DriveConnectionType in volume_manager.js.
const char kDriveConnectionTypeOffline[] = "offline";
const char kDriveConnectionTypeMetered[] = "metered";
const char kDriveConnectionTypeOnline[] = "online";


// List of reasons of kDriveConnectionType*.
// Keep this in sync with the DriveConnectionReason in volume_manager.js.
const char kDriveConnectionReasonNotReady[] = "not_ready";
const char kDriveConnectionReasonNoNetwork[] = "no_network";
const char kDriveConnectionReasonNoService[] = "no_service";

// Does nothing with a bool parameter. Used as a placeholder for calling
// ClearCacheAndRemountFileSystem(). TODO(yoshiki): Handle an error from
// ClearCacheAndRemountFileSystem() properly: http://crbug.com/140511.
void DoNothingWithBool(bool /* success */) {
}

// Copies properties from |entry_proto| to |property_dict|.
void FillDriveEntryPropertiesValue(
    const drive::ResourceEntry& entry_proto,
    DictionaryValue* property_dict) {
  property_dict->SetBoolean("sharedWithMe", entry_proto.shared_with_me());

  if (!entry_proto.has_file_specific_info())
    return;

  const drive::FileSpecificInfo& file_specific_info =
      entry_proto.file_specific_info();

  property_dict->SetString("thumbnailUrl", file_specific_info.thumbnail_url());
  property_dict->SetBoolean("isHosted",
                            file_specific_info.is_hosted_document());
  property_dict->SetString("contentMimeType",
                           file_specific_info.content_mime_type());
}

}  // namespace

GetDriveEntryPropertiesFunction::GetDriveEntryPropertiesFunction() {
}

GetDriveEntryPropertiesFunction::~GetDriveEntryPropertiesFunction() {
}

bool GetDriveEntryPropertiesFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string file_url_str;
  if (args_->GetSize() != 1 || !args_->GetString(0, &file_url_str))
    return false;

  GURL file_url = GURL(file_url_str);
  file_path_ = drive::util::ExtractDrivePath(
      util::GetLocalPathFromURL(render_view_host(), profile(), file_url));

  properties_.reset(new base::DictionaryValue);
  properties_->SetString("fileUrl", file_url.spec());

  // Start getting the file info.
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service) {
    CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
    return true;
  }

  integration_service->file_system()->GetResourceEntryByPath(
      file_path_,
      base::Bind(&GetDriveEntryPropertiesFunction::OnGetFileInfo, this));
  return true;
}

void GetDriveEntryPropertiesFunction::OnGetFileInfo(
    drive::FileError error,
    scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(properties_);

  if (error != drive::FILE_ERROR_OK) {
    CompleteGetFileProperties(error);
    return;
  }
  DCHECK(entry);

  FillDriveEntryPropertiesValue(*entry, properties_.get());

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service) {
    CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
    return;
  }

  // The properties meaningful for directories are already filled in
  // FillDriveEntryPropertiesValue().
  if (entry.get() && !entry->has_file_specific_info()) {
    CompleteGetFileProperties(error);
    return;
  }

  const drive::FileSpecificInfo& file_specific_info =
      entry->file_specific_info();

  // Get drive WebApps that can accept this file. We just need to extract the
  // doc icon for the drive app, which is set as default.
  ScopedVector<drive::DriveAppInfo> drive_apps;
  integration_service->drive_app_registry()->GetAppsForFile(
      file_path_, file_specific_info.content_mime_type(), &drive_apps);
  if (!drive_apps.empty()) {
    std::string default_task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        *profile_->GetPrefs(),
        file_specific_info.content_mime_type(),
        file_path_.Extension());
    file_tasks::TaskDescriptor default_task;
    file_tasks::ParseTaskID(default_task_id, &default_task);
    DCHECK(default_task_id.empty() || !default_task.app_id.empty());
    for (size_t i = 0; i < drive_apps.size(); ++i) {
      const drive::DriveAppInfo* app_info = drive_apps[i];
      if (default_task.app_id == app_info->app_id) {
        // The drive app is set as default. Files.app should use the doc icon.
        const GURL doc_icon =
            drive::util::FindPreferredIcon(app_info->document_icons,
                                           drive::util::kPreferredIconSize);
        properties_->SetString("customIconUrl", doc_icon.spec());
      }
    }
  }

  integration_service->file_system()->GetCacheEntryByPath(
      file_path_,
      base::Bind(&GetDriveEntryPropertiesFunction::CacheStateReceived, this));
}

void GetDriveEntryPropertiesFunction::CacheStateReceived(
    bool /* success */,
    const drive::FileCacheEntry& cache_entry) {
  // In case of an error (i.e. success is false), cache_entry.is_*() all
  // returns false.
  properties_->SetBoolean("isPinned", cache_entry.is_pinned());
  properties_->SetBoolean("isPresent", cache_entry.is_present());
  properties_->SetBoolean("isDirty", cache_entry.is_dirty());

  CompleteGetFileProperties(drive::FILE_ERROR_OK);
}

void GetDriveEntryPropertiesFunction::CompleteGetFileProperties(
    drive::FileError error) {
  if (error != drive::FILE_ERROR_OK)
    properties_->SetInteger("errorCode", error);
  SetResult(properties_.release());
  SendResponse(true);
}

PinDriveFileFunction::PinDriveFileFunction() {
}

PinDriveFileFunction::~PinDriveFileFunction() {
}

bool PinDriveFileFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string url;
  bool set_pin = false;
  if (args_->GetSize() != 2 ||
      !args_->GetString(0, &url) ||
      !args_->GetBoolean(1, &set_pin))
    return false;

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  drive::FileSystemInterface* file_system =
      integration_service ? integration_service->file_system() : NULL;
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  base::FilePath drive_path =
      drive::util::ExtractDrivePath(
          util::GetLocalPathFromURL(render_view_host(), profile(), GURL(url)));
  if (set_pin) {
    file_system->Pin(drive_path,
                     base::Bind(&PinDriveFileFunction::OnPinStateSet, this));
  } else {
    file_system->Unpin(drive_path,
                       base::Bind(&PinDriveFileFunction::OnPinStateSet, this));
  }
  return true;
}

void PinDriveFileFunction::OnPinStateSet(drive::FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == drive::FILE_ERROR_OK) {
    SendResponse(true);
  } else {
    error_ = drive::FileErrorToString(error);
    SendResponse(false);
  }
}

GetDriveFilesFunction::GetDriveFilesFunction()
    : local_paths_(NULL) {
}

GetDriveFilesFunction::~GetDriveFilesFunction() {
}

bool GetDriveFilesFunction::RunImpl() {
  ListValue* file_urls_as_strings = NULL;
  if (!args_->GetList(0, &file_urls_as_strings))
    return false;

  // Convert the list of strings to a list of GURLs.
  for (size_t i = 0; i < file_urls_as_strings->GetSize(); ++i) {
    std::string file_url_as_string;
    if (!file_urls_as_strings->GetString(i, &file_url_as_string))
      return false;
    const base::FilePath path = util::GetLocalPathFromURL(
        render_view_host(), profile(), GURL(file_url_as_string));
    DCHECK(drive::util::IsUnderDriveMountPoint(path));
    base::FilePath drive_path = drive::util::ExtractDrivePath(path);
    remaining_drive_paths_.push(drive_path);
  }

  local_paths_ = new ListValue;
  GetFileOrSendResponse();
  return true;
}

void GetDriveFilesFunction::GetFileOrSendResponse() {
  // Send the response if all files are obtained.
  if (remaining_drive_paths_.empty()) {
    SetResult(local_paths_);
    SendResponse(true);
    return;
  }

  // Get the file on the top of the queue.
  base::FilePath drive_path = remaining_drive_paths_.front();

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service) {
    OnFileReady(drive::FILE_ERROR_FAILED, drive_path,
                scoped_ptr<drive::ResourceEntry>());
    return;
  }

  integration_service->file_system()->GetFileByPath(
      drive_path,
      base::Bind(&GetDriveFilesFunction::OnFileReady, this));
}


void GetDriveFilesFunction::OnFileReady(
    drive::FileError error,
    const base::FilePath& local_path,
    scoped_ptr<drive::ResourceEntry> entry) {
  base::FilePath drive_path = remaining_drive_paths_.front();

  if (error == drive::FILE_ERROR_OK) {
    local_paths_->Append(new base::StringValue(local_path.value()));
    DVLOG(1) << "Got " << drive_path.value() << " as " << local_path.value();

    // TODO(benchan): If the file is a hosted document, a temporary JSON file
    // is created to represent the document. The JSON file is not cached and
    // should be deleted after use. We need to somehow communicate with
    // file_manager.js to manage the lifetime of the temporary file.
    // See crosbug.com/28058.
  } else {
    local_paths_->Append(new base::StringValue(""));
    DVLOG(1) << "Failed to get " << drive_path.value()
             << " with error code: " << error;
  }

  remaining_drive_paths_.pop();

  // Start getting the next file.
  GetFileOrSendResponse();
}

CancelFileTransfersFunction::CancelFileTransfersFunction() {
}

CancelFileTransfersFunction::~CancelFileTransfersFunction() {
}

bool CancelFileTransfersFunction::RunImpl() {
  ListValue* url_list = NULL;
  if (!args_->GetList(0, &url_list))
    return false;

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service)
    return false;

  // Create the mapping from file path to job ID.
  drive::JobListInterface* job_list = integration_service->job_list();
  DCHECK(job_list);
  std::vector<drive::JobInfo> jobs = job_list->GetJobInfoList();

  typedef std::map<base::FilePath, std::vector<drive::JobID> > PathToIdMap;
  PathToIdMap path_to_id_map;
  for (size_t i = 0; i < jobs.size(); ++i) {
    if (drive::IsActiveFileTransferJobInfo(jobs[i]))
      path_to_id_map[jobs[i].file_path].push_back(jobs[i].job_id);
  }

  // Cancel by Job ID.
  scoped_ptr<ListValue> responses(new ListValue());
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    std::string url_as_string;
    url_list->GetString(i, &url_as_string);

    base::FilePath file_path = util::GetLocalPathFromURL(
        render_view_host(), profile(), GURL(url_as_string));
    if (file_path.empty())
      continue;

    DCHECK(drive::util::IsUnderDriveMountPoint(file_path));
    file_path = drive::util::ExtractDrivePath(file_path);
    scoped_ptr<DictionaryValue> result(new DictionaryValue());

    // Cancel all the jobs for the file.
    PathToIdMap::iterator it = path_to_id_map.find(file_path);
    if (it != path_to_id_map.end()) {
      for (size_t i = 0; i < it->second.size(); ++i)
        job_list->CancelJob(it->second[i]);
    }
    result->SetBoolean("canceled", it != path_to_id_map.end());
    // TODO(kinaba): simplify cancelFileTransfer() to take single URL each time,
    // and eliminate this field; it is just returning a copy of the argument.
    result->SetString("fileUrl", url_as_string);
    responses->Append(result.release());
  }
  SetResult(responses.release());
  SendResponse(true);
  return true;
}

SearchDriveFunction::SearchDriveFunction() {
}

SearchDriveFunction::~SearchDriveFunction() {
}

bool SearchDriveFunction::RunImpl() {
  DictionaryValue* search_params;
  if (!args_->GetDictionary(0, &search_params))
    return false;

  std::string query;
  if (!search_params->GetString("query", &query))
    return false;

  std::string next_feed;
  if (!search_params->GetString("nextFeed", &next_feed))
    return false;

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service || !integration_service->file_system())
    return false;

  integration_service->file_system()->Search(
      query, next_feed,
      base::Bind(&SearchDriveFunction::OnSearch, this));
  return true;
}

void SearchDriveFunction::OnSearch(
    drive::FileError error,
    const std::string& next_feed,
    scoped_ptr<std::vector<drive::SearchResultInfo> > results) {
  if (error != drive::FILE_ERROR_OK) {
    SendResponse(false);
    return;
  }

  DCHECK(results.get());

  base::ListValue* entries = new ListValue();

  // Convert Drive files to something File API stack can understand.
  GURL origin_url = source_url_.GetOrigin();
  fileapi::FileSystemType file_system_type = fileapi::kFileSystemTypeExternal;
  GURL file_system_root_url =
      fileapi::GetFileSystemRootURI(origin_url, file_system_type);
  std::string file_system_name =
      fileapi::GetFileSystemName(origin_url, file_system_type);
  for (size_t i = 0; i < results->size(); ++i) {
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("fileSystemName", file_system_name);
    entry->SetString("fileSystemRoot", file_system_root_url.spec());
    entry->SetString("fileFullPath", "/" + results->at(i).path.value());
    entry->SetBoolean("fileIsDirectory",
                      results->at(i).entry.file_info().is_directory());
    entries->Append(entry);
  }

  base::DictionaryValue* result = new DictionaryValue();
  result->Set("entries", entries);
  result->SetString("nextFeed", next_feed);

  SetResult(result);
  SendResponse(true);
}

SearchDriveMetadataFunction::SearchDriveMetadataFunction() {
}

SearchDriveMetadataFunction::~SearchDriveMetadataFunction() {
}

bool SearchDriveMetadataFunction::RunImpl() {
  DictionaryValue* search_params;
  if (!args_->GetDictionary(0, &search_params))
    return false;

  std::string query;
  if (!search_params->GetString("query", &query))
    return false;

  std::string types;
  if (!search_params->GetString("types", &types))
    return false;

  int max_results = 0;
  if (!search_params->GetInteger("maxResults", &max_results))
    return false;

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (types: '%s', maxResults: '%d')",
                   name().c_str(),
                   request_id(),
                   types.c_str(),
                   max_results);
  set_log_on_completion(true);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service || !integration_service->file_system())
    return false;

  int options = drive::SEARCH_METADATA_ALL;
  // TODO(hirono): Switch to the JSON scheme compiler. http://crbug.com/241693
  if (types == "EXCLUDE_DIRECTORIES")
    options = drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES;
  else if (types == "SHARED_WITH_ME")
    options = drive::SEARCH_METADATA_SHARED_WITH_ME;
  else if (types == "OFFLINE")
    options = drive::SEARCH_METADATA_OFFLINE;
  else
    DCHECK_EQ("ALL", types);

  integration_service->file_system()->SearchMetadata(
      query,
      options,
      max_results,
      base::Bind(&SearchDriveMetadataFunction::OnSearchMetadata, this));
  return true;
}

void SearchDriveMetadataFunction::OnSearchMetadata(
    drive::FileError error,
    scoped_ptr<drive::MetadataSearchResultVector> results) {
  if (error != drive::FILE_ERROR_OK) {
    SendResponse(false);
    return;
  }

  DCHECK(results.get());

  base::ListValue* results_list = new ListValue();

  // Convert Drive files to something File API stack can understand.  See
  // file_browser_handler_custom_bindings.cc and
  // file_browser_private_custom_bindings.js for how this is magically
  // converted to a FileEntry.
  GURL origin_url = source_url_.GetOrigin();
  fileapi::FileSystemType file_system_type = fileapi::kFileSystemTypeExternal;
  GURL file_system_root_url =
      fileapi::GetFileSystemRootURI(origin_url, file_system_type);
  std::string file_system_name =
      fileapi::GetFileSystemName(origin_url, file_system_type);
  for (size_t i = 0; i < results->size(); ++i) {
    DictionaryValue* result_dict = new DictionaryValue();

    // FileEntry fields.
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("fileSystemName", file_system_name);
    entry->SetString("fileSystemRoot", file_system_root_url.spec());
    entry->SetString("fileFullPath", "/" + results->at(i).path.value());
    entry->SetBoolean("fileIsDirectory",
                      results->at(i).entry.file_info().is_directory());

    result_dict->Set("entry", entry);
    result_dict->SetString("highlightedBaseName",
                           results->at(i).highlighted_base_name);
    results_list->Append(result_dict);
  }

  SetResult(results_list);
  SendResponse(true);
}

ClearDriveCacheFunction::ClearDriveCacheFunction() {
}

ClearDriveCacheFunction::~ClearDriveCacheFunction() {
}

bool ClearDriveCacheFunction::RunImpl() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service || !integration_service->file_system())
    return false;

  // TODO(yoshiki): Receive a callback from JS-side and pass it to
  // ClearCacheAndRemountFileSystem(). http://crbug.com/140511
  integration_service->ClearCacheAndRemountFileSystem(
      base::Bind(&DoNothingWithBool));

  SendResponse(true);
  return true;
}

GetDriveConnectionStateFunction::GetDriveConnectionStateFunction() {
}

GetDriveConnectionStateFunction::~GetDriveConnectionStateFunction() {
}

bool GetDriveConnectionStateFunction::RunImpl() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_ptr<ListValue> reasons(new ListValue());

  std::string type_string;
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);

  bool ready = integration_service &&
      integration_service->drive_service()->CanSendRequest();
  bool is_connection_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());

  if (net::NetworkChangeNotifier::IsOffline() || !ready) {
    type_string = kDriveConnectionTypeOffline;
    if (net::NetworkChangeNotifier::IsOffline())
      reasons->AppendString(kDriveConnectionReasonNoNetwork);
    if (!ready)
      reasons->AppendString(kDriveConnectionReasonNotReady);
    if (!integration_service)
      reasons->AppendString(kDriveConnectionReasonNoService);
  } else if (
      is_connection_cellular &&
      profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular)) {
    type_string = kDriveConnectionTypeMetered;
  } else {
    type_string = kDriveConnectionTypeOnline;
  }

  value->SetString("type", type_string);
  value->Set("reasons", reasons.release());
  SetResult(value.release());

  drive::util::Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

RequestAccessTokenFunction::RequestAccessTokenFunction() {
}

RequestAccessTokenFunction::~RequestAccessTokenFunction() {
}

bool RequestAccessTokenFunction::RunImpl() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  bool refresh;
  args_->GetBoolean(0, &refresh);

  if (!integration_service) {
    SetResult(new base::StringValue(""));
    SendResponse(true);
    return true;
  }

  // If refreshing is requested, then clear the token to refetch it.
  if (refresh)
    integration_service->drive_service()->ClearAccessToken();

  // Retrieve the cached auth token (if available), otherwise the AuthService
  // instance will try to refetch it.
  integration_service->drive_service()->RequestAccessToken(
      base::Bind(&RequestAccessTokenFunction::OnAccessTokenFetched, this));
  return true;
}

void RequestAccessTokenFunction::OnAccessTokenFetched(
    google_apis::GDataErrorCode code, const std::string& access_token) {
  SetResult(new base::StringValue(access_token));
  SendResponse(true);
}

GetShareUrlFunction::GetShareUrlFunction() {
}

GetShareUrlFunction::~GetShareUrlFunction() {
}

bool GetShareUrlFunction::RunImpl() {
  std::string file_url;
  if (!args_->GetString(0, &file_url))
    return false;

  const base::FilePath path = util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(file_url));
  DCHECK(drive::util::IsUnderDriveMountPoint(path));

  base::FilePath drive_path = drive::util::ExtractDrivePath(path);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  // |integration_service| is NULL if Drive is disabled.
  if (!integration_service)
    return false;

  integration_service->file_system()->GetShareUrl(
      drive_path,
      util::GetFileManagerBaseUrl(),  // embed origin
      base::Bind(&GetShareUrlFunction::OnGetShareUrl, this));
  return true;
}


void GetShareUrlFunction::OnGetShareUrl(drive::FileError error,
                                        const GURL& share_url) {
  if (error != drive::FILE_ERROR_OK) {
    error_ = "Share Url for this item is not available.";
    SendResponse(false);
    return;
  }

  SetResult(new base::StringValue(share_url.spec()));
  SendResponse(true);
}

}  // namespace file_manager
