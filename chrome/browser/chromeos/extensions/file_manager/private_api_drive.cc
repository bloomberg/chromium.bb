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
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
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
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_util.h"

using content::BrowserThread;

using chromeos::file_system_provider::EntryMetadata;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::util::FileSystemURLParser;
using extensions::api::file_manager_private::EntryProperties;
using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionCallback;
using file_manager::util::EntryDefinitionList;
using file_manager::util::EntryDefinitionListCallback;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;

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
void FillEntryPropertiesValueForDrive(const drive::ResourceEntry& entry_proto,
                                      bool shared_with_me,
                                      EntryProperties* properties) {
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

  if (file_specific_info.cache_state().is_present()) {
    properties->is_available_offline.reset(new bool(true));
  } else if (file_specific_info.is_hosted_document() &&
             file_specific_info.has_document_extension()) {
    const std::string file_extension = file_specific_info.document_extension();
    // What's available offline? See the 'Web' column at:
    // http://support.google.com/drive/answer/1628467
    properties->is_available_offline.reset(
        new bool(file_extension == ".gdoc" || file_extension == ".gdraw" ||
                 file_extension == ".gsheet" || file_extension == ".gslides"));
  } else {
    properties->is_available_offline.reset(new bool(false));
  }

  properties->is_available_when_metered.reset(
      new bool(file_specific_info.cache_state().is_present() ||
               file_specific_info.is_hosted_document()));
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

class SingleEntryPropertiesGetterForDrive {
 public:
  typedef base::Callback<void(scoped_ptr<EntryProperties> properties,
                              base::File::Error error)> ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const base::FilePath local_path,
                    Profile* const profile,
                    const ResultCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    SingleEntryPropertiesGetterForDrive* instance =
        new SingleEntryPropertiesGetterForDrive(local_path, profile, callback);
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleEntryPropertiesGetterForDrive() {}

 private:
  SingleEntryPropertiesGetterForDrive(const base::FilePath local_path,
                                      Profile* const profile,
                                      const ResultCallback& callback)
      : callback_(callback),
        local_path_(local_path),
        running_profile_(profile),
        properties_(new EntryProperties),
        file_owner_profile_(NULL),
        weak_ptr_factory_(this) {
    DCHECK(!callback_.is_null());
    DCHECK(profile);
  }

  void StartProcess() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    file_path_ = drive::util::ExtractDrivePath(local_path_);
    file_owner_profile_ = drive::util::ExtractProfileFromPath(local_path_);

    if (!file_owner_profile_ ||
        !g_browser_process->profile_manager()->IsValidProfile(
            file_owner_profile_)) {
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // Start getting the file info.
    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled or not mounted.
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    file_system->GetResourceEntry(
        file_path_,
        base::Bind(&SingleEntryPropertiesGetterForDrive::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetFileInfo(drive::FileError error,
                     scoped_ptr<drive::ResourceEntry> entry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetEntryProperties(error);
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
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }
    file_system->GetPathFromResourceId(
        owner_resource_entry_->resource_id(),
        base::Bind(&SingleEntryPropertiesGetterForDrive::OnGetRunningPath,
                   weak_ptr_factory_.GetWeakPtr()));
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
        base::Bind(&SingleEntryPropertiesGetterForDrive::OnGetShareInfo,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetShareInfo(drive::FileError error,
                      scoped_ptr<drive::ResourceEntry> entry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetEntryProperties(error);
      return;
    }

    DCHECK(entry.get());
    StartParseFileInfo(entry->shared_with_me());
  }

  void StartParseFileInfo(bool shared_with_me) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    FillEntryPropertiesValueForDrive(
        *owner_resource_entry_, shared_with_me, properties_.get());

    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    drive::DriveAppRegistry* const app_registry =
        drive::util::GetDriveAppRegistryByProfile(file_owner_profile_);
    if (!file_system || !app_registry) {
      // |file_system| or |app_registry| is NULL if Drive is disabled.
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // The properties meaningful for directories are already filled in
    // FillEntryPropertiesValueForDrive().
    if (!owner_resource_entry_->has_file_specific_info()) {
      CompleteGetEntryProperties(drive::FILE_ERROR_OK);
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

    CompleteGetEntryProperties(drive::FILE_ERROR_OK);
  }

  void CompleteGetEntryProperties(drive::FileError error) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback_.is_null());

    callback_.Run(properties_.Pass(), drive::FileErrorToBaseFileError(error));
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  // Given parameters.
  const ResultCallback callback_;
  const base::FilePath local_path_;
  Profile* const running_profile_;

  // Values used in the process.
  scoped_ptr<EntryProperties> properties_;
  Profile* file_owner_profile_;
  base::FilePath file_path_;
  scoped_ptr<drive::ResourceEntry> owner_resource_entry_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDrive> weak_ptr_factory_;
};  // class SingleEntryPropertiesGetterForDrive

class SingleEntryPropertiesGetterForFileSystemProvider {
 public:
  typedef base::Callback<void(scoped_ptr<EntryProperties> properties,
                              base::File::Error error)> ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL file_system_url,
                    const ResultCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    SingleEntryPropertiesGetterForFileSystemProvider* instance =
        new SingleEntryPropertiesGetterForFileSystemProvider(file_system_url,
                                                             callback);
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleEntryPropertiesGetterForFileSystemProvider() {}

 private:
  SingleEntryPropertiesGetterForFileSystemProvider(
      const storage::FileSystemURL& file_system_url,
      const ResultCallback& callback)
      : callback_(callback),
        file_system_url_(file_system_url),
        properties_(new EntryProperties),
        weak_ptr_factory_(this) {
    DCHECK(!callback_.is_null());
  }

  void StartProcess() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    FileSystemURLParser parser(file_system_url_);
    if (!parser.Parse()) {
      CompleteGetEntryProperties(base::File::FILE_ERROR_NOT_FOUND);
      return;
    }

    parser.file_system()->GetMetadata(
        parser.file_path(),
        ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
        base::Bind(&SingleEntryPropertiesGetterForFileSystemProvider::
                       OnGetMetadataCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetMetadataCompleted(scoped_ptr<EntryMetadata> metadata,
                              base::File::Error result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (result != base::File::FILE_OK) {
      CompleteGetEntryProperties(result);
      return;
    }

    properties_->file_size.reset(new double(metadata->size));
    properties_->last_modified_time.reset(
        new double(metadata->modification_time.ToJsTime()));

    if (!metadata->thumbnail.empty())
      properties_->thumbnail_url.reset(new std::string(metadata->thumbnail));

    CompleteGetEntryProperties(base::File::FILE_OK);
  }

  void CompleteGetEntryProperties(base::File::Error result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!callback_.is_null());

    callback_.Run(properties_.Pass(), result);
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  // Given parameters.
  const ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;

  // Values used in the process.
  scoped_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForFileSystemProvider>
      weak_ptr_factory_;
};  // class SingleEntryPropertiesGetterForDrive

}  // namespace

FileManagerPrivateGetEntryPropertiesFunction::
    FileManagerPrivateGetEntryPropertiesFunction()
    : processed_count_(0) {
}

FileManagerPrivateGetEntryPropertiesFunction::
    ~FileManagerPrivateGetEntryPropertiesFunction() {
}

bool FileManagerPrivateGetEntryPropertiesFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using api::file_manager_private::GetEntryProperties::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  properties_list_.resize(params->file_urls.size());
  for (size_t i = 0; i < params->file_urls.size(); i++) {
    const GURL url = GURL(params->file_urls[i]);
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(url);
    switch (file_system_url.type()) {
      case storage::kFileSystemTypeDrive:
        SingleEntryPropertiesGetterForDrive::Start(
            file_system_url.path(),
            GetProfile(),
            base::Bind(&FileManagerPrivateGetEntryPropertiesFunction::
                           CompleteGetEntryProperties,
                       this,
                       i));
        break;
      case storage::kFileSystemTypeProvided:
        SingleEntryPropertiesGetterForFileSystemProvider::Start(
            file_system_url,
            base::Bind(&FileManagerPrivateGetEntryPropertiesFunction::
                           CompleteGetEntryProperties,
                       this,
                       i));
        break;
      default:
        LOG(ERROR) << "Not supported file system type.";
        CompleteGetEntryProperties(i,
                                   make_scoped_ptr(new EntryProperties),
                                   base::File::FILE_ERROR_INVALID_OPERATION);
    }
  }

  return true;
}

void FileManagerPrivateGetEntryPropertiesFunction::CompleteGetEntryProperties(
    size_t index,
    scoped_ptr<EntryProperties> properties,
    base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(0 <= processed_count_ && processed_count_ < properties_list_.size());

  properties_list_[index] = make_linked_ptr(properties.release());

  processed_count_++;
  if (processed_count_ < properties_list_.size())
    return;

  results_ = extensions::api::file_manager_private::GetEntryProperties::
      Results::Create(properties_list_);
  SendResponse(true);
}

bool FileManagerPrivatePinDriveFileFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using extensions::api::file_manager_private::PinDriveFile::Params;
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
                     base::Bind(&FileManagerPrivatePinDriveFileFunction::
                                    OnPinStateSet, this));
  } else {
    file_system->Unpin(drive_path,
                       base::Bind(&FileManagerPrivatePinDriveFileFunction::
                                      OnPinStateSet, this));
  }
  return true;
}

void FileManagerPrivatePinDriveFileFunction::
    OnPinStateSet(drive::FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == drive::FILE_ERROR_OK) {
    SendResponse(true);
  } else {
    SetError(drive::FileErrorToString(error));
    SendResponse(false);
  }
}

