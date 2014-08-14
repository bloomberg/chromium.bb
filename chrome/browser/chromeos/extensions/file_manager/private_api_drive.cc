// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/drive/event_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/auth_service.h"
#include "webkit/common/fileapi/file_system_info.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::BrowserThread;

using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionCallback;
using file_manager::util::EntryDefinitionList;
using file_manager::util::EntryDefinitionListCallback;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;
using extensions::api::file_browser_private::DriveEntryProperties;

namespace extensions {
namespace {

// List of connection types of drive.
// Keep this in sync with the DriveConnectionType in common/js/util.js.
const char kDriveConnectionTypeOffline[] = "offline";
const char kDriveConnectionTypeMetered[] = "metered";
const char kDriveConnectionTypeOnline[] = "online";

// List of reasons of kDriveConnectionType*.
// Keep this in sync with the DriveConnectionReason in common/js/util.js.
const char kDriveConnectionReasonNotReady[] = "not_ready";
const char kDriveConnectionReasonNoNetwork[] = "no_network";
const char kDriveConnectionReasonNoService[] = "no_service";

// Copies properties from |entry_proto| to |properties|. |shared_with_me| is
// given from the running profile.
void FillDriveEntryPropertiesValue(const drive::ResourceEntry& entry_proto,
                                   bool shared_with_me,
                                   DriveEntryProperties* properties) {
  properties->shared_with_me.reset(new bool(shared_with_me));
  properties->shared.reset(new bool(entry_proto.shared()));

  const drive::PlatformFileInfoProto& file_info = entry_proto.file_info();
  properties->file_size.reset(new double(file_info.size()));
  properties->last_modified_time.reset(new double(
      base::Time::FromInternalValue(file_info.last_modified()).ToJsTime()));

  if (!entry_proto.has_file_specific_info())
    return;

  const drive::FileSpecificInfo& file_specific_info =
      entry_proto.file_specific_info();

  if (!entry_proto.resource_id().empty()) {
    properties->thumbnail_url.reset(
        new std::string("https://www.googledrive.com/thumb/" +
                        entry_proto.resource_id() + "?width=500&height=500"));
  }
  if (file_specific_info.has_image_width()) {
    properties->image_width.reset(
        new int(file_specific_info.image_width()));
  }
  if (file_specific_info.has_image_height()) {
    properties->image_height.reset(
        new int(file_specific_info.image_height()));
  }
  if (file_specific_info.has_image_rotation()) {
    properties->image_rotation.reset(
        new int(file_specific_info.image_rotation()));
  }
  properties->is_hosted.reset(
      new bool(file_specific_info.is_hosted_document()));
  properties->content_mime_type.reset(
      new std::string(file_specific_info.content_mime_type()));

  properties->is_pinned.reset(
      new bool(file_specific_info.cache_state().is_pinned()));
  properties->is_present.reset(
      new bool(file_specific_info.cache_state().is_present()));
}

// Creates entry definition list for (metadata) search result info list.
template <class T>
void ConvertSearchResultInfoListToEntryDefinitionList(
    Profile* profile,
    const std::string& extension_id,
    const std::vector<T>& search_result_info_list,
    const EntryDefinitionListCallback& callback) {
  FileDefinitionList file_definition_list;

  for (size_t i = 0; i < search_result_info_list.size(); ++i) {
    FileDefinition file_definition;
    file_definition.virtual_path =
        file_manager::util::ConvertDrivePathToRelativeFileSystemPath(
            profile, extension_id, search_result_info_list.at(i).path);
    file_definition.is_directory = search_result_info_list.at(i).is_directory;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      profile,
      extension_id,
      file_definition_list,  // Safe, since copied internally.
      callback);
}

class SingleDriveEntryPropertiesGetter {
 public:
  typedef base::Callback<void(drive::FileError error)> ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const base::FilePath local_path,
                    linked_ptr<DriveEntryProperties> properties,
                    Profile* const profile,
                    const ResultCallback& callback) {

    SingleDriveEntryPropertiesGetter* instance =
        new SingleDriveEntryPropertiesGetter(
            local_path, properties, profile, callback);
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleDriveEntryPropertiesGetter() {}

 private:
  // Given parameters.
  const ResultCallback callback_;
  const base::FilePath local_path_;
  const linked_ptr<DriveEntryProperties> properties_;
  Profile* const running_profile_;

  // Values used in the process.
  Profile* file_owner_profile_;
  base::FilePath file_path_;
  scoped_ptr<drive::ResourceEntry> owner_resource_entry_;

  base::WeakPtrFactory<SingleDriveEntryPropertiesGetter> weak_ptr_factory_;

  SingleDriveEntryPropertiesGetter(const base::FilePath local_path,
                                   linked_ptr<DriveEntryProperties> properties,
                                   Profile* const profile,
                                   const ResultCallback& callback)
      : callback_(callback),
        local_path_(local_path),
        properties_(properties),
        running_profile_(profile),
        file_owner_profile_(NULL),
        weak_ptr_factory_(this) {
    DCHECK(!callback_.is_null());
    DCHECK(profile);
  }

  base::WeakPtr<SingleDriveEntryPropertiesGetter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartProcess() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    file_path_ = drive::util::ExtractDrivePath(local_path_);
    file_owner_profile_ = drive::util::ExtractProfileFromPath(local_path_);

    if (!file_owner_profile_ ||
        !g_browser_process->profile_manager()->IsValidProfile(
            file_owner_profile_)) {
      CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // Start getting the file info.
    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled or not mounted.
      CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    file_system->GetResourceEntry(
        file_path_,
        base::Bind(&SingleDriveEntryPropertiesGetter::OnGetFileInfo,
                   GetWeakPtr()));
  }

  void OnGetFileInfo(drive::FileError error,
                     scoped_ptr<drive::ResourceEntry> entry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetFileProperties(error);
      return;
    }

    DCHECK(entry);
    owner_resource_entry_.swap(entry);

    if (running_profile_->IsSameProfile(file_owner_profile_)) {
      StartParseFileInfo(owner_resource_entry_->shared_with_me());
      return;
    }

    // If the running profile does not own the file, obtain the shared_with_me
    // flag from the running profile's value.
    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(running_profile_);
    if (!file_system) {
      CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
      return;
    }
    file_system->GetPathFromResourceId(
        owner_resource_entry_->resource_id(),
        base::Bind(&SingleDriveEntryPropertiesGetter::OnGetRunningPath,
                   GetWeakPtr()));
  }

