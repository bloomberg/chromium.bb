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
#include "chrome/common/extensions/api/file_browser_private.h"
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

// Copies properties from |entry_proto| to |properties|.
void FillDriveEntryPropertiesValue(
    const drive::ResourceEntry& entry_proto,
    api::file_browser_private::DriveEntryProperties* properties) {
  properties->shared_with_me.reset(new bool(entry_proto.shared_with_me()));

  if (!entry_proto.has_file_specific_info())
    return;

  const drive::FileSpecificInfo& file_specific_info =
      entry_proto.file_specific_info();

  properties->thumbnail_url.reset(
      new std::string("https://www.googledrive.com/thumb/" +
          entry_proto.resource_id() + "?width=500&height=500"));
  properties->is_hosted.reset(
      new bool(file_specific_info.is_hosted_document()));
  properties->content_mime_type.reset(
      new std::string(file_specific_info.content_mime_type()));
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

  using extensions::api::file_browser_private::GetDriveEntryProperties::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL file_url = GURL(params->file_url);
  file_path_ = drive::util::ExtractDrivePath(
      file_manager::util::GetLocalPathFromURL(
          render_view_host(), profile(), file_url));

  properties_.reset(new extensions::api::file_browser_private::
                    DriveEntryProperties);

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
        properties_->custom_icon_url.reset(new std::string(doc_icon.spec()));
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
  properties_->is_pinned.reset(new bool(cache_entry.is_pinned()));
  properties_->is_present.reset(new bool(cache_entry.is_present()));

  CompleteGetFileProperties(drive::FILE_ERROR_OK);
}

void FileBrowserPrivateGetDriveEntryPropertiesFunction::
    CompleteGetFileProperties(drive::FileError error) {
  results_ = extensions::api::file_browser_private::GetDriveEntryProperties::
      Results::Create(*properties_);
  SendResponse(true);
}

bool FileBrowserPrivatePinDriveFileFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using extensions::api::file_browser_private::PinDriveFile::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  const base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_manager::util::GetLocalPathFromURL(
          render_view_host(), profile(), GURL(params->file_url)));
  if (params->pin) {
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
    FileBrowserPrivateGetDriveFilesFunction() {
}

FileBrowserPrivateGetDriveFilesFunction::
    ~FileBrowserPrivateGetDriveFilesFunction() {
}

bool FileBrowserPrivateGetDriveFilesFunction::RunImpl() {
  using extensions::api::file_browser_private::GetDriveFiles::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert the list of strings to a list of GURLs.
  for (size_t i = 0; i < params->file_urls.size(); ++i) {
    const base::FilePath path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), profile(), GURL(params->file_urls[i]));
    DCHECK(drive::util::IsUnderDriveMountPoint(path));
    base::FilePath drive_path = drive::util::ExtractDrivePath(path);
    remaining_drive_paths_.push(drive_path);
  }

  GetFileOrSendResponse();
  return true;
}

void FileBrowserPrivateGetDriveFilesFunction::GetFileOrSendResponse() {
  // Send the response if all files are obtained.
  if (remaining_drive_paths_.empty()) {
    results_ = extensions::api::file_browser_private::
        GetDriveFiles::Results::Create(local_paths_);
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
    local_paths_.push_back(local_path.AsUTF8Unsafe());
    DVLOG(1) << "Got " << drive_path.value() << " as " << local_path.value();

    // TODO(benchan): If the file is a hosted document, a temporary JSON file
    // is created to represent the document. The JSON file is not cached and
    // should be deleted after use. We need to somehow communicate with
    // file_manager.js to manage the lifetime of the temporary file.
    // See crosbug.com/28058.
  } else {
    local_paths_.push_back("");
    DVLOG(1) << "Failed to get " << drive_path.value()
             << " with error code: " << error;
  }

  remaining_drive_paths_.pop();

  // Start getting the next file.
  GetFileOrSendResponse();
}