bool FileManagerPrivateCancelFileTransfersFunction::RunAsync() {
  using extensions::api::file_manager_private::CancelFileTransfers::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  if (!integration_service || !integration_service->IsMounted())
    return false;

  drive::JobListInterface* job_list = integration_service->job_list();
  DCHECK(job_list);
  std::vector<drive::JobInfo> jobs = job_list->GetJobInfoList();

  // If file_urls are empty, cancel all jobs.
  if (!params->file_urls.get()) {
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (drive::IsActiveFileTransferJobInfo(jobs[i]))
        job_list->CancelJob(jobs[i].job_id);
    }
  } else {
    // Create the mapping from file path to job ID.
    std::vector<std::string> file_urls(*params->file_urls.get());
    typedef std::map<base::FilePath, std::vector<drive::JobID> > PathToIdMap;
    PathToIdMap path_to_id_map;
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (drive::IsActiveFileTransferJobInfo(jobs[i]))
        path_to_id_map[jobs[i].file_path].push_back(jobs[i].job_id);
    }

    for (size_t i = 0; i < file_urls.size(); ++i) {
      base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
          render_view_host(), GetProfile(), GURL(file_urls[i]));
      if (file_path.empty())
        continue;

      file_path = drive::util::ExtractDrivePath(file_path);
      DCHECK(file_path.empty());

      // Cancel all the jobs for the file.
      PathToIdMap::iterator it = path_to_id_map.find(file_path);
      if (it != path_to_id_map.end()) {
        for (size_t i = 0; i < it->second.size(); ++i)
          job_list->CancelJob(it->second[i]);
      }
    }
  }
  SendResponse(true);
  return true;
}

