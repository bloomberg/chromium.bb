// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"

#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive_app_registry.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::BrowserThread;

namespace extensions {
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

FileBrowserPrivateGetDriveEntryPropertiesFunction::
    FileBrowserPrivateGetDriveEntryPropertiesFunction() {
}

FileBrowserPrivateGetDriveEntryPropertiesFunction::
    ~FileBrowserPrivateGetDriveEntryPropertiesFunction() {
}

bool FileBrowserPrivateGetDriveEntryPropertiesFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string file_url_str;
  if (args_->GetSize() != 1 || !args_->GetString(0, &file_url_str))
    return false;

  GURL file_url = GURL(file_url_str);
  file_path_ = drive::util::ExtractDrivePath(
      file_manager::util::GetLocalPathFromURL(
          render_view_host(), profile(), file_url));

  properties_.reset(new base::DictionaryValue);

  // Start getting the file info.
  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled or not mounted.
    CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
    return true;
  }

  file_system->GetResourceEntryByPath(
      file_path_,
      base::Bind(&FileBrowserPrivateGetDriveEntryPropertiesFunction::
                     OnGetFileInfo, this));
  return true;
}

void FileBrowserPrivateGetDriveEntryPropertiesFunction::OnGetFileInfo(
    drive::FileError error,
    scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(properties_);

  if (error != drive::FILE_ERROR_OK) {
    CompleteGetFileProperties(error);
    return;
  }
  DCHECK(entry);

  FillDriveEntryPropertiesValue(*entry, properties_.get());

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile_);
  drive::DriveAppRegistry* app_registry =
      drive::util::GetDriveAppRegistryByProfile(profile_);
  if (!file_system || !app_registry) {
    // |file_system| or |app_registry| is NULL if Drive is disabled.
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
  app_registry->GetAppsForFile(file_path_.Extension(),
                               file_specific_info.content_mime_type(),
                               &drive_apps);
  if (!drive_apps.empty()) {
    std::string default_task_id =
        file_manager::file_tasks::GetDefaultTaskIdFromPrefs(
            *profile_->GetPrefs(),
            file_specific_info.content_mime_type(),
            file_path_.Extension());
    file_manager::file_tasks::TaskDescriptor default_task;
    file_manager::file_tasks::ParseTaskID(default_task_id, &default_task);
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

  file_system->GetCacheEntryByPath(
      file_path_,
      base::Bind(&FileBrowserPrivateGetDriveEntryPropertiesFunction::
                     CacheStateReceived, this));
}

void FileBrowserPrivateGetDriveEntryPropertiesFunction::CacheStateReceived(
    bool /* success */,
    const drive::FileCacheEntry& cache_entry) {
  // In case of an error (i.e. success is false), cache_entry.is_*() all
  // returns false.
  properties_->SetBoolean("isPinned", cache_entry.is_pinned());
  properties_->SetBoolean("isPresent", cache_entry.is_present());

  CompleteGetFileProperties(drive::FILE_ERROR_OK);
}

void FileBrowserPrivateGetDriveEntryPropertiesFunction::
    CompleteGetFileProperties(drive::FileError error) {
  SetResult(properties_.release());
  SendResponse(true);
}

FileBrowserPrivatePinDriveFileFunction::
    FileBrowserPrivatePinDriveFileFunction() {
}

FileBrowserPrivatePinDriveFileFunction::
    ~FileBrowserPrivatePinDriveFileFunction() {
}

bool FileBrowserPrivatePinDriveFileFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string url;
  bool set_pin = false;
  if (args_->GetSize() != 2 ||
      !args_->GetString(0, &url) ||
      !args_->GetBoolean(1, &set_pin))
    return false;

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_manager::util::GetLocalPathFromURL(
          render_view_host(), profile(), GURL(url)));
  if (set_pin) {
    file_system->Pin(drive_path,
                     base::Bind(&FileBrowserPrivatePinDriveFileFunction::
                                    OnPinStateSet, this));
  } else {
    file_system->Unpin(drive_path,
                       base::Bind(&FileBrowserPrivatePinDriveFileFunction::
                                      OnPinStateSet, this));
  }
  return true;
}

void FileBrowserPrivatePinDriveFileFunction::
    OnPinStateSet(drive::FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == drive::FILE_ERROR_OK) {
    SendResponse(true);
  } else {
    error_ = drive::FileErrorToString(error);
    SendResponse(false);
  }
}