  void OnGetRunningPath(drive::FileError error,
                        const base::FilePath& file_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK) {
      // The running profile does not know the file.
      StartParseFileInfo(false);
      return;
    }

    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(running_profile_);
    if (!file_system) {
      // The drive is disable for the running profile.
      StartParseFileInfo(false);
      return;
    }

    file_system->GetResourceEntry(
        file_path,
        base::Bind(&SingleDriveEntryPropertiesGetter::OnGetShareInfo,
                   GetWeakPtr()));
  }

  void OnGetShareInfo(drive::FileError error,
                      scoped_ptr<drive::ResourceEntry> entry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetFileProperties(error);
      return;
    }

    DCHECK(entry);
    StartParseFileInfo(entry->shared_with_me());
  }

  void StartParseFileInfo(bool shared_with_me) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    FillDriveEntryPropertiesValue(
        *owner_resource_entry_, shared_with_me, properties_.get());

    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    drive::DriveAppRegistry* const app_registry =
        drive::util::GetDriveAppRegistryByProfile(file_owner_profile_);
    if (!file_system || !app_registry) {
      // |file_system| or |app_registry| is NULL if Drive is disabled.
      CompleteGetFileProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // The properties meaningful for directories are already filled in
    // FillDriveEntryPropertiesValue().
    if (!owner_resource_entry_->has_file_specific_info()) {
      CompleteGetFileProperties(drive::FILE_ERROR_OK);
      return;
    }

    const drive::FileSpecificInfo& file_specific_info =
        owner_resource_entry_->file_specific_info();

    // Get drive WebApps that can accept this file. We just need to extract the
    // doc icon for the drive app, which is set as default.
    std::vector<drive::DriveAppInfo> drive_apps;
    app_registry->GetAppsForFile(file_path_.Extension(),
                                 file_specific_info.content_mime_type(),
                                 &drive_apps);
    if (!drive_apps.empty()) {
      std::string default_task_id =
          file_manager::file_tasks::GetDefaultTaskIdFromPrefs(
              *file_owner_profile_->GetPrefs(),
              file_specific_info.content_mime_type(),
              file_path_.Extension());
      file_manager::file_tasks::TaskDescriptor default_task;
      file_manager::file_tasks::ParseTaskID(default_task_id, &default_task);
      DCHECK(default_task_id.empty() || !default_task.app_id.empty());
      for (size_t i = 0; i < drive_apps.size(); ++i) {
        const drive::DriveAppInfo& app_info = drive_apps[i];
        if (default_task.app_id == app_info.app_id) {
          // The drive app is set as default. Files.app should use the doc icon.
          const GURL doc_icon = drive::util::FindPreferredIcon(
              app_info.document_icons, drive::util::kPreferredIconSize);
          properties_->custom_icon_url.reset(new std::string(doc_icon.spec()));
        }
      }
    }

    CompleteGetFileProperties(drive::FILE_ERROR_OK);
  }

  void CompleteGetFileProperties(drive::FileError error) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback_.is_null());
    callback_.Run(error);

    delete this;
  }
};  // class SingleDriveEntryPropertiesGetter

}  // namespace