bool FileManagerPrivateSearchDriveFunction::RunAsync() {
  using extensions::api::file_manager_private::SearchDrive::Params;
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
      base::Bind(&FileManagerPrivateSearchDriveFunction::OnSearch, this));
  return true;
}

void FileManagerPrivateSearchDriveFunction::OnSearch(
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
      base::Bind(&FileManagerPrivateSearchDriveFunction::OnEntryDefinitionList,
                 this,
                 next_link,
                 base::Passed(&results)));
}

void FileManagerPrivateSearchDriveFunction::OnEntryDefinitionList(
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

bool FileManagerPrivateSearchDriveMetadataFunction::RunAsync() {
  using api::file_manager_private::SearchDriveMetadata::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger) {
    logger->Log(logging::LOG_INFO,
                "%s[%d] called. (types: '%s', maxResults: '%d')",
                name().c_str(),
                request_id(),
                api::file_manager_private::ToString(
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
    case api::file_manager_private::SEARCH_TYPE_EXCLUDE_DIRECTORIES:
      options = drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES;
      break;
    case api::file_manager_private::SEARCH_TYPE_SHARED_WITH_ME:
      options = drive::SEARCH_METADATA_SHARED_WITH_ME;
      break;
    case api::file_manager_private::SEARCH_TYPE_OFFLINE:
      options = drive::SEARCH_METADATA_OFFLINE;
      break;
    case api::file_manager_private::SEARCH_TYPE_ALL:
      options = drive::SEARCH_METADATA_ALL;
      break;
    case api::file_manager_private::SEARCH_TYPE_NONE:
      break;
  }
  DCHECK_NE(options, -1);

  file_system->SearchMetadata(
      params->search_params.query,
      options,
      params->search_params.max_results,
      base::Bind(&FileManagerPrivateSearchDriveMetadataFunction::
                     OnSearchMetadata, this));
  return true;
}

void FileManagerPrivateSearchDriveMetadataFunction::OnSearchMetadata(
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
          &FileManagerPrivateSearchDriveMetadataFunction::OnEntryDefinitionList,
          this,
          base::Passed(&results)));
}