FileBrowserPrivateGetDriveFilesFunction::
    FileBrowserPrivateGetDriveFilesFunction() : local_paths_(NULL) {
}

FileBrowserPrivateGetDriveFilesFunction::
    ~FileBrowserPrivateGetDriveFilesFunction() {
}

bool FileBrowserPrivateGetDriveFilesFunction::RunImpl() {
  ListValue* file_urls_as_strings = NULL;
  if (!args_->GetList(0, &file_urls_as_strings))
    return false;

  // Convert the list of strings to a list of GURLs.
  for (size_t i = 0; i < file_urls_as_strings->GetSize(); ++i) {
    std::string file_url_as_string;
    if (!file_urls_as_strings->GetString(i, &file_url_as_string))
      return false;
    const base::FilePath path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), profile(), GURL(file_url_as_string));
    DCHECK(drive::util::IsUnderDriveMountPoint(path));
    base::FilePath drive_path = drive::util::ExtractDrivePath(path);
    remaining_drive_paths_.push(drive_path);
  }

  local_paths_ = new ListValue;
  GetFileOrSendResponse();
  return true;
}

void FileBrowserPrivateGetDriveFilesFunction::GetFileOrSendResponse() {
  // Send the response if all files are obtained.
  if (remaining_drive_paths_.empty()) {
    SetResult(local_paths_);
    SendResponse(true);
    return;
  }

  // Get the file on the top of the queue.
  base::FilePath drive_path = remaining_drive_paths_.front();

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled or not mounted.
    OnFileReady(drive::FILE_ERROR_FAILED, drive_path,
                scoped_ptr<drive::ResourceEntry>());
    return;
  }

  file_system->GetFileByPath(
      drive_path,
      base::Bind(&FileBrowserPrivateGetDriveFilesFunction::OnFileReady, this));
}


void FileBrowserPrivateGetDriveFilesFunction::OnFileReady(
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

FileBrowserPrivateCancelFileTransfersFunction::
    FileBrowserPrivateCancelFileTransfersFunction() {
}

FileBrowserPrivateCancelFileTransfersFunction::
    ~FileBrowserPrivateCancelFileTransfersFunction() {
}

bool FileBrowserPrivateCancelFileTransfersFunction::RunImpl() {
  ListValue* url_list = NULL;
  if (!args_->GetList(0, &url_list))
    return false;

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!integration_service || !integration_service->IsMounted())
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

    base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
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

FileBrowserPrivateSearchDriveFunction::
    FileBrowserPrivateSearchDriveFunction() {
}

FileBrowserPrivateSearchDriveFunction::
    ~FileBrowserPrivateSearchDriveFunction() {
}

bool FileBrowserPrivateSearchDriveFunction::RunImpl() {
  DictionaryValue* search_params;
  if (!args_->GetDictionary(0, &search_params))
    return false;

  std::string query;
  if (!search_params->GetString("query", &query))
    return false;

  std::string next_feed;
  if (!search_params->GetString("nextFeed", &next_feed))
    return false;

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  file_system->Search(
      query, GURL(next_feed),
      base::Bind(&FileBrowserPrivateSearchDriveFunction::OnSearch, this));
  return true;
}

void FileBrowserPrivateSearchDriveFunction::OnSearch(
    drive::FileError error,
    const GURL& next_link,
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
    entry->SetBoolean("fileIsDirectory", results->at(i).is_directory);
    entries->Append(entry);
  }

  base::DictionaryValue* result = new DictionaryValue();
  result->Set("entries", entries);
  result->SetString("nextFeed", next_link.spec());

  SetResult(result);
  SendResponse(true);
}

FileBrowserPrivateSearchDriveMetadataFunction::
    FileBrowserPrivateSearchDriveMetadataFunction() {
}

FileBrowserPrivateSearchDriveMetadataFunction::
    ~FileBrowserPrivateSearchDriveMetadataFunction() {
}

bool FileBrowserPrivateSearchDriveMetadataFunction::RunImpl() {
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

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

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

  file_system->SearchMetadata(
      query,
      options,
      max_results,
      base::Bind(&FileBrowserPrivateSearchDriveMetadataFunction::
                     OnSearchMetadata, this));
  return true;
}