FileBrowserPrivateGetDriveEntryPropertiesFunction::
    FileBrowserPrivateGetDriveEntryPropertiesFunction()
    : processed_count_(0) {}

FileBrowserPrivateGetDriveEntryPropertiesFunction::
    ~FileBrowserPrivateGetDriveEntryPropertiesFunction() {}

bool FileBrowserPrivateGetDriveEntryPropertiesFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using api::file_browser_private::GetDriveEntryProperties::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  properties_list_.resize(params->file_urls.size());

  for (size_t i = 0; i < params->file_urls.size(); i++) {
    const GURL url = GURL(params->file_urls[i]);
    const base::FilePath local_path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), GetProfile(), url);
    properties_list_[i] = make_linked_ptr(new DriveEntryProperties);

    SingleDriveEntryPropertiesGetter::Start(
        local_path,
        properties_list_[i],
        GetProfile(),
        base::Bind(&FileBrowserPrivateGetDriveEntryPropertiesFunction::
                       CompleteGetFileProperties,
                   this));
  }

  return true;
}

void FileBrowserPrivateGetDriveEntryPropertiesFunction::
    CompleteGetFileProperties(drive::FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(0 <= processed_count_ && processed_count_ < properties_list_.size());

  processed_count_++;
  if (processed_count_ < properties_list_.size())
    return;

  results_ = extensions::api::file_browser_private::GetDriveEntryProperties::
      Results::Create(properties_list_);
  SendResponse(true);
}

bool FileBrowserPrivatePinDriveFileFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using extensions::api::file_browser_private::PinDriveFile::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  const base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_manager::util::GetLocalPathFromURL(
          render_view_host(), GetProfile(), GURL(params->file_url)));
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
    SetError(drive::FileErrorToString(error));
    SendResponse(false);
  }
}

FileBrowserPrivateGetDriveFilesFunction::
    FileBrowserPrivateGetDriveFilesFunction() {
}

FileBrowserPrivateGetDriveFilesFunction::
    ~FileBrowserPrivateGetDriveFilesFunction() {
}

bool FileBrowserPrivateGetDriveFilesFunction::RunAsync() {
  using extensions::api::file_browser_private::GetDriveFiles::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert the list of strings to a list of GURLs.
  for (size_t i = 0; i < params->file_urls.size(); ++i) {
    const base::FilePath path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), GetProfile(), GURL(params->file_urls[i]));
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
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled or not mounted.
    OnFileReady(drive::FILE_ERROR_FAILED, drive_path,
                scoped_ptr<drive::ResourceEntry>());
    return;
  }

  file_system->GetFile(
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
  } else {
    local_paths_.push_back("");
    DVLOG(1) << "Failed to get " << drive_path.value()
             << " with error code: " << error;
  }

  remaining_drive_paths_.pop();

  // Start getting the next file.
  GetFileOrSendResponse();
}