void FileManagerPrivateSearchDriveMetadataFunction::OnEntryDefinitionList(
    scoped_ptr<drive::MetadataSearchResultVector> search_result_info_list,
    scoped_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(search_result_info_list->size(), entry_definition_list->size());
  base::ListValue* results_list = new base::ListValue();

  // Convert Drive files to something File API stack can understand.  See
  // file_browser_handler_custom_bindings.cc and
  // file_manager_private_custom_bindings.js for how this is magically
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

bool FileManagerPrivateGetDriveConnectionStateFunction::RunSync() {
  api::file_manager_private::DriveConnectionState result;

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

  results_ = api::file_manager_private::GetDriveConnectionState::Results::
      Create(result);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger)
    logger->Log(logging::LOG_INFO, "%s succeeded.", name().c_str());
  return true;
}

bool FileManagerPrivateRequestAccessTokenFunction::RunAsync() {
  using extensions::api::file_manager_private::RequestAccessToken::Params;
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
      base::Bind(&FileManagerPrivateRequestAccessTokenFunction::
                      OnAccessTokenFetched, this));
  return true;
}

void FileManagerPrivateRequestAccessTokenFunction::OnAccessTokenFetched(
    google_apis::GDataErrorCode code,
    const std::string& access_token) {
  SetResult(new base::StringValue(access_token));
  SendResponse(true);
}

bool FileManagerPrivateGetShareUrlFunction::RunAsync() {
  using extensions::api::file_manager_private::GetShareUrl::Params;
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
      base::Bind(&FileManagerPrivateGetShareUrlFunction::OnGetShareUrl, this));
  return true;
}

void FileManagerPrivateGetShareUrlFunction::OnGetShareUrl(
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

bool FileManagerPrivateRequestDriveShareFunction::RunAsync() {
  using extensions::api::file_manager_private::RequestDriveShare::Params;
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
    case api::file_manager_private::DRIVE_SHARE_TYPE_NONE:
      NOTREACHED();
      return false;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_EDIT:
      role = google_apis::drive::PERMISSION_ROLE_WRITER;
      break;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_COMMENT:
      role = google_apis::drive::PERMISSION_ROLE_COMMENTER;
      break;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_VIEW:
      role = google_apis::drive::PERMISSION_ROLE_READER;
      break;
  }

  // Share |drive_path| in |owner_file_system| to |user->email()|.
  owner_file_system->AddPermission(
      drive_path,
      user->email(),
      role,
      base::Bind(&FileManagerPrivateRequestDriveShareFunction::OnAddPermission,
                 this));
  return true;
}

void FileManagerPrivateRequestDriveShareFunction::OnAddPermission(
    drive::FileError error) {
  SendResponse(error == drive::FILE_ERROR_OK);
}

FileManagerPrivateGetDownloadUrlFunction::
    FileManagerPrivateGetDownloadUrlFunction() {
}

FileManagerPrivateGetDownloadUrlFunction::
    ~FileManagerPrivateGetDownloadUrlFunction() {
}

bool FileManagerPrivateGetDownloadUrlFunction::RunAsync() {
  using extensions::api::file_manager_private::GetShareUrl::Params;
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
      base::Bind(&FileManagerPrivateGetDownloadUrlFunction::OnGetResourceEntry,
                 this));
  return true;
}

void FileManagerPrivateGetDownloadUrlFunction::OnGetResourceEntry(
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
      &FileManagerPrivateGetDownloadUrlFunction::OnTokenFetched, this));
}

void FileManagerPrivateGetDownloadUrlFunction::OnTokenFetched(
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