void FileBrowserPrivateSearchDriveMetadataFunction::OnSearchMetadata(
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

FileBrowserPrivateClearDriveCacheFunction::
    FileBrowserPrivateClearDriveCacheFunction() {
}

FileBrowserPrivateClearDriveCacheFunction::
    ~FileBrowserPrivateClearDriveCacheFunction() {
}

bool FileBrowserPrivateClearDriveCacheFunction::RunImpl() {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!integration_service || !integration_service->IsMounted())
    return false;

  // TODO(yoshiki): Receive a callback from JS-side and pass it to
  // ClearCacheAndRemountFileSystem(). http://crbug.com/140511
  integration_service->ClearCacheAndRemountFileSystem(
      base::Bind(&DoNothingWithBool));

  SendResponse(true);
  return true;
}

FileBrowserPrivateGetDriveConnectionStateFunction::
    FileBrowserPrivateGetDriveConnectionStateFunction() {
}

FileBrowserPrivateGetDriveConnectionStateFunction::
    ~FileBrowserPrivateGetDriveConnectionStateFunction() {
}

bool FileBrowserPrivateGetDriveConnectionStateFunction::RunImpl() {
  drive::DriveServiceInterface* drive_service =
      drive::util::GetDriveServiceByProfile(profile());

  bool ready = drive_service && drive_service->CanSendRequest();
  bool is_connection_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());

  std::string type_string;
  scoped_ptr<ListValue> reasons(new ListValue());
  if (net::NetworkChangeNotifier::IsOffline() || !ready) {
    type_string = kDriveConnectionTypeOffline;
    if (net::NetworkChangeNotifier::IsOffline())
      reasons->AppendString(kDriveConnectionReasonNoNetwork);
    if (!ready)
      reasons->AppendString(kDriveConnectionReasonNotReady);
    if (!drive_service)
      reasons->AppendString(kDriveConnectionReasonNoService);
  } else if (
      is_connection_cellular &&
      profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular)) {
    type_string = kDriveConnectionTypeMetered;
  } else {
    type_string = kDriveConnectionTypeOnline;
  }

  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("type", type_string);
  value->Set("reasons", reasons.release());
  SetResult(value.release());

  drive::util::Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

FileBrowserPrivateRequestAccessTokenFunction::
    FileBrowserPrivateRequestAccessTokenFunction() {
}

FileBrowserPrivateRequestAccessTokenFunction::
    ~FileBrowserPrivateRequestAccessTokenFunction() {
}

bool FileBrowserPrivateRequestAccessTokenFunction::RunImpl() {
  bool refresh;
  if (!args_->GetBoolean(0, &refresh))
    return false;

  drive::DriveServiceInterface* drive_service =
      drive::util::GetDriveServiceByProfile(profile());

  if (!drive_service) {
    // DriveService is not available.
    SetResult(new base::StringValue(""));
    SendResponse(true);
    return true;
  }

  // If refreshing is requested, then clear the token to refetch it.
  if (refresh)
    drive_service->ClearAccessToken();

  // Retrieve the cached auth token (if available), otherwise the AuthService
  // instance will try to refetch it.
  drive_service->RequestAccessToken(
      base::Bind(&FileBrowserPrivateRequestAccessTokenFunction::
                      OnAccessTokenFetched, this));
  return true;
}

void FileBrowserPrivateRequestAccessTokenFunction::OnAccessTokenFetched(
    google_apis::GDataErrorCode code,
    const std::string& access_token) {
  SetResult(new base::StringValue(access_token));
  SendResponse(true);
}

FileBrowserPrivateGetShareUrlFunction::FileBrowserPrivateGetShareUrlFunction() {
}

FileBrowserPrivateGetShareUrlFunction::
    ~FileBrowserPrivateGetShareUrlFunction() {
}

bool FileBrowserPrivateGetShareUrlFunction::RunImpl() {
  std::string file_url;
  if (!args_->GetString(0, &file_url))
    return false;

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(file_url));
  DCHECK(drive::util::IsUnderDriveMountPoint(path));

  base::FilePath drive_path = drive::util::ExtractDrivePath(path);

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  file_system->GetShareUrl(
      drive_path,
      file_manager::util::GetFileManagerBaseUrl(),  // embed origin
      base::Bind(&FileBrowserPrivateGetShareUrlFunction::OnGetShareUrl, this));
  return true;
}

void FileBrowserPrivateGetShareUrlFunction::OnGetShareUrl(
    drive::FileError error,
    const GURL& share_url) {
  if (error != drive::FILE_ERROR_OK) {
    error_ = "Share Url for this item is not available.";
    SendResponse(false);
    return;
  }

  SetResult(new base::StringValue(share_url.spec()));
  SendResponse(true);
}

}  // namespace extensions