bool FileBrowserPrivateCancelFileTransfersFunction::RunAsync() {
  using extensions::api::file_browser_private::CancelFileTransfers::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
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
        render_view_host(), GetProfile(), GURL(params->file_urls[i]));
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

bool FileBrowserPrivateSearchDriveFunction::RunAsync() {
  using extensions::api::file_browser_private::SearchDrive::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
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
    scoped_ptr<SearchResultInfoList> results) {
  if (error != drive::FILE_ERROR_OK) {
    SendResponse(false);
    return;
  }

  // Outlives the following conversion, since the pointer is bound to the
  // callback.
  DCHECK(results.get());
  const SearchResultInfoList& results_ref = *results.get();

  ConvertSearchResultInfoListToEntryDefinitionList(
      GetProfile(),
      extension_->id(),
      results_ref,
      base::Bind(&FileBrowserPrivateSearchDriveFunction::OnEntryDefinitionList,
                 this,
                 next_link,
                 base::Passed(&results)));
}

void FileBrowserPrivateSearchDriveFunction::OnEntryDefinitionList(
    const GURL& next_link,
    scoped_ptr<SearchResultInfoList> search_result_info_list,
    scoped_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(search_result_info_list->size(), entry_definition_list->size());
  base::ListValue* entries = new base::ListValue();

  // Convert Drive files to something File API stack can understand.
  for (EntryDefinitionList::const_iterator it = entry_definition_list->begin();
       it != entry_definition_list->end();
       ++it) {
    base::DictionaryValue* entry = new base::DictionaryValue();
    entry->SetString("fileSystemName", it->file_system_name);
    entry->SetString("fileSystemRoot", it->file_system_root_url);
    entry->SetString("fileFullPath", "/" + it->full_path.AsUTF8Unsafe());
    entry->SetBoolean("fileIsDirectory", it->is_directory);
    entries->Append(entry);
  }

  base::DictionaryValue* result = new base::DictionaryValue();
  result->Set("entries", entries);
  result->SetString("nextFeed", next_link.spec());

  SetResult(result);
  SendResponse(true);
}

bool FileBrowserPrivateSearchDriveMetadataFunction::RunAsync() {
  using api::file_browser_private::SearchDriveMetadata::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger) {
    logger->Log(logging::LOG_INFO,
                "%s[%d] called. (types: '%s', maxResults: '%d')",
                name().c_str(),
                request_id(),
                api::file_browser_private::ToString(
                    params->search_params.types).c_str(),
                params->search_params.max_results);
  }
  set_log_on_completion(true);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  int options = -1;
  switch (params->search_params.types) {
    case api::file_browser_private::SEARCH_TYPE_EXCLUDE_DIRECTORIES:
      options = drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES;
      break;
    case api::file_browser_private::SEARCH_TYPE_SHARED_WITH_ME:
      options = drive::SEARCH_METADATA_SHARED_WITH_ME;
      break;
    case api::file_browser_private::SEARCH_TYPE_OFFLINE:
      options = drive::SEARCH_METADATA_OFFLINE;
      break;
    case api::file_browser_private::SEARCH_TYPE_ALL:
      options = drive::SEARCH_METADATA_ALL;
      break;
    case api::file_browser_private::SEARCH_TYPE_NONE:
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

  // Outlives the following conversion, since the pointer is bound to the
  // callback.
  DCHECK(results.get());
  const drive::MetadataSearchResultVector& results_ref = *results.get();

  ConvertSearchResultInfoListToEntryDefinitionList(
      GetProfile(),
      extension_->id(),
      results_ref,
      base::Bind(
          &FileBrowserPrivateSearchDriveMetadataFunction::OnEntryDefinitionList,
          this,
          base::Passed(&results)));
}