bool FileBrowserPrivateCancelFileTransfersFunction::RunImpl() {
  using extensions::api::file_browser_private::CancelFileTransfers::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

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
  std::vector<linked_ptr<api::file_browser_private::
                         FileTransferCancelStatus> > responses;
  for (size_t i = 0; i < params->file_urls.size(); ++i) {
    base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), profile(), GURL(params->file_urls[i]));
    if (file_path.empty())
      continue;

    DCHECK(drive::util::IsUnderDriveMountPoint(file_path));
    file_path = drive::util::ExtractDrivePath(file_path);

    // Cancel all the jobs for the file.
    PathToIdMap::iterator it = path_to_id_map.find(file_path);
    if (it != path_to_id_map.end()) {
      for (size_t i = 0; i < it->second.size(); ++i)
        job_list->CancelJob(it->second[i]);
    }
    linked_ptr<api::file_browser_private::FileTransferCancelStatus> result(
        new api::file_browser_private::FileTransferCancelStatus);
    result->canceled = it != path_to_id_map.end();
    // TODO(kinaba): simplify cancelFileTransfer() to take single URL each time,
    // and eliminate this field; it is just returning a copy of the argument.
    result->file_url = params->file_urls[i];
    responses.push_back(result);
  }
  results_ = api::file_browser_private::CancelFileTransfers::Results::Create(
      responses);
  SendResponse(true);
  return true;
}

bool FileBrowserPrivateSearchDriveFunction::RunImpl() {
  using extensions::api::file_browser_private::SearchDrive::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  file_system->Search(
      params->search_params.query, GURL(params->search_params.next_feed),
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

bool FileBrowserPrivateSearchDriveMetadataFunction::RunImpl() {
  using extensions::api::file_browser_private::SearchDriveMetadata::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (types: '%s', maxResults: '%d')",
                   name().c_str(),
                   request_id(),
                   Params::SearchParams::ToString(
                       params->search_params.types).c_str(),
                   params->search_params.max_results);
  set_log_on_completion(true);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(profile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  int options = -1;
  switch (params->search_params.types) {
    case Params::SearchParams::TYPES_EXCLUDE_DIRECTORIES:
      options = drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES;
      break;
    case Params::SearchParams::TYPES_SHARED_WITH_ME:
      options = drive::SEARCH_METADATA_SHARED_WITH_ME;
      break;
    case Params::SearchParams::TYPES_OFFLINE:
      options = drive::SEARCH_METADATA_OFFLINE;
      break;
    case Params::SearchParams::TYPES_ALL:
      options = drive::SEARCH_METADATA_ALL;
      break;
    case Params::SearchParams::TYPES_NONE:
      break;
  }
  DCHECK_NE(options, -1);

  file_system->SearchMetadata(
      params->search_params.query,
      options,
      params->search_params.max_results,
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

bool FileBrowserPrivateGetDriveConnectionStateFunction::RunImpl() {
  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(profile());

  api::file_browser_private::GetDriveConnectionState::Results::Result result;

  const bool ready = drive_service && drive_service->CanSendRequest();
  const bool is_connection_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());

  if (net::NetworkChangeNotifier::IsOffline() || !ready) {
    result.type = kDriveConnectionTypeOffline;
    if (net::NetworkChangeNotifier::IsOffline())
      result.reasons.push_back(kDriveConnectionReasonNoNetwork);
    if (!ready)
      result.reasons.push_back(kDriveConnectionReasonNotReady);
    if (!drive_service)
      result.reasons.push_back(kDriveConnectionReasonNoService);
  } else if (
      is_connection_cellular &&
      profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular)) {
    result.type = kDriveConnectionTypeMetered;
  } else {
    result.type = kDriveConnectionTypeOnline;
  }

  results_ = api::file_browser_private::GetDriveConnectionState::Results::
      Create(result);

  drive::util::Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

bool FileBrowserPrivateRequestAccessTokenFunction::RunImpl() {
  using extensions::api::file_browser_private::RequestAccessToken::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(profile());

  if (!drive_service) {
    // DriveService is not available.
    SetResult(new base::StringValue(""));
    SendResponse(true);
    return true;
  }

  // If refreshing is requested, then clear the token to refetch it.
  if (params->refresh)
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

bool FileBrowserPrivateGetShareUrlFunction::RunImpl() {
  using extensions::api::file_browser_private::GetShareUrl::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(params->url));
  DCHECK(drive::util::IsUnderDriveMountPoint(path));

  const base::FilePath drive_path = drive::util::ExtractDrivePath(path);

  drive::FileSystemInterface* const file_system =
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