void FileBrowserPrivateSearchDriveMetadataFunction::OnEntryDefinitionList(
    scoped_ptr<drive::MetadataSearchResultVector> search_result_info_list,
    scoped_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(search_result_info_list->size(), entry_definition_list->size());
  base::ListValue* results_list = new base::ListValue();

  // Convert Drive files to something File API stack can understand.  See
  // file_browser_handler_custom_bindings.cc and
  // file_browser_private_custom_bindings.js for how this is magically
  // converted to a FileEntry.
  for (size_t i = 0; i < entry_definition_list->size(); ++i) {
    base::DictionaryValue* result_dict = new base::DictionaryValue();

    // FileEntry fields.
    base::DictionaryValue* entry = new base::DictionaryValue();
    entry->SetString(
        "fileSystemName", entry_definition_list->at(i).file_system_name);
    entry->SetString(
        "fileSystemRoot", entry_definition_list->at(i).file_system_root_url);
    entry->SetString(
        "fileFullPath",
        "/" + entry_definition_list->at(i).full_path.AsUTF8Unsafe());
    entry->SetBoolean("fileIsDirectory",
                      entry_definition_list->at(i).is_directory);

    result_dict->Set("entry", entry);
    result_dict->SetString(
        "highlightedBaseName",
        search_result_info_list->at(i).highlighted_base_name);
    results_list->Append(result_dict);
  }

  SetResult(results_list);
  SendResponse(true);
}

bool FileBrowserPrivateGetDriveConnectionStateFunction::RunSync() {
  api::file_browser_private::DriveConnectionState result;

  switch (drive::util::GetDriveConnectionStatus(GetProfile())) {
    case drive::util::DRIVE_DISCONNECTED_NOSERVICE:
      result.type = kDriveConnectionTypeOffline;
      result.reason.reset(new std::string(kDriveConnectionReasonNoService));
      break;
    case drive::util::DRIVE_DISCONNECTED_NONETWORK:
      result.type = kDriveConnectionTypeOffline;
      result.reason.reset(new std::string(kDriveConnectionReasonNoNetwork));
      break;
    case drive::util::DRIVE_DISCONNECTED_NOTREADY:
      result.type = kDriveConnectionTypeOffline;
      result.reason.reset(new std::string(kDriveConnectionReasonNotReady));
      break;
    case drive::util::DRIVE_CONNECTED_METERED:
      result.type = kDriveConnectionTypeMetered;
      break;
    case drive::util::DRIVE_CONNECTED:
      result.type = kDriveConnectionTypeOnline;
      break;
  }

  results_ = api::file_browser_private::GetDriveConnectionState::Results::
      Create(result);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger)
    logger->Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

bool FileBrowserPrivateRequestAccessTokenFunction::RunAsync() {
  using extensions::api::file_browser_private::RequestAccessToken::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(GetProfile());

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

bool FileBrowserPrivateGetShareUrlFunction::RunAsync() {
  using extensions::api::file_browser_private::GetShareUrl::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), GetProfile(), GURL(params->url));
  DCHECK(drive::util::IsUnderDriveMountPoint(path));

  const base::FilePath drive_path = drive::util::ExtractDrivePath(path);

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled.
    return false;
  }

  file_system->GetShareUrl(
      drive_path,
      GURL("chrome-extension://" + extension_id()),  // embed origin
      base::Bind(&FileBrowserPrivateGetShareUrlFunction::OnGetShareUrl, this));
  return true;
}

void FileBrowserPrivateGetShareUrlFunction::OnGetShareUrl(
    drive::FileError error,
    const GURL& share_url) {
  if (error != drive::FILE_ERROR_OK) {
    SetError("Share Url for this item is not available.");
    SendResponse(false);
    return;
  }

  SetResult(new base::StringValue(share_url.spec()));
  SendResponse(true);
}

bool FileBrowserPrivateRequestDriveShareFunction::RunAsync() {
  using extensions::api::file_browser_private::RequestDriveShare::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), GetProfile(), GURL(params->url));
  const base::FilePath drive_path = drive::util::ExtractDrivePath(path);
  Profile* const owner_profile = drive::util::ExtractProfileFromPath(path);

  if (!owner_profile)
    return false;

  drive::FileSystemInterface* const owner_file_system =
      drive::util::GetFileSystemByProfile(owner_profile);
  if (!owner_file_system)
    return false;

  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(GetProfile());
  if (!user || !user->is_logged_in())
    return false;

  google_apis::drive::PermissionRole role =
      google_apis::drive::PERMISSION_ROLE_READER;
  switch (params->share_type) {
    case api::file_browser_private::DRIVE_SHARE_TYPE_NONE:
      NOTREACHED();
      return false;
    case api::file_browser_private::DRIVE_SHARE_TYPE_CAN_EDIT:
      role = google_apis::drive::PERMISSION_ROLE_WRITER;
      break;
    case api::file_browser_private::DRIVE_SHARE_TYPE_CAN_COMMENT:
      role = google_apis::drive::PERMISSION_ROLE_COMMENTER;
      break;
    case api::file_browser_private::DRIVE_SHARE_TYPE_CAN_VIEW:
      role = google_apis::drive::PERMISSION_ROLE_READER;
      break;
  }

  // Share |drive_path| in |owner_file_system| to |user->email()|.
  owner_file_system->AddPermission(
      drive_path,
      user->email(),
      role,
      base::Bind(&FileBrowserPrivateRequestDriveShareFunction::OnAddPermission,
                 this));
  return true;
}

void FileBrowserPrivateRequestDriveShareFunction::OnAddPermission(
    drive::FileError error) {
  SendResponse(error == drive::FILE_ERROR_OK);
}

FileBrowserPrivateGetDownloadUrlFunction::
    FileBrowserPrivateGetDownloadUrlFunction() {
}

FileBrowserPrivateGetDownloadUrlFunction::
    ~FileBrowserPrivateGetDownloadUrlFunction() {
}

bool FileBrowserPrivateGetDownloadUrlFunction::RunAsync() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          chromeos::switches::kEnableVideoPlayerChromecastSupport)) {
    SetError("Cast support is disabled.");
    SetResult(new base::StringValue(""));  // Intentionally returns a blank.
    return false;
  }

  using extensions::api::file_browser_private::GetShareUrl::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Start getting the file info.
  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled or not mounted.
    SetError("Drive is disabled or not mounted.");
    SetResult(new base::StringValue(""));  // Intentionally returns a blank.
    return false;
  }

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), GetProfile(), GURL(params->url));
  if (!drive::util::IsUnderDriveMountPoint(path)) {
    SetError("The given file is not in Drive.");
    SetResult(new base::StringValue(""));  // Intentionally returns a blank.
    return false;
  }
  base::FilePath file_path = drive::util::ExtractDrivePath(path);

  file_system->GetResourceEntry(
      file_path,
      base::Bind(&FileBrowserPrivateGetDownloadUrlFunction::OnGetResourceEntry,
                 this));
  return true;
}

void FileBrowserPrivateGetDownloadUrlFunction::OnGetResourceEntry(
    drive::FileError error,
    scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != drive::FILE_ERROR_OK) {
    SetError("Download Url for this item is not available.");
    SetResult(new base::StringValue(""));  // Intentionally returns a blank.
    SendResponse(false);
    return;
  }

  download_url_ =
      google_apis::DriveApiUrlGenerator::kBaseDownloadUrlForProduction +
      entry->resource_id();

  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(GetProfile());
  const std::string& account_id = signin_manager->GetAuthenticatedAccountId();
  std::vector<std::string> scopes;
  scopes.push_back("https://www.googleapis.com/auth/drive.readonly");

  auth_service_.reset(
      new google_apis::AuthService(oauth2_token_service,
                                   account_id,
                                   GetProfile()->GetRequestContext(),
                                   scopes));
  auth_service_->StartAuthentication(base::Bind(
      &FileBrowserPrivateGetDownloadUrlFunction::OnTokenFetched, this));
}

void FileBrowserPrivateGetDownloadUrlFunction::OnTokenFetched(
    google_apis::GDataErrorCode code,
    const std::string& access_token) {
  if (code != google_apis::HTTP_SUCCESS) {
    SetError("Not able to fetch the token.");
    SetResult(new base::StringValue(""));  // Intentionally returns a blank.
    SendResponse(false);
    return;
  }

  const std::string url = download_url_ + "?access_token=" + access_token;
  SetResult(new base::StringValue(url));

  SendResponse(true);
}

}  // namespace extensions
