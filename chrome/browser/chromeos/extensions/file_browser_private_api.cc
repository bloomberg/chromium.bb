// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <utime.h>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler.h"
#include "chrome/browser/chromeos/extensions/file_browser_private_api_factory.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/zip_file_creator.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/web_intents_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "ui/base/dialogs/selected_file_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/glue/web_intent_service_data.h"

using extensions::app_file_handler_util::FindFileHandlersForMimeTypes;
using chromeos::disks::DiskMountManager;
using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using extensions::ZipFileCreator;
using file_handler_util::FileTaskExecutor;
using google_apis::InstalledApp;

namespace {

// Default icon path for drive docs.
const char kDefaultIcon[] = "images/filetype_generic.png";
const int kPreferredIconSize = 16;

// Error messages.
const char kFileError[] = "File error %d";
const char kInvalidFileUrl[] = "Invalid file URL";
const char kVolumeDevicePathNotFound[] = "Device path not found";

// Unescape rules used for parsing query parameters.
const net::UnescapeRule::Type kUnescapeRuleForQueryParameters =
    net::UnescapeRule::SPACES |
    net::UnescapeRule::URL_SPECIAL_CHARS |
    net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;

const DiskMountManager::Disk* GetVolumeAsDisk(const std::string& mount_path) {
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();

  DiskMountManager::MountPointMap::const_iterator mount_point_it =
      disk_mount_manager->mount_points().find(mount_path);
  if (mount_point_it == disk_mount_manager->mount_points().end())
    return NULL;

  const DiskMountManager::Disk* disk = disk_mount_manager->FindDiskBySourcePath(
      mount_point_it->second.source_path);

  return (disk && disk->is_hidden()) ? NULL : disk;
}

base::DictionaryValue* CreateValueFromDisk(
    Profile* profile,
    const std::string& extension_id,
    const DiskMountManager::Disk* volume) {
  base::DictionaryValue* volume_info = new base::DictionaryValue();

  std::string mount_path;
  if (!volume->mount_path().empty()) {
    FilePath relative_mount_path;
    file_manager_util::ConvertFileToRelativeFileSystemPath(profile,
        extension_id, FilePath(volume->mount_path()), &relative_mount_path);
    mount_path = relative_mount_path.value();
  }

  volume_info->SetString("devicePath", volume->device_path());
  volume_info->SetString("mountPath", mount_path);
  volume_info->SetString("systemPath", volume->system_path());
  volume_info->SetString("filePath", volume->file_path());
  volume_info->SetString("deviceLabel", volume->device_label());
  volume_info->SetString("driveLabel", volume->drive_label());
  volume_info->SetString("deviceType",
      DiskMountManager::DeviceTypeToString(volume->device_type()));
  volume_info->SetInteger("totalSize", volume->total_size_in_bytes());
  volume_info->SetBoolean("isParent", volume->is_parent());
  volume_info->SetBoolean("isReadOnly", volume->is_read_only());
  volume_info->SetBoolean("hasMedia", volume->has_media());
  volume_info->SetBoolean("isOnBootDevice", volume->on_boot_device());

  return volume_info;
}

base::DictionaryValue* CreateValueFromMountPoint(Profile* profile,
    const DiskMountManager::MountPointInfo& mount_point_info,
    const std::string& extension_id,
    const GURL& extension_source_url) {

  base::DictionaryValue *mount_info = new base::DictionaryValue();

  mount_info->SetString("mountType",
                        DiskMountManager::MountTypeToString(
                            mount_point_info.mount_type));
  mount_info->SetString("sourcePath", mount_point_info.source_path);

  FilePath relative_mount_path;
  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  if (file_manager_util::ConvertFileToRelativeFileSystemPath(
          profile,
          extension_id,
          FilePath(mount_point_info.mount_path),
          &relative_mount_path)) {
    mount_info->SetString("mountPath", relative_mount_path.value());
  }

  mount_info->SetString("mountCondition",
      DiskMountManager::MountConditionToString(
          mount_point_info.mount_condition));

  return mount_info;
}

// Gives the extension renderer |host| file |permissions| for the given |path|.
void GrantFilePermissionsToHost(content::RenderViewHost* host,
                                const FilePath& path,
                                int permissions) {
  ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      host->GetProcess()->GetID(), path, permissions);
}

void AddDriveMountPoint(
    Profile* profile,
    const std::string& extension_id,
    content::RenderViewHost* render_view_host) {
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetStoragePartition(profile, site_instance)->
      GetFileSystemContext()->external_provider();
  if (!provider)
    return;

  const FilePath mount_point = drive::util::GetDriveMountPointPath();
  if (!render_view_host || !render_view_host->GetProcess())
    return;

  // Grant R/W permissions to drive 'folder'. File API layer still
  // expects this to be satisfied.
  GrantFilePermissionsToHost(render_view_host,
                             mount_point,
                             file_handler_util::GetReadWritePermissions());

  // Grant R/W permission for tmp and pinned cache folder.
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service || !system_service->file_system())
    return;
  drive::DriveCache* cache = system_service->cache();

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for drive/ paths.
  GrantFilePermissionsToHost(render_view_host,
                             cache->GetCacheDirectoryPath(
                                 drive::DriveCache::CACHE_TYPE_TMP),
                             file_handler_util::GetReadOnlyPermissions());
  GrantFilePermissionsToHost(
      render_view_host,
      cache->GetCacheDirectoryPath(
          drive::DriveCache::CACHE_TYPE_PERSISTENT),
      file_handler_util::GetReadOnlyPermissions());

  FilePath mount_point_virtual;
  if (provider->GetVirtualPath(mount_point, &mount_point_virtual))
    provider->GrantFileAccessToExtension(extension_id, mount_point_virtual);
}

// Finds an icon in the list of icons. If unable to find an icon of the exact
// size requested, returns one with the next larger size. If all icons are
// smaller than the preferred size, we'll return the largest one available.
// Icons must be sorted by the icon size, smallest to largest. If there are no
// icons in the list, returns an empty URL.
GURL FindPreferredIcon(const InstalledApp::IconList& icons,
                       int preferred_size) {
  GURL result;
  if (icons.empty())
    return result;
  result = icons.rbegin()->second;
  for (InstalledApp::IconList::const_reverse_iterator iter = icons.rbegin();
       iter != icons.rend() && iter->first >= preferred_size; ++iter) {
        result = iter->second;
  }
  return result;
}

// Finds the title of the given Web Intents |action|, if the passed extension
// supports this action for all specified |mime_types|. Returns true and
// provides the |title| as output on success.
bool FindTitleForActionWithTypes(
    const Extension* extension,
    const std::string& action,
    const std::set<std::string>& mime_types,
    std::string* title) {
  DCHECK(!mime_types.empty());
  std::set<std::string> pending(mime_types.begin(), mime_types.end());
  std::string found_title;

  for (std::vector<webkit_glue::WebIntentServiceData>::const_iterator data =
          extensions::WebIntentsInfo::GetIntentsServices(extension).begin();
       data != extensions::WebIntentsInfo::GetIntentsServices(extension).end();
       ++data) {
    if (pending.empty())
      break;

    if (UTF16ToUTF8(data->action) != action)
      continue;

    std::set<std::string>::iterator pending_iter = pending.begin();
    while (pending_iter != pending.end()) {
      std::set<std::string>::iterator current = pending_iter++;
      if (net::MatchesMimeType(UTF16ToUTF8(data->type), *current))
        pending.erase(current);
    }
    if (found_title.empty())
      found_title = UTF16ToUTF8(data->title);
  }

  // Not all mime-types have been found.
  if (!pending.empty())
    return false;

  *title = found_title;
  return true;
}

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsOnFileThread(const std::string& mount_path,
                              size_t* total_size_kb,
                              size_t* remaining_size_kb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  uint64_t total_size_in_bytes = 0;
  uint64_t remaining_size_in_bytes = 0;

  struct statvfs stat = {};  // Zero-clear
  if (statvfs(mount_path.c_str(), &stat) == 0) {
    total_size_in_bytes =
        static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    remaining_size_in_bytes =
        static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
  }
  *total_size_kb = static_cast<size_t>(total_size_in_bytes / 1024);
  *remaining_size_kb = static_cast<size_t>(remaining_size_in_bytes / 1024);
}

// Given a |url|, return the virtual FilePath associated with it. If the file
// isn't of the type CrosMountPointProvider handles, return an empty FilePath.
//
// Virtual paths will look like "Downloads/foo/bar.txt" or "drive/foo/bar.txt".
FilePath GetVirtualPathFromURL(const GURL& url) {
  fileapi::FileSystemURL filesystem_url(url);
  if (!chromeos::CrosMountPointProvider::CanHandleURL(filesystem_url))
    return FilePath();
  return filesystem_url.virtual_path();
}

// Given a |url|, return the local FilePath associated with it. If the file
// isn't of the type CrosMountPointProvider handles, return an empty FilePath.
//
// Local paths will look like "/home/chronos/user/Downloads/foo/bar.txt" or
// "/special/drive/foo/bar.txt".
FilePath GetLocalPathFromURL(const GURL& url) {
  fileapi::FileSystemURL filesystem_url(url);
  if (!chromeos::CrosMountPointProvider::CanHandleURL(filesystem_url))
    return FilePath();
  return filesystem_url.path();
}

// Make a set of unique filename suffixes out of the list of file URLs.
std::set<std::string> GetUniqueSuffixes(base::ListValue* file_url_list) {
  std::set<std::string> suffixes;
  for (size_t i = 0; i < file_url_list->GetSize(); ++i) {
    std::string url;
    if (!file_url_list->GetString(i, &url))
      return std::set<std::string>();
    FilePath path = GetVirtualPathFromURL(GURL(url));
    if (path.empty())
      return std::set<std::string>();
    // We'll skip empty suffixes.
    if (!path.Extension().empty())
      suffixes.insert(path.Extension());
  }
  return suffixes;
}

// Make a set of unique MIME types out of the list of MIME types.
std::set<std::string> GetUniqueMimeTypes(base::ListValue* mime_type_list) {
  std::set<std::string> mime_types;
  for (size_t i = 0; i < mime_type_list->GetSize(); ++i) {
    std::string mime_type;
    if (!mime_type_list->GetString(i, &mime_type))
      return std::set<std::string>();
    // We'll skip empty MIME types.
    if (!mime_type.empty())
      mime_types.insert(mime_type);
  }
  return mime_types;
}

void LogDefaultTask(const std::set<std::string>& mime_types,
                    const std::set<std::string>& suffixes,
                    const std::string& task_id) {
  if (!mime_types.empty()) {
    std::string mime_types_str;
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
        iter != mime_types.end(); ++iter) {
      if (iter == mime_types.begin()) {
        mime_types_str = *iter;
      } else {
        mime_types_str += ", " + *iter;
      }
    }
    VLOG(1) << "Associating task " << task_id
            << " with the following MIME types: ";
    VLOG(1) << "  " << mime_types_str;
  }

  if (!suffixes.empty()) {
    std::string suffixes_str;
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
        iter != suffixes.end(); ++iter) {
      if (iter == suffixes.begin()) {
        suffixes_str = *iter;
      } else {
        suffixes_str += ", " + *iter;
      }
    }
    VLOG(1) << "Associating task " << task_id
            << " with the following suffixes: ";
    VLOG(1) << "  " << suffixes_str;
  }
}

// Does nothing with a bool parameter. Used as a placeholder for calling
// ClearCacheAndRemountFileSystem(). TODO(yoshiki): Handle an error from
// ClearCacheAndRemountFileSystem() properly: http://crbug.com/140511.
void DoNothingWithBool(bool /* success */) {
}

void FillDriveFilePropertiesValue(
    const drive::DriveEntryProto& entry_proto,
    DictionaryValue* property_dict) {
  const drive::DriveFileSpecificInfo& file_specific_info =
      entry_proto.file_specific_info();

  property_dict->SetString("thumbnailUrl", file_specific_info.thumbnail_url());

  if (!file_specific_info.alternate_url().empty())
    property_dict->SetString("editUrl", file_specific_info.alternate_url());

  if (!file_specific_info.share_url().empty())
    property_dict->SetString("shareUrl", file_specific_info.share_url());

  if (!entry_proto.content_url().empty())
    property_dict->SetString("contentUrl", entry_proto.content_url());

  property_dict->SetBoolean("isHosted",
                            file_specific_info.is_hosted_document());

  property_dict->SetString("contentMimeType",
                           file_specific_info.content_mime_type());
}

}  // namespace

class RequestLocalFileSystemFunction::LocalFileSystemCallbackDispatcher {
 public:
  static fileapi::FileSystemContext::OpenFileSystemCallback CreateCallback(
      RequestLocalFileSystemFunction* function,
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      int child_id,
      scoped_refptr<const Extension> extension) {
    return base::Bind(
        &LocalFileSystemCallbackDispatcher::DidOpenFileSystem,
        base::Owned(new LocalFileSystemCallbackDispatcher(
            function, file_system_context, child_id, extension)));
  }

  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& name,
                         const GURL& root_path) {
    if (result != base::PLATFORM_FILE_OK) {
      DidFail(result);
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // Set up file permission access.
    if (!SetupFileSystemAccessPermissions()) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &RequestLocalFileSystemFunction::RespondSuccessOnUIThread,
            function_,
            name,
            root_path));
  }

  void DidFail(base::PlatformFileError error_code) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &RequestLocalFileSystemFunction::RespondFailedOnUIThread,
            function_,
            error_code));
  }

 private:
  LocalFileSystemCallbackDispatcher(
      RequestLocalFileSystemFunction* function,
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      int child_id,
      scoped_refptr<const Extension> extension)
      : function_(function),
        file_system_context_(file_system_context),
        child_id_(child_id),
        extension_(extension)  {
    DCHECK(function_);
  }

  // Grants file system access permissions to file browser component.
  bool SetupFileSystemAccessPermissions() {
    if (!extension_.get())
      return false;

    // Make sure that only component extension can access the entire
    // local file system.
    if (extension_->location() != Extension::COMPONENT) {
      NOTREACHED() << "Private method access by non-component extension "
                   << extension_->id();
      return false;
    }

    fileapi::ExternalFileSystemMountPointProvider* provider =
        file_system_context_->external_provider();
    if (!provider)
      return false;

    // Grant full access to File API from this component extension.
    provider->GrantFullAccessToExtension(extension_->id());

    // Grant R/W file permissions to the renderer hosting component
    // extension for all paths exposed by our local file system provider.
    std::vector<FilePath> root_dirs = provider->GetRootDirectories();
    for (size_t i = 0; i < root_dirs.size(); ++i) {
      ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
          child_id_, root_dirs[i],
          file_handler_util::GetReadWritePermissions());
    }
    return true;
  }

  RequestLocalFileSystemFunction* function_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  // Renderer process id.
  int child_id_;
  // Extension source URL.
  scoped_refptr<const Extension> extension_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemCallbackDispatcher);
};

FileBrowserPrivateAPI::FileBrowserPrivateAPI(Profile* profile)
    : event_router_(make_scoped_refptr(new FileBrowserEventRouter(profile))) {
  extensions::ManifestHandler::Register(
      extension_manifest_keys::kFileBrowserHandlers,
      new FileBrowserHandlerParser);

  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<CancelFileDialogFunction>();
  registry->RegisterFunction<ExecuteTasksFileBrowserFunction>();
  registry->RegisterFunction<SetDefaultTaskFileBrowserFunction>();
  registry->RegisterFunction<FileDialogStringsFunction>();
  registry->RegisterFunction<GetFileTasksFileBrowserFunction>();
  registry->RegisterFunction<GetVolumeMetadataFunction>();
  registry->RegisterFunction<RequestLocalFileSystemFunction>();
  registry->RegisterFunction<AddFileWatchBrowserFunction>();
  registry->RegisterFunction<RemoveFileWatchBrowserFunction>();
  registry->RegisterFunction<SelectFileFunction>();
  registry->RegisterFunction<SelectFilesFunction>();
  registry->RegisterFunction<AddMountFunction>();
  registry->RegisterFunction<RemoveMountFunction>();
  registry->RegisterFunction<GetMountPointsFunction>();
  registry->RegisterFunction<GetSizeStatsFunction>();
  registry->RegisterFunction<FormatDeviceFunction>();
  registry->RegisterFunction<ViewFilesFunction>();
  registry->RegisterFunction<ToggleFullscreenFunction>();
  registry->RegisterFunction<IsFullscreenFunction>();
  registry->RegisterFunction<GetDriveFilePropertiesFunction>();
  registry->RegisterFunction<PinDriveFileFunction>();
  registry->RegisterFunction<GetFileLocationsFunction>();
  registry->RegisterFunction<GetDriveFilesFunction>();
  registry->RegisterFunction<GetFileTransfersFunction>();
  registry->RegisterFunction<CancelFileTransfersFunction>();
  registry->RegisterFunction<TransferFileFunction>();
  registry->RegisterFunction<GetPreferencesFunction>();
  registry->RegisterFunction<SetPreferencesFunction>();
  registry->RegisterFunction<SearchDriveFunction>();
  registry->RegisterFunction<ClearDriveCacheFunction>();
  registry->RegisterFunction<ReloadDriveFunction>();
  registry->RegisterFunction<GetNetworkConnectionStateFunction>();
  registry->RegisterFunction<RequestDirectoryRefreshFunction>();
  registry->RegisterFunction<SetLastModifiedFunction>();
  registry->RegisterFunction<ZipSelectionFunction>();

  event_router_->ObserveFileSystemEvents();
}

FileBrowserPrivateAPI::~FileBrowserPrivateAPI() {
}

void FileBrowserPrivateAPI::Shutdown() {
  event_router_->Shutdown();
}

// static
FileBrowserPrivateAPI* FileBrowserPrivateAPI::Get(Profile* profile) {
  return FileBrowserPrivateAPIFactory::GetForProfile(profile);
}

void RequestLocalFileSystemFunction::RequestOnFileThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& source_url,
    int child_id) {
  GURL origin_url = source_url.GetOrigin();
  file_system_context->OpenFileSystem(
      origin_url, fileapi::kFileSystemTypeExternal, false,  // create
      LocalFileSystemCallbackDispatcher::CreateCallback(
          this,
          file_system_context,
          child_id,
          GetExtension()));
}

bool RequestLocalFileSystemFunction::RunImpl() {
  if (!dispatcher() || !render_view_host() || !render_view_host()->GetProcess())
    return false;

  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(profile_, site_instance)->
          GetFileSystemContext();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &RequestLocalFileSystemFunction::RequestOnFileThread,
          this,
          file_system_context,
          source_url_,
          render_view_host()->GetProcess()->GetID()));
  // Will finish asynchronously.
  return true;
}

void RequestLocalFileSystemFunction::RespondSuccessOnUIThread(
    const std::string& name, const GURL& root_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Add drive mount point immediately when we kick of first instance of file
  // manager. The actual mount event will be sent to UI only when we perform
  // proper authentication.
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  if (system_service)
    AddDriveMountPoint(profile_, extension_id(), render_view_host());
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("name", name);
  dict->SetString("path", root_path.spec());
  dict->SetInteger("error", drive::DRIVE_FILE_OK);
  SendResponse(true);
}

void RequestLocalFileSystemFunction::RespondFailedOnUIThread(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  error_ = base::StringPrintf(kFileError, static_cast<int>(error_code));
  SendResponse(false);
}

void FileWatchBrowserFunctionBase::RespondOnUIThread(bool success) {
  SetResult(Value::CreateBooleanValue(success));
  SendResponse(success);
}

bool FileWatchBrowserFunctionBase::RunImpl() {
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // First param is url of a file to watch.
  std::string url;
  if (!args_->GetString(0, &url) || url.empty())
    return false;

  GURL file_watch_url(url);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &FileWatchBrowserFunctionBase::RunFileWatchOperationOnFileThread,
          this,
          FileBrowserPrivateAPI::Get(profile_)->event_router(),
          file_watch_url,
          extension_id()));

  return true;
}

void FileWatchBrowserFunctionBase::RunFileWatchOperationOnFileThread(
    scoped_refptr<FileBrowserEventRouter> event_router,
    const GURL& file_url, const std::string& extension_id) {
  FilePath local_path = GetLocalPathFromURL(file_url);
  FilePath virtual_path = GetVirtualPathFromURL(file_url);
  bool result = !local_path.empty() && PerformFileWatchOperation(
      event_router, local_path, virtual_path, extension_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &FileWatchBrowserFunctionBase::RespondOnUIThread, this, result));
}

bool AddFileWatchBrowserFunction::PerformFileWatchOperation(
    scoped_refptr<FileBrowserEventRouter> event_router,
    const FilePath& local_path, const FilePath& virtual_path,
    const std::string& extension_id) {
  return event_router->AddFileWatch(local_path, virtual_path, extension_id);
}

bool RemoveFileWatchBrowserFunction::PerformFileWatchOperation(
    scoped_refptr<FileBrowserEventRouter> event_router,
    const FilePath& local_path, const FilePath& unused,
    const std::string& extension_id) {
  event_router->RemoveFileWatch(local_path, extension_id);
  return true;
}

// static
void GetFileTasksFileBrowserFunction::IntersectAvailableDriveTasks(
    drive::DriveWebAppsRegistry* registry,
    const FileInfoList& file_info_list,
    WebAppInfoMap* app_info,
    std::set<std::string>* available_tasks) {
  for (FileInfoList::const_iterator file_iter = file_info_list.begin();
       file_iter != file_info_list.end(); ++file_iter) {
    if (file_iter->file_path.empty())
      continue;
    ScopedVector<drive::DriveWebAppInfo> info;
    registry->GetWebAppsForFile(file_iter->file_path,
                                file_iter->mime_type, &info);
    std::vector<drive::DriveWebAppInfo*> info_ptrs;
    info.release(&info_ptrs);  // so they don't go away prematurely.
    std::set<std::string> tasks_for_this_file;
    for (std::vector<drive::DriveWebAppInfo*>::iterator
         apps = info_ptrs.begin(); apps != info_ptrs.end(); ++apps) {
      std::pair<WebAppInfoMap::iterator, bool> insert_result =
          app_info->insert(std::make_pair((*apps)->app_id, *apps));
      // TODO(gspencer): For now, the action id is always "open-with", but we
      // could add any actions that the drive app supports.
      std::string task_id = file_handler_util::MakeTaskID(
          (*apps)->app_id, file_handler_util::kTaskDrive, "open-with");
      tasks_for_this_file.insert(task_id);
      // If we failed to insert a task_id because there was a duplicate, then we
      // must delete it (since we own it).
      if (!insert_result.second)
        delete *apps;
    }
    if (file_iter == file_info_list.begin()) {
      *available_tasks = tasks_for_this_file;
    } else {
      std::set<std::string> intersection;
      std::set_intersection(available_tasks->begin(),
                            available_tasks->end(),
                            tasks_for_this_file.begin(),
                            tasks_for_this_file.end(),
                            std::inserter(intersection,
                                          intersection.begin()));
      *available_tasks = intersection;
    }
  }
}

void GetFileTasksFileBrowserFunction::FindDefaultDriveTasks(
    const FileInfoList& file_info_list,
    const std::set<std::string>& available_tasks,
    std::set<std::string>* default_tasks) {
  std::set<std::string> default_task_ids;
  for (FileInfoList::const_iterator file_iter = file_info_list.begin();
       file_iter != file_info_list.end(); ++file_iter) {
    std::string task_id = file_handler_util::GetDefaultTaskIdFromPrefs(
        profile_, file_iter->mime_type, file_iter->file_path.Extension());
    if (available_tasks.find(task_id) != available_tasks.end()) {
      VLOG(1) << "Found default task for " << file_iter->file_path.value()
              << ": " << task_id;
      default_tasks->insert(task_id);
    } else {
      if (VLOG_IS_ON(2)) {
        VLOG(2) << "Didn't find default task " << task_id
                << " in available tasks";
        VLOG(2) << "Available Tasks:";
        for (std::set<std::string>::iterator iter = available_tasks.begin();
             iter != available_tasks.end(); ++iter) {
          VLOG(2) << "  " << *iter;
        }
      }
    }
  }
}

// static
void GetFileTasksFileBrowserFunction::CreateDriveTasks(
    drive::DriveWebAppsRegistry* registry,
    const WebAppInfoMap& app_info,
    const std::set<std::string>& available_tasks,
    const std::set<std::string>& default_tasks,
    ListValue* result_list,
    bool* default_already_set) {
  *default_already_set = false;
  // OK, now we traverse the intersection of available applications for this
  // list of files, adding a task for each one that is found.
  for (std::set<std::string>::const_iterator app_iter = available_tasks.begin();
       app_iter != available_tasks.end(); ++app_iter) {
    std::string app_id;
    std::string task_type;
    bool result = file_handler_util::CrackTaskID(
        *app_iter, &app_id, &task_type, NULL);
    DCHECK(result) << "Unable to parse Drive task id: " << *app_iter;
    DCHECK_EQ(task_type, file_handler_util::kTaskDrive);

    WebAppInfoMap::const_iterator info_iter = app_info.find(app_id);
    DCHECK(info_iter != app_info.end());
    drive::DriveWebAppInfo* info = info_iter->second;
    DictionaryValue* task = new DictionaryValue;

    task->SetString("taskId", *app_iter);
    task->SetString("title", info->app_name);

    GURL best_icon = FindPreferredIcon(info->app_icons,
                                       kPreferredIconSize);
    if (!best_icon.is_empty()) {
      task->SetString("iconUrl", best_icon.spec());
    }
    task->SetBoolean("driveApp", true);

    // Once we set a default app, we don't want to set any more.
    if (!(*default_already_set) &&
        default_tasks.find(*app_iter) != default_tasks.end()) {
      task->SetBoolean("isDefault", true);
      *default_already_set = true;
    } else {
      task->SetBoolean("isDefault", false);
    }
    result_list->Append(task);
  }
}

// Find special tasks here for Drive (Blox) apps. Iterate through matching drive
// apps and add them, with generated task ids. Extension ids will be the app_ids
// from drive. We'll know that they are drive apps because the extension id will
// begin with kDriveTaskExtensionPrefix.
bool GetFileTasksFileBrowserFunction::FindDriveAppTasks(
    const FileInfoList& file_info_list,
    ListValue* result_list,
    bool* default_already_set) {

  if (file_info_list.empty())
    return true;

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled. We return true in this
  // case because there might be other extension tasks, even if we don't have
  // any to add.
  if (!system_service || !system_service->webapps_registry())
    return true;

  drive::DriveWebAppsRegistry* registry = system_service->webapps_registry();

  // Map of app_id to DriveWebAppInfo so we can look up the apps we've found
  // after taking the intersection of available apps.
  std::map<std::string, drive::DriveWebAppInfo*> app_info;
  // Set of application IDs. This will end up with the intersection of the
  // application IDs that apply to the paths in |file_paths|.
  std::set<std::string> available_tasks;

  IntersectAvailableDriveTasks(registry, file_info_list,
                               &app_info, &available_tasks);
  std::set<std::string> default_tasks;
  FindDefaultDriveTasks(file_info_list, available_tasks, &default_tasks);
  CreateDriveTasks(registry, app_info, available_tasks, default_tasks,
                   result_list, default_already_set);

  // We own the pointers in |app_info|, so we need to delete them.
  STLDeleteContainerPairSecondPointers(app_info.begin(), app_info.end());
  return true;
}

static void GetMimeTypesForFileURLs(const std::vector<GURL>& file_urls,
    std::set<std::string>* mime_types) {
  for (std::vector<GURL>::const_iterator iter = file_urls.begin();
       iter != file_urls.end(); ++iter) {
    const FilePath file = FilePath(GURL(iter->spec()).ExtractFileName());
    const FilePath::StringType file_extension =
        StringToLowerASCII(file.Extension());

    // TODO(thorogood): Rearchitect this call so it can run on the File thread;
    // GetMimeTypeFromFile requires this on Linux. Right now, we use
    // Chrome-level knowledge only.
    std::string mime_type;
    if (file_extension.empty() || !net::GetWellKnownMimeTypeFromExtension(
            file_extension.substr(1), &mime_type)) {
      // If the file doesn't have an extension or its mime-type cannot be
      // determined, then indicate that it has the empty mime-type. This will
      // only be matched if the Web Intents accepts "*" or "*/*".
      mime_types->insert("");
    } else {
      mime_types->insert(mime_type);
    }
  }
}

bool GetFileTasksFileBrowserFunction::FindAppTasks(
    const std::vector<GURL>& file_urls,
    ListValue* result_list) {
  DCHECK(!file_urls.empty());
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return false;

  std::set<std::string> mime_types;
  GetMimeTypesForFileURLs(file_urls, &mime_types);

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = *iter;

    // We don't support using hosted apps to open files.
    if (!extension->is_platform_app())
      continue;

    if (profile_->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    typedef std::vector<const extensions::FileHandlerInfo*> FileHandlerList;
    FileHandlerList file_handlers =
        FindFileHandlersForMimeTypes(*extension, mime_types);
    // TODO(benwells): also support matching by file extension.
    if (file_handlers.empty())
      continue;

    for (FileHandlerList::iterator i = file_handlers.begin();
         i != file_handlers.end(); ++i) {
      DictionaryValue* task = new DictionaryValue;
      std::string task_id = file_handler_util::MakeTaskID(extension->id(),
          file_handler_util::kTaskApp, (*i)->id);
      task->SetString("taskId", task_id);
      task->SetString("title", (*i)->title);
      task->SetBoolean("isDefault", false);

      GURL best_icon = extension->GetIconURL(kPreferredIconSize,
                                             ExtensionIconSet::MATCH_BIGGER);
      if (!best_icon.is_empty())
        task->SetString("iconUrl", best_icon.spec());
      else
        task->SetString("iconUrl", kDefaultIcon);

      task->SetBoolean("driveApp", false);
      result_list->Append(task);
    }
  }

  return true;
}

// Find Web Intent platform apps that support the View task, and add them to
// the |result_list|. These will be marked as kTaskWebIntent.
bool GetFileTasksFileBrowserFunction::FindWebIntentTasks(
    const std::vector<GURL>& file_urls,
    ListValue* result_list) {
  DCHECK(!file_urls.empty());
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return false;

  std::set<std::string> mime_types;
  GetMimeTypesForFileURLs(file_urls, &mime_types);

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = *iter;

    // We don't support using hosted apps to open files.
    if (!extension->is_platform_app())
      continue;

    if (profile_->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    std::string title;
    if (!FindTitleForActionWithTypes(
            extension, web_intents::kActionView, mime_types, &title))
      continue;

    DictionaryValue* task = new DictionaryValue;
    std::string task_id = file_handler_util::MakeTaskID(extension->id(),
        file_handler_util::kTaskWebIntent, web_intents::kActionView);
    task->SetString("taskId", task_id);
    task->SetString("title", title);
    task->SetBoolean("isDefault", false);

    GURL best_icon = extension->GetIconURL(kPreferredIconSize,
                                           ExtensionIconSet::MATCH_BIGGER);
    if (!best_icon.is_empty())
      task->SetString("iconUrl", best_icon.spec());
    else
      task->SetString("iconUrl", kDefaultIcon);

    task->SetBoolean("driveApp", false);
    result_list->Append(task);
  }

  return true;
}

bool GetFileTasksFileBrowserFunction::RunImpl() {
  // First argument is the list of files to get tasks for.
  ListValue* files_list = NULL;
  if (!args_->GetList(0, &files_list))
    return false;

  // Second argument is the list of mime types of each of the files in the list.
  ListValue* mime_types_list = NULL;
  if (!args_->GetList(1, &mime_types_list))
    return false;

  // MIME types can either be empty, or there needs to be one for each file.
  if (mime_types_list->GetSize() != files_list->GetSize() &&
      mime_types_list->GetSize() != 0)
    return false;

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  FileInfoList info_list;
  std::vector<GURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); ++i) {
    FileInfo info;
    std::string file_url;
    if (!files_list->GetString(i, &file_url))
      return false;
    info.file_url = GURL(file_url);
    file_urls.push_back(info.file_url);
    if (mime_types_list->GetSize() != 0 &&
        !mime_types_list->GetString(i, &info.mime_type))
      return false;
    fileapi::FileSystemURL file_system_url(info.file_url);
    if (chromeos::CrosMountPointProvider::CanHandleURL(file_system_url)) {
      info.file_path = file_system_url.path();
    }
    info_list.push_back(info);
  }

  ListValue* result_list = new ListValue();
  SetResult(result_list);

  // Find the Drive apps first, because we want them to take precedence
  // when setting the default app.
  bool default_already_set = false;
  if (!FindDriveAppTasks(info_list, result_list, &default_already_set))
    return false;

  // Take the union of Drive and extension tasks: Because any Drive tasks we
  // found must apply to all of the files (intersection), and because the same
  // is true of the extensions, we simply take the union of two lists by adding
  // the extension tasks to the Drive task list. We know there aren't duplicates
  // because they're entirely different kinds of tasks, but there could be both
  // kinds of tasks for a file type (an image file, for instance).
  std::set<const FileBrowserHandler*> common_tasks;
  std::set<const FileBrowserHandler*> default_tasks;
  if (!file_handler_util::FindCommonTasks(profile_, file_urls, &common_tasks))
    return false;
  file_handler_util::FindDefaultTasks(profile_, file_urls,
                                      common_tasks, &default_tasks);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  for (std::set<const FileBrowserHandler*>::const_iterator iter =
           common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const FileBrowserHandler* handler = *iter;
    const std::string extension_id = handler->extension_id();
    const Extension* extension = service->GetExtensionById(extension_id, false);
    CHECK(extension);
    DictionaryValue* task = new DictionaryValue;
    task->SetString("taskId", file_handler_util::MakeTaskID(
        extension_id, file_handler_util::kTaskFile, handler->id()));
    task->SetString("title", handler->title());
    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    GURL icon =
        ExtensionIconSource::GetIconURL(extension,
                                        extension_misc::EXTENSION_ICON_BITTY,
                                        ExtensionIconSet::MATCH_BIGGER,
                                        false, NULL);     // grayscale
    task->SetString("iconUrl", icon.spec());
    task->SetBoolean("driveApp", false);

    // Only set the default if there isn't already a default set.
    if (!default_already_set &&
        default_tasks.find(*iter) != default_tasks.end()) {
      task->SetBoolean("isDefault", true);
      default_already_set = true;
    } else {
      task->SetBoolean("isDefault", false);
    }

    result_list->Append(task);
  }

  // Take the union of platform app file handlers, Web Intents that platform
  // apps may accept, and all previous Drive and extension tasks. As above, we
  // know there aren't duplicates because they're entirely different kinds of
  // tasks.
  if (!FindAppTasks(file_urls, result_list))
    return false;

  // TODO(benwells): remove the web intents tasks once we no longer support
  // them.
  if (!FindWebIntentTasks(file_urls, result_list))
    return false;

  if (VLOG_IS_ON(1)) {
    std::string result_json;
    base::JSONWriter::WriteWithOptions(
        result_list,
        base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &result_json);
    VLOG(1) << "GetFileTasks result:\n" << result_json;
  }

  SendResponse(true);
  return true;
}

ExecuteTasksFileBrowserFunction::ExecuteTasksFileBrowserFunction() {}

void ExecuteTasksFileBrowserFunction::OnTaskExecuted(bool success) {
  SendResponse(success);
}

ExecuteTasksFileBrowserFunction::~ExecuteTasksFileBrowserFunction() {}

bool ExecuteTasksFileBrowserFunction::RunImpl() {
  // First param is task id that was to the extension with getFileTasks call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  // TODO(kaznacheev): Crack the task_id here, store it in the Executor
  // and avoid passing it around.

  // The second param is the list of files that need to be executed with this
  // task.
  ListValue* files_list = NULL;
  if (!args_->GetList(1, &files_list))
    return false;

  std::string extension_id;
  std::string task_type;
  std::string action_id;
  if (!file_handler_util::CrackTaskID(
      task_id, &extension_id, &task_type, &action_id)) {
    LOG(WARNING) << "Invalid task " << task_id;
    return false;
  }

  if (!files_list->GetSize())
    return true;

  std::vector<GURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); i++) {
    std::string origin_file_url;
    if (!files_list->GetString(i, &origin_file_url)) {
      error_ = kInvalidFileUrl;
      return false;
    }
    file_urls.push_back(GURL(origin_file_url));
  }

  WebContents* web_contents =
      dispatcher()->delegate()->GetAssociatedWebContents();
  int32 tab_id = 0;
  if (web_contents)
    tab_id = ExtensionTabUtil::GetTabId(web_contents);

  scoped_refptr<FileTaskExecutor> executor(
      FileTaskExecutor::Create(profile(),
                               source_url(),
                               extension_->id(),
                               tab_id,
                               extension_id,
                               task_type,
                               action_id));

  if (!executor->ExecuteAndNotify(
      file_urls,
      base::Bind(&ExecuteTasksFileBrowserFunction::OnTaskExecuted, this)))
    return false;

  SetResult(new base::FundamentalValue(true));
  return true;
}

SetDefaultTaskFileBrowserFunction::SetDefaultTaskFileBrowserFunction() {}

SetDefaultTaskFileBrowserFunction::~SetDefaultTaskFileBrowserFunction() {}

bool SetDefaultTaskFileBrowserFunction::RunImpl() {
  // First param is task id that was to the extension with setDefaultTask call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  base::ListValue* file_url_list;
  if (!args_->GetList(1, &file_url_list))
    return false;
  std::set<std::string> suffixes = GetUniqueSuffixes(file_url_list);

  // MIME types are an optional parameter.
  base::ListValue* mime_type_list;
  std::set<std::string> mime_types;
  if (args_->GetList(2, &mime_type_list) && !mime_type_list->empty()) {
    if (mime_type_list->GetSize() != file_url_list->GetSize())
      return false;
    mime_types = GetUniqueMimeTypes(mime_type_list);
  }

  if (VLOG_IS_ON(1))
    LogDefaultTask(mime_types, suffixes, task_id);

  // If there weren't any mime_types, and all the suffixes were blank,
  // then we "succeed", but don't actually associate with anything.
  // Otherwise, any time we set the default on a file with no extension
  // on the local drive, we'd fail.
  // TODO(gspencer): Fix file manager so that it never tries to set default in
  // cases where extensionless local files are part of the selection.
  if (suffixes.empty() && mime_types.empty()) {
    SetResult(new base::FundamentalValue(true));
    return true;
  }

  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &file_handler_util::UpdateDefaultTask,
            profile_, task_id, suffixes, mime_types));

  SetResult(new base::FundamentalValue(true));
  return true;
}

FileBrowserFunction::FileBrowserFunction() {
}

FileBrowserFunction::~FileBrowserFunction() {
}

int32 FileBrowserFunction::GetTabId() const {
  if (!dispatcher()) {
    LOG(WARNING) << "No dispatcher";
    return 0;
  }
  if (!dispatcher()->delegate()) {
    LOG(WARNING) << "No delegate";
    return 0;
  }
  WebContents* web_contents =
      dispatcher()->delegate()->GetAssociatedWebContents();
  if (!web_contents) {
    LOG(WARNING) << "No associated tab contents";
    return 0;
  }
  return ExtensionTabUtil::GetTabId(web_contents);
}

void FileBrowserFunction::GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
    const UrlList& file_urls,
    GetLocalPathsCallback callback) {
  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(profile(), site_instance)->
          GetFileSystemContext();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &FileBrowserFunction::GetLocalPathsOnFileThread,
          this,
          file_system_context, file_urls, callback));
}

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls |callback| with the result vector, on the UI
// thread.
// TODO(kinuko): We no longer call GetFileSystemRootPathOnFileThread and
// we can likely remove this cross-thread helper method.
void FileBrowserFunction::GetLocalPathsOnFileThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const UrlList& file_urls,
    GetLocalPathsCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<ui::SelectedFileInfo> selected_files;

  GURL origin_url = source_url().GetOrigin();
  size_t len = file_urls.size();
  selected_files.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    FilePath local_path;
    const GURL& file_url = file_urls[i];

    // If "localPath" parameter is set, use it as the real path.
    // TODO(satorux): Eventually, we should be able to get the real path
    // from DriveFileSystem instead of passing through with filesystem
    // URLs. crosbug.com/27510.
    //
    // TODO(satorux): GURL::query() is not yet supported for filesystem:
    // URLs. For now, use GURL::spec() to get the query portion. Should
    // get rid of the hack once query() is supported: crbug.com/114484.
    const std::string::size_type query_start = file_url.spec().find('?');
    if (query_start != std::string::npos) {
      const std::string query = file_url.spec().substr(query_start + 1);
      std::vector<std::pair<std::string, std::string> > parameters;
      if (base::SplitStringIntoKeyValuePairs(
              query, '=', '&', &parameters)) {
        for (size_t i = 0; i < parameters.size(); ++i) {
          if (parameters[i].first == "localPath") {
            const std::string unescaped_value =
                net::UnescapeURLComponent(parameters[i].second,
                                          kUnescapeRuleForQueryParameters);
            local_path = FilePath::FromUTF8Unsafe(unescaped_value);
            break;
          }
        }
      }
    }

    // Extract the path from |file_url|.
    fileapi::FileSystemURL url(file_url);
    if (!chromeos::CrosMountPointProvider::CanHandleURL(url))
      continue;

    if (!url.path().empty()) {
      DVLOG(1) << "Selected: file path: " << url.path().value()
               << " local path: " << local_path.value();
      selected_files.push_back(
          ui::SelectedFileInfo(url.path(), local_path));
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, selected_files));
}

bool SelectFileFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }
  std::string file_url;
  args_->GetString(0, &file_url);
  UrlList file_paths;
  file_paths.push_back(GURL(file_url));

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_paths,
      base::Bind(&SelectFileFunction::GetLocalPathsResponseOnUIThread, this));
  return true;
}

void SelectFileFunction::GetLocalPathsResponseOnUIThread(
    const SelectedFileInfoList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  int index;
  args_->GetInteger(1, &index);
  int32 tab_id = GetTabId();
  SelectFileDialogExtension::OnFileSelected(tab_id, files[0], index);
  SendResponse(true);
}


ViewFilesFunction::ViewFilesFunction() {
}

ViewFilesFunction::~ViewFilesFunction() {
}

bool ViewFilesFunction::RunImpl() {
  if (args_->GetSize() < 1) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string internal_task_id;
  args_->GetString(1, &internal_task_id);

  std::vector<FilePath> files;
  for (size_t i = 0; i < path_list->GetSize(); ++i) {
    std::string url_as_string;
    path_list->GetString(i, &url_as_string);
    FilePath path = GetLocalPathFromURL(GURL(url_as_string));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  Browser* browser = GetCurrentBrowser();
  bool success = browser;

  if (browser) {
    for (size_t i = 0; i < files.size(); ++i) {
      bool handled = file_manager_util::ExecuteBuiltinHandler(
          browser, files[i], internal_task_id);
      if (!handled && files.size() == 1)
        success = false;
    }
  }

  SetResult(Value::CreateBooleanValue(success));
  SendResponse(true);
  return true;
}

SelectFilesFunction::SelectFilesFunction() {
}

SelectFilesFunction::~SelectFilesFunction() {
}

bool SelectFilesFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  UrlList file_urls;
  file_urls.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    file_urls.push_back(GURL(virtual_path));
  }

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_urls,
      base::Bind(&SelectFilesFunction::GetLocalPathsResponseOnUIThread, this));
  return true;
}

void SelectFilesFunction::GetLocalPathsResponseOnUIThread(
    const SelectedFileInfoList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int32 tab_id = GetTabId();
  SelectFileDialogExtension::OnMultiFilesSelected(tab_id, files);
  SendResponse(true);
}

bool CancelFileDialogFunction::RunImpl() {
  int32 tab_id = GetTabId();
  SelectFileDialogExtension::OnFileSelectionCanceled(tab_id);
  SendResponse(true);
  return true;
}

AddMountFunction::AddMountFunction() {
}

AddMountFunction::~AddMountFunction() {
}

bool AddMountFunction::RunImpl() {
  // The third argument is simply ignored.
  if (args_->GetSize() != 2 && args_->GetSize() != 3) {
    error_ = "Invalid argument count";
    return false;
  }

  std::string file_url;
  if (!args_->GetString(0, &file_url)) {
    return false;
  }

  std::string mount_type_str;
  if (!args_->GetString(1, &mount_type_str)) {
    return false;
  }

  // Set default return source path to the empty string.
  SetResult(new base::StringValue(""));

  chromeos::MountType mount_type =
      DiskMountManager::MountTypeFromString(mount_type_str);
  switch (mount_type) {
    case chromeos::MOUNT_TYPE_INVALID: {
      error_ = "Invalid mount type";
      SendResponse(false);
      break;
    }
    case chromeos::MOUNT_TYPE_GOOGLE_DRIVE: {
      const bool success = true;
      // Pass back the drive mount point path as source path.
      const std::string& drive_path =
          drive::util::GetDriveMountPointPathAsString();
      SetResult(new base::StringValue(drive_path));
      FileBrowserPrivateAPI::Get(profile_)->event_router()->
          MountDrive(base::Bind(&AddMountFunction::SendResponse,
                                this,
                                success));
      break;
    }
    default: {
      UrlList file_paths;
      file_paths.push_back(GURL(file_url));

      GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
          file_paths,
          base::Bind(&AddMountFunction::GetLocalPathsResponseOnUIThread,
                     this,
                     mount_type_str));
      break;
    }
  }

  return true;
}

void AddMountFunction::GetLocalPathsResponseOnUIThread(
    const std::string& mount_type_str,
    const SelectedFileInfoList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!files.size()) {
    SendResponse(false);
    return;
  }

  const FilePath& source_path = files[0].local_path;
  const FilePath::StringType& display_name = files[0].display_name;
  // Check if the source path is under Drive cache directory.
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  drive::DriveCache* cache = system_service ? system_service->cache() : NULL;
  if (cache && cache->IsUnderDriveCacheDirectory(source_path)) {
    cache->MarkAsMounted(
        source_path,
        base::Bind(&AddMountFunction::OnMountedStateSet, this, mount_type_str,
                   display_name));
  } else {
    OnMountedStateSet(mount_type_str, display_name,
                      drive::DRIVE_FILE_OK, source_path);
  }
}

void AddMountFunction::OnMountedStateSet(const std::string& mount_type,
                                         const FilePath::StringType& file_name,
                                         drive::DriveFileError error,
                                         const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  // Pass back the actual source path of the mount point.
  SetResult(new base::StringValue(file_path.value()));
  SendResponse(true);
  // MountPath() takes a std::string.
  disk_mount_manager->MountPath(file_path.AsUTF8Unsafe(),
                                FilePath(file_name).Extension(), file_name,
                                DiskMountManager::MountTypeFromString(
                                    mount_type));
}

RemoveMountFunction::RemoveMountFunction() {
}

RemoveMountFunction::~RemoveMountFunction() {
}

bool RemoveMountFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_path;
  if (!args_->GetString(0, &mount_path)) {
    return false;
  }

  UrlList file_paths;
  file_paths.push_back(GURL(mount_path));
  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_paths,
      base::Bind(&RemoveMountFunction::GetLocalPathsResponseOnUIThread, this));
  return true;
}

void RemoveMountFunction::GetLocalPathsResponseOnUIThread(
    const SelectedFileInfoList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  DiskMountManager::GetInstance()->UnmountPath(files[0].local_path.value(),
                                               chromeos::UNMOUNT_OPTIONS_NONE);
  SendResponse(true);
}

GetMountPointsFunction::GetMountPointsFunction() {
}

GetMountPointsFunction::~GetMountPointsFunction() {
}

bool GetMountPointsFunction::RunImpl() {
  if (args_->GetSize())
    return false;

  base::ListValue *mounts = new base::ListValue();
  SetResult(mounts);

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  DiskMountManager::MountPointMap mount_points =
      disk_mount_manager->mount_points();

  for (DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end();
       ++it) {
    mounts->Append(CreateValueFromMountPoint(profile_, it->second,
        extension_->id(), source_url_));
  }

  SendResponse(true);
  return true;
}

SetLastModifiedFunction::SetLastModifiedFunction() {
}

SetLastModifiedFunction::~SetLastModifiedFunction() {
}

bool SetLastModifiedFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }

  std::string file_url;
  if (!args_->GetString(0, &file_url))
    return false;

  std::string timestamp;
  if (!args_->GetString(1, &timestamp))
    return false;

  BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &SetLastModifiedFunction::RunOperationOnFileThread,
            this,
            file_url,
            strtoul(timestamp.c_str(), NULL, 0)));

  return true;
}

void SetLastModifiedFunction::RunOperationOnFileThread(std::string file_url,
                                                       time_t timestamp) {
  bool succeeded = false;

  FilePath local_path = GetLocalPathFromURL(GURL(file_url));
  if (!local_path.empty()) {
    struct stat sb;
    if (stat(local_path.value().c_str(), &sb) == 0) {
      struct utimbuf times;
      times.actime = sb.st_atime;
      times.modtime = timestamp;

      if (utime(local_path.value().c_str(), &times) == 0)
        succeeded = true;
    }
  }

  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &SetLastModifiedFunction::SendResponse,
            this,
            succeeded));
}

GetSizeStatsFunction::GetSizeStatsFunction() {
}

GetSizeStatsFunction::~GetSizeStatsFunction() {
}

bool GetSizeStatsFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_url;
  if (!args_->GetString(0, &mount_url))
    return false;

  FilePath file_path = GetLocalPathFromURL(GURL(mount_url));
  if (file_path.empty())
    return false;

  if (file_path == drive::util::GetDriveMountPointPath()) {
    drive::DriveSystemService* system_service =
        drive::DriveSystemServiceFactory::GetForProfile(profile_);
    // |system_service| is NULL if Drive is disabled.
    if (!system_service) {
      // If stats couldn't be gotten for drive, result should be left
      // undefined. See comments in GetDriveAvailableSpaceCallback().
      SendResponse(true);
      return true;
    }

    drive::DriveFileSystemInterface* file_system =
        system_service->file_system();

    file_system->GetAvailableSpace(
        base::Bind(&GetSizeStatsFunction::GetDriveAvailableSpaceCallback,
                   this));

  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &GetSizeStatsFunction::CallGetSizeStatsOnFileThread,
            this,
            file_path.value()));
  }
  return true;
}

void GetSizeStatsFunction::GetDriveAvailableSpaceCallback(
    drive::DriveFileError error,
    int64 bytes_total,
    int64 bytes_used) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == drive::DRIVE_FILE_OK) {
    int64 bytes_remaining = bytes_total - bytes_used;
    GetSizeStatsCallbackOnUIThread(static_cast<size_t>(bytes_total/1024),
                                   static_cast<size_t>(bytes_remaining/1024));
  } else {
    // If stats couldn't be gotten for drive, result should be left undefined.
    SendResponse(true);
  }
}

void GetSizeStatsFunction::CallGetSizeStatsOnFileThread(
    const std::string& mount_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  size_t total_size_kb = 0;
  size_t remaining_size_kb = 0;
  GetSizeStatsOnFileThread(mount_path, &total_size_kb, &remaining_size_kb);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread,
          this,
          total_size_kb, remaining_size_kb));
}

void GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread(
    size_t total_size_kb,
    size_t remaining_size_kb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* sizes = new base::DictionaryValue();
  SetResult(sizes);

  sizes->SetInteger("totalSizeKB", total_size_kb);
  sizes->SetInteger("remainingSizeKB", remaining_size_kb);

  SendResponse(true);
}

FormatDeviceFunction::FormatDeviceFunction() {
}

FormatDeviceFunction::~FormatDeviceFunction() {
}

bool FormatDeviceFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string volume_file_url;
  if (!args_->GetString(0, &volume_file_url)) {
    NOTREACHED();
    return false;
  }

  FilePath file_path = GetLocalPathFromURL(GURL(volume_file_url));
  if (file_path.empty())
    return false;

  DiskMountManager::GetInstance()->FormatMountedDevice(file_path.value());
  SendResponse(true);
  return true;
}

GetVolumeMetadataFunction::GetVolumeMetadataFunction() {
}

GetVolumeMetadataFunction::~GetVolumeMetadataFunction() {
}

bool GetVolumeMetadataFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    error_ = "Invalid argument count";
    return false;
  }

  std::string volume_mount_url;
  if (!args_->GetString(0, &volume_mount_url)) {
    NOTREACHED();
    return false;
  }

  FilePath file_path = GetLocalPathFromURL(GURL(volume_mount_url));
  if (file_path.empty()) {
    error_ = "Invalid mount path.";
    return false;
  }

  results_.reset();

  const DiskMountManager::Disk* volume = GetVolumeAsDisk(file_path.value());
  if (volume) {
    DictionaryValue* volume_info =
        CreateValueFromDisk(profile_, extension_->id(), volume);
    SetResult(volume_info);
  }

  SendResponse(true);
  return true;
}

bool ToggleFullscreenFunction::RunImpl() {
  Browser* browser = GetCurrentBrowser();
  if (browser) {
    browser->ToggleFullscreenModeWithExtension(
        file_manager_util::GetFileBrowserExtensionUrl());
  }
  return true;
}

bool IsFullscreenFunction::RunImpl() {
  Browser* browser = GetCurrentBrowser();
  SetResult(Value::CreateBooleanValue(
      browser && browser->window() && browser->window()->IsFullscreen()));
  return true;
}

bool FileDialogStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

#define SET_STRING(ns, id) \
  dict->SetString(#id, l10n_util::GetStringUTF16(ns##_##id))

  SET_STRING(IDS, WEB_FONT_FAMILY);
  SET_STRING(IDS, WEB_FONT_SIZE);

  SET_STRING(IDS_FILE_BROWSER, ROOT_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, ARCHIVE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, REMOVABLE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, NAME_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_KB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_MB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_GB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_TB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_PB);

  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_CTRL);
  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_ALT);
  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_SHIFT);
  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_META);
  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_SPACE);
  SET_STRING(IDS_FILE_BROWSER, SHORTCUT_ENTER);

  SET_STRING(IDS_FILE_BROWSER, TYPE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DATE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, PREVIEW_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, OFFLINE_COLUMN_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_WARNING);

  SET_STRING(IDS_FILE_BROWSER, ERROR_CREATING_FOLDER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_INVALID_CHARACTER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_RESERVED_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_HIDDEN_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_WHITESPACE_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_NEW_FOLDER_EMPTY_NAME);
  SET_STRING(IDS_FILE_BROWSER, NEW_FOLDER_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, FILENAME_LABEL);
  SET_STRING(IDS_FILE_BROWSER, PREPARING_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DRAGGING_MULTIPLE_ITEMS);

  SET_STRING(IDS_FILE_BROWSER, DIMENSIONS_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DIMENSIONS_FORMAT);

  SET_STRING(IDS_FILE_BROWSER, IMAGE_DIMENSIONS);
  SET_STRING(IDS_FILE_BROWSER, VOLUME_LABEL);
  SET_STRING(IDS_FILE_BROWSER, READ_ONLY);

  SET_STRING(IDS_FILE_BROWSER, ARCHIVE_MOUNT_FAILED);
  SET_STRING(IDS_FILE_BROWSER, UNMOUNT_FAILED);
  SET_STRING(IDS_FILE_BROWSER, MOUNT_ARCHIVE);
  SET_STRING(IDS_FILE_BROWSER, FORMAT_DEVICE_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, UNMOUNT_DEVICE_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, CLOSE_ARCHIVE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, SEARCH_TEXT_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ACTION_VIEW);
  SET_STRING(IDS_FILE_BROWSER, ACTION_OPEN);
  SET_STRING(IDS_FILE_BROWSER, ACTION_OPEN_GDOC);
  SET_STRING(IDS_FILE_BROWSER, ACTION_OPEN_GSHEET);
  SET_STRING(IDS_FILE_BROWSER, ACTION_OPEN_GSLIDES);
  SET_STRING(IDS_FILE_BROWSER, ACTION_WATCH);
  SET_STRING(IDS_FILE_BROWSER, ACTION_LISTEN);
  SET_STRING(IDS_FILE_BROWSER, INSTALL_CRX);
  SET_STRING(IDS_FILE_BROWSER, SEND_TO_DRIVE);

  SET_STRING(IDS_FILE_BROWSER, GALLERY_NO_IMAGES);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_ITEMS_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_MOSAIC);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_SLIDE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_DELETE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_SLIDESHOW);

  SET_STRING(IDS_FILE_BROWSER, GALLERY_EDIT);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_SHARE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_ENTER_WHEN_DONE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_AUTOFIX);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_FIXED);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_CROP);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_EXPOSURE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_BRIGHTNESS);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_CONTRAST);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_ROTATE_LEFT);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_ROTATE_RIGHT);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_UNDO);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_REDO);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_FILE_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_SAVED);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_OVERWRITE_ORIGINAL);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_OVERWRITE_BUBBLE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_UNSAVED_CHANGES);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_READONLY_WARNING);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_IMAGE_ERROR);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_IMAGE_TOO_BIG_ERROR);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_VIDEO_ERROR);
  SET_STRING(IDS_FILE_BROWSER, AUDIO_ERROR);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_IMAGE_OFFLINE);
  SET_STRING(IDS_FILE_BROWSER, GALLERY_VIDEO_OFFLINE);
  SET_STRING(IDS_FILE_BROWSER, AUDIO_OFFLINE);
  // Reusing strings, but with alias starting with GALLERY.
  dict->SetString("GALLERY_FILE_HIDDEN_NAME",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_HIDDEN_NAME));
  dict->SetString("GALLERY_OK_LABEL",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OK_LABEL));
  dict->SetString("GALLERY_CANCEL_LABEL",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL));
  dict->SetString("GALLERY_CONFIRM_DELETE_ONE",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CONFIRM_DELETE_ONE));
  dict->SetString("GALLERY_CONFIRM_DELETE_SOME",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CONFIRM_DELETE_SOME));

  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_PHOTOS_DRIVE);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_DRIVE_NOT_REACHED);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_VIEW_FILES);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_WATCH_SINGLE_VIDEO);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_OK);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_COUNTER_NO_MEDIA);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_COUNTER);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_LOADING_USB);
  SET_STRING(IDS_FILE_BROWSER, ACTION_CHOICE_LOADING_SD);

  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_TITLE);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_IMPORT_BUTTON);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_CANCEL_BUTTON);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_DRIVE_ERROR);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_SOURCE_ERROR);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_UNKNOWN_DATE);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_NEW_ALBUM_NAME);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_SELECT_ALBUM_CAPTION);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_SELECT_ALBUM_CAPTION_PLURAL);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_IMPORTING_ERROR);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_IMPORTING);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_IMPORT_COMPLETE);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_CAPTION);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_ONE_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_MANY_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_SELECT_ALL);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_SELECT_NONE);
  SET_STRING(IDS_FILE_BROWSER, PHOTO_IMPORT_DELETE_AFTER);

  SET_STRING(IDS_FILE_BROWSER, CONFIRM_OVERWRITE_FILE);
  SET_STRING(IDS_FILE_BROWSER, FILE_ALREADY_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, DIRECTORY_ALREADY_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, ERROR_RENAMING);
  SET_STRING(IDS_FILE_BROWSER, RENAME_PROMPT);
  SET_STRING(IDS_FILE_BROWSER, RENAME_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_DELETING);
  SET_STRING(IDS_FILE_BROWSER, DELETE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, PASTE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, COPY_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, CUT_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, ZIP_SELECTION_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, OPEN_WITH_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, TRANSFER_ITEMS_REMAINING);
  SET_STRING(IDS_FILE_BROWSER, TRANSFER_CANCELLED);
  SET_STRING(IDS_FILE_BROWSER, TRANSFER_TARGET_EXISTS_ERROR);
  SET_STRING(IDS_FILE_BROWSER, TRANSFER_FILESYSTEM_ERROR);
  SET_STRING(IDS_FILE_BROWSER, TRANSFER_UNEXPECTED_ERROR);
  SET_STRING(IDS_FILE_BROWSER, COPY_FILE_NAME);
  SET_STRING(IDS_FILE_BROWSER, COPY_ITEMS_REMAINING);
  SET_STRING(IDS_FILE_BROWSER, COPY_CANCELLED);
  SET_STRING(IDS_FILE_BROWSER, COPY_TARGET_EXISTS_ERROR);
  SET_STRING(IDS_FILE_BROWSER, COPY_FILESYSTEM_ERROR);
  SET_STRING(IDS_FILE_BROWSER, COPY_UNEXPECTED_ERROR);
  SET_STRING(IDS_FILE_BROWSER, MOVE_FILE_NAME);
  SET_STRING(IDS_FILE_BROWSER, MOVE_ITEMS_REMAINING);
  SET_STRING(IDS_FILE_BROWSER, MOVE_CANCELLED);
  SET_STRING(IDS_FILE_BROWSER, MOVE_TARGET_EXISTS_ERROR);
  SET_STRING(IDS_FILE_BROWSER, MOVE_FILESYSTEM_ERROR);
  SET_STRING(IDS_FILE_BROWSER, MOVE_UNEXPECTED_ERROR);
  SET_STRING(IDS_FILE_BROWSER, ZIP_FILE_NAME);
  SET_STRING(IDS_FILE_BROWSER, ZIP_ITEMS_REMAINING);
  SET_STRING(IDS_FILE_BROWSER, ZIP_CANCELLED);
  SET_STRING(IDS_FILE_BROWSER, ZIP_TARGET_EXISTS_ERROR);
  SET_STRING(IDS_FILE_BROWSER, ZIP_FILESYSTEM_ERROR);
  SET_STRING(IDS_FILE_BROWSER, ZIP_UNEXPECTED_ERROR);

  SET_STRING(IDS_FILE_BROWSER, DELETED_MESSAGE_PLURAL);
  SET_STRING(IDS_FILE_BROWSER, DELETED_MESSAGE);
  SET_STRING(IDS_FILE_BROWSER, UNDO_DELETE);

  SET_STRING(IDS_FILE_BROWSER, CANCEL_LABEL);
  SET_STRING(IDS_FILE_BROWSER, OPEN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SAVE_LABEL);
  SET_STRING(IDS_FILE_BROWSER, OK_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DEFAULT_NEW_FOLDER_NAME);
  SET_STRING(IDS_FILE_BROWSER, MORE_FILES);

  SET_STRING(IDS_FILE_BROWSER, CONFIRM_DELETE_ONE);
  SET_STRING(IDS_FILE_BROWSER, CONFIRM_DELETE_SOME);

  SET_STRING(IDS_FILE_BROWSER, UNKNOWN_FILESYSTEM_WARNING);
  SET_STRING(IDS_FILE_BROWSER, UNSUPPORTED_FILESYSTEM_WARNING);
  SET_STRING(IDS_FILE_BROWSER, FORMATTING_WARNING);

  SET_STRING(IDS_FILE_BROWSER, DRIVE_MENU_HELP);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_SHOW_HOSTED_FILES_OPTION);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_MOBILE_CONNECTION_OPTION);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_CLEAR_LOCAL_CACHE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_RELOAD);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_SPACE_AVAILABLE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_SPACE_AVAILABLE_LONG);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WAITING_FOR_SPACE_INFO);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_FAILED_SPACE_INFO);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_BUY_MORE_SPACE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_BUY_MORE_SPACE_LINK);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_VISIT_DRIVE_GOOGLE_COM);

  SET_STRING(IDS_FILE_BROWSER, SELECT_FOLDER_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_MULTI_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_SAVEAS_FILE_TITLE);

  SET_STRING(IDS_FILE_BROWSER, COMPUTING_SELECTION);
  SET_STRING(IDS_FILE_BROWSER, MANY_FILES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_DIRECTORIES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_ENTRIES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, CALCULATING_SIZE);

  SET_STRING(IDS_FILE_BROWSER, OFFLINE_HEADER);
  SET_STRING(IDS_FILE_BROWSER, OFFLINE_MESSAGE);
  SET_STRING(IDS_FILE_BROWSER, OFFLINE_MESSAGE_PLURAL);
  SET_STRING(IDS_FILE_BROWSER, HOSTED_OFFLINE_MESSAGE);
  SET_STRING(IDS_FILE_BROWSER, HOSTED_OFFLINE_MESSAGE_PLURAL);
  SET_STRING(IDS_FILE_BROWSER, CONFIRM_MOBILE_DATA_USE);
  SET_STRING(IDS_FILE_BROWSER, CONFIRM_MOBILE_DATA_USE_PLURAL);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_OUT_OF_SPACE_HEADER);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_OUT_OF_SPACE_MESSAGE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_SERVER_OUT_OF_SPACE_HEADER);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_SERVER_OUT_OF_SPACE_MESSAGE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_TITLE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_TEXT_SHORT);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_TEXT_LONG);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_DISMISS);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_TITLE_ALTERNATIVE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_WELCOME_GET_STARTED);
  SET_STRING(IDS_FILE_BROWSER, NO_ACTION_FOR_FILE);

  // MP3 metadata extractor plugin
  SET_STRING(IDS_FILE_BROWSER, ID3_ALBUM);  // TALB
  SET_STRING(IDS_FILE_BROWSER, ID3_BPM);  // TBPM
  SET_STRING(IDS_FILE_BROWSER, ID3_COMPOSER);  // TCOM
  SET_STRING(IDS_FILE_BROWSER, ID3_COPYRIGHT_MESSAGE);  // TCOP
  SET_STRING(IDS_FILE_BROWSER, ID3_DATE);  // TDAT
  SET_STRING(IDS_FILE_BROWSER, ID3_PLAYLIST_DELAY);  // TDLY
  SET_STRING(IDS_FILE_BROWSER, ID3_ENCODED_BY);  // TENC
  SET_STRING(IDS_FILE_BROWSER, ID3_LYRICIST);  // TEXT
  SET_STRING(IDS_FILE_BROWSER, ID3_FILE_TYPE);  // TFLT
  SET_STRING(IDS_FILE_BROWSER, ID3_TIME);  // TIME
  SET_STRING(IDS_FILE_BROWSER, ID3_TITLE);  // TIT2
  SET_STRING(IDS_FILE_BROWSER, ID3_LENGTH);  // TLEN
  SET_STRING(IDS_FILE_BROWSER, ID3_FILE_OWNER);  // TOWN
  SET_STRING(IDS_FILE_BROWSER, ID3_LEAD_PERFORMER);  // TPE1
  SET_STRING(IDS_FILE_BROWSER, ID3_BAND);  // TPE2
  SET_STRING(IDS_FILE_BROWSER, ID3_TRACK_NUMBER);  // TRCK
  SET_STRING(IDS_FILE_BROWSER, ID3_YEAR);  // TYER
  SET_STRING(IDS_FILE_BROWSER, ID3_COPYRIGHT);  // WCOP
  SET_STRING(IDS_FILE_BROWSER, ID3_OFFICIAL_AUDIO_FILE_WEBPAGE);  // WOAF
  SET_STRING(IDS_FILE_BROWSER, ID3_OFFICIAL_ARTIST);  // WOAR
  SET_STRING(IDS_FILE_BROWSER, ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE);  // WOAS
  SET_STRING(IDS_FILE_BROWSER, ID3_PUBLISHERS_OFFICIAL_WEBPAGE);  // WPUB
  SET_STRING(IDS_FILE_BROWSER, ID3_USER_DEFINED_URL_LINK_FRAME);  // WXXX

  // File types
  SET_STRING(IDS_FILE_BROWSER, FOLDER);
  SET_STRING(IDS_FILE_BROWSER, GENERIC_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, NO_EXTENSION_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, DEVICE);
  SET_STRING(IDS_FILE_BROWSER, IMAGE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, VIDEO_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, AUDIO_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, HTML_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, ZIP_ARCHIVE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, RAR_ARCHIVE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, TAR_ARCHIVE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, TAR_BZIP2_ARCHIVE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, TAR_GZIP_ARCHIVE_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, PLAIN_TEXT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, PDF_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, WORD_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, POWERPOINT_PRESENTATION_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, EXCEL_FILE_TYPE);

  SET_STRING(IDS_FILE_BROWSER, GDOC_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GSHEET_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GSLIDES_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GDRAW_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GTABLE_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GLINK_DOCUMENT_FILE_TYPE);

  SET_STRING(IDS_FILE_BROWSER, DRIVE_LOADING);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_LOADING_PROGRESS);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_CANNOT_REACH);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_LEARN_MORE);
  SET_STRING(IDS_FILE_BROWSER, DRIVE_RETRY);

  SET_STRING(IDS_FILE_BROWSER, AUDIO_PLAYER_TITLE);
  SET_STRING(IDS_FILE_BROWSER, AUDIO_PLAYER_DEFAULT_ARTIST);

  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_GENERIC);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_NOT_FOUND);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_SECURITY);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_NOT_READABLE);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_NO_MODIFICATION_ALLOWED);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_INVALID_STATE);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_INVALID_MODIFICATION);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_PATH_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, FILE_ERROR_QUOTA_EXCEEDED);

  SET_STRING(IDS_FILE_BROWSER, SEARCH_NO_MATCHING_FILES);
  SET_STRING(IDS_FILE_BROWSER, SEARCH_EXPAND);
  SET_STRING(IDS_FILE_BROWSER, SEARCH_SPINNER);

  SET_STRING(IDS_FILE_BROWSER, CHANGE_DEFAULT_MENU_ITEM);
  SET_STRING(IDS_FILE_BROWSER, CHANGE_DEFAULT_CAPTION);
  SET_STRING(IDS_FILE_BROWSER, DEFAULT_ACTION_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DETAIL_VIEW_TOOLTIP);
  SET_STRING(IDS_FILE_BROWSER, THUMBNAIL_VIEW_TOOLTIP);

  SET_STRING(IDS_FILE_BROWSER, TIME_TODAY);
  SET_STRING(IDS_FILE_BROWSER, TIME_YESTERDAY);

  SET_STRING(IDS_FILE_BROWSER, ALL_FILES_FILTER);
#undef SET_STRING

  dict->SetBoolean("PDF_VIEW_ENABLED",
      file_manager_util::ShouldBeOpenedWithPdfPlugin(profile(), ".pdf"));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(dict);

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  dict->SetBoolean("ENABLE_GDATA", system_service != NULL);

#if defined(USE_ASH)
  dict->SetBoolean("ASH", true);
#else
  dict->SetBoolean("ASH", false);
#endif

  std::string board;
  const char kMachineInfoBoard[] = "CHROMEOS_RELEASE_BOARD";
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(kMachineInfoBoard, &board))
    board = "unknown";
  dict->SetString(kMachineInfoBoard, board);

  return true;
}

GetDriveFilePropertiesFunction::GetDriveFilePropertiesFunction() {
}

GetDriveFilePropertiesFunction::~GetDriveFilePropertiesFunction() {
}

void GetDriveFilePropertiesFunction::DoOperation(
    const FilePath& file_path,
    base::DictionaryValue* property_dict,
    scoped_ptr<drive::DriveEntryProto> entry_proto) {
  DCHECK(property_dict);

  // Nothing to do here so simply call OnOperationComplete().
  OnOperationComplete(file_path,
                      property_dict,
                      drive::DRIVE_FILE_OK,
                      entry_proto.Pass());
}

bool GetDriveFilePropertiesFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (args_->GetSize() != 1)
    return false;

  PrepareResults();
  return true;
}

void GetDriveFilePropertiesFunction::PrepareResults() {
  args_->GetList(0, &path_list_);
  DCHECK(path_list_);

  file_properties_.reset(new base::ListValue);

  current_index_ = 0;
  GetNextFileProperties();
}

void GetDriveFilePropertiesFunction::GetNextFileProperties() {
  if (current_index_ >= path_list_->GetSize()) {
    // Exit of asynchronous look and return the result.
    SetResult(file_properties_.release());
    SendResponse(true);
    return;
  }

  std::string file_str;
  path_list_->GetString(current_index_, &file_str);
  GURL file_url = GURL(file_str);
  FilePath file_path = GetVirtualPathFromURL(file_url);

  base::DictionaryValue* property_dict = new base::DictionaryValue;
  property_dict->SetString("fileUrl", file_url.spec());
  file_properties_->Append(property_dict);

  // Start getting the file info.
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service) {
    OnOperationComplete(file_path,
                        property_dict,
                        drive::DRIVE_FILE_ERROR_FAILED,
                        scoped_ptr<drive::DriveEntryProto>());
    return;
  }

  system_service->file_system()->GetEntryInfoByPath(
      file_path,
      base::Bind(&GetDriveFilePropertiesFunction::OnGetFileInfo,
                 this,
                 file_path,
                 property_dict));
}

void GetDriveFilePropertiesFunction::CompleteGetFileProperties() {
  current_index_++;

  // Could be called from callback. Let finish operation.
  MessageLoop::current()->PostTask(FROM_HERE,
      Bind(&GetDriveFilePropertiesFunction::GetNextFileProperties, this));
}

void GetDriveFilePropertiesFunction::OnGetFileInfo(
    const FilePath& file_path,
    base::DictionaryValue* property_dict,
    drive::DriveFileError error,
    scoped_ptr<drive::DriveEntryProto> entry_proto) {
  DCHECK(property_dict);

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = drive::DRIVE_FILE_ERROR_NOT_FOUND;

  if (error == drive::DRIVE_FILE_OK)
    DoOperation(file_path, property_dict, entry_proto.Pass());
  else
    OnOperationComplete(file_path, property_dict, error, entry_proto.Pass());
}

void GetDriveFilePropertiesFunction::OnOperationComplete(
    const FilePath& file_path,
    base::DictionaryValue* property_dict,
    drive::DriveFileError error,
    scoped_ptr<drive::DriveEntryProto> entry_proto) {
  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = drive::DRIVE_FILE_ERROR_NOT_FOUND;

  if (error != drive::DRIVE_FILE_OK) {
    property_dict->SetInteger("errorCode", error);
    CompleteGetFileProperties();
    return;
  }
  DCHECK(entry_proto.get());

  const drive::DriveFileSpecificInfo& file_specific_info =
      entry_proto->file_specific_info();

  FillDriveFilePropertiesValue(*entry_proto.get(), property_dict);

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service) {
    property_dict->SetInteger("errorCode", error);
    CompleteGetFileProperties();
    return;
  }

  // Get drive WebApps that can accept this file.
  ScopedVector<drive::DriveWebAppInfo> web_apps;
  system_service->webapps_registry()->GetWebAppsForFile(
          file_path, file_specific_info.content_mime_type(), &web_apps);
  if (!web_apps.empty()) {
    std::string default_task_id = file_handler_util::GetDefaultTaskIdFromPrefs(
        profile_,
        file_specific_info.content_mime_type(),
        file_path.Extension());
    std::string default_app_id;
    file_handler_util::CrackTaskID(
        default_task_id, &default_app_id, NULL, NULL);

    ListValue* apps = new ListValue();
    property_dict->Set("driveApps", apps);
    for (ScopedVector<drive::DriveWebAppInfo>::const_iterator it =
             web_apps.begin();
         it != web_apps.end(); ++it) {
      const drive::DriveWebAppInfo* webapp_info = *it;
      DictionaryValue* app = new DictionaryValue();
      app->SetString("appId", webapp_info->app_id);
      app->SetString("appName", webapp_info->app_name);
      GURL app_icon = FindPreferredIcon(webapp_info->app_icons,
                                        kPreferredIconSize);
      if (!app_icon.is_empty())
        app->SetString("appIcon", app_icon.spec());
      GURL doc_icon = FindPreferredIcon(webapp_info->document_icons,
                                        kPreferredIconSize);
      if (!doc_icon.is_empty())
        app->SetString("docIcon", doc_icon.spec());
      app->SetString("objectType", webapp_info->object_type);
      app->SetBoolean("isPrimary", default_app_id == webapp_info->app_id);
      apps->Append(app);
    }
  }

  if (VLOG_IS_ON(2)) {
    std::string result_json;
    base::JSONWriter::WriteWithOptions(
        property_dict,
        base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &result_json);
    VLOG(2) << "Drive File Properties:\n" << result_json;
  }

  system_service->cache()->GetCacheEntry(
      entry_proto->resource_id(),
      file_specific_info.file_md5(),
      base::Bind(
          &GetDriveFilePropertiesFunction::CacheStateReceived,
          this, property_dict));
}

void GetDriveFilePropertiesFunction::CacheStateReceived(
    base::DictionaryValue* property_dict,
    bool /* success */,
    const drive::DriveCacheEntry& cache_entry) {
  // In case of an error (i.e. success is false), cache_entry.is_*() all
  // returns false.
  property_dict->SetBoolean("isPinned", cache_entry.is_pinned());
  property_dict->SetBoolean("isPresent", cache_entry.is_present());
  property_dict->SetBoolean("isDirty", cache_entry.is_dirty());

  CompleteGetFileProperties();
}

PinDriveFileFunction::PinDriveFileFunction() {
}

PinDriveFileFunction::~PinDriveFileFunction() {
}

bool PinDriveFileFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (args_->GetSize() != 2 || !args_->GetBoolean(1, &set_pin_))
    return false;

  PrepareResults();

  return true;
}

void PinDriveFileFunction::DoOperation(
    const FilePath& file_path,
    base::DictionaryValue* properties,
    scoped_ptr<drive::DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service) {
    OnOperationComplete(file_path,
                        properties,
                        drive::DRIVE_FILE_ERROR_FAILED,
                        entry_proto.Pass());
    return;
  }

  // This is subtle but we should take references of resource_id and md5
  // before |file_info| is passed to |callback| by base::Passed(). Otherwise,
  // file_info->whatever() crashes.
  const std::string& resource_id = entry_proto->resource_id();
  const std::string& md5 = entry_proto->file_specific_info().file_md5();
  const drive::FileOperationCallback callback =
      base::Bind(&PinDriveFileFunction::OnPinStateSet,
                 this,
                 file_path,
                 properties,
                 base::Passed(&entry_proto));

  if (set_pin_)
    system_service->cache()->Pin(resource_id, md5, callback);
  else
    system_service->cache()->Unpin(resource_id, md5, callback);
}

void PinDriveFileFunction::OnPinStateSet(
    const FilePath& path,
    base::DictionaryValue* properties,
    scoped_ptr<drive::DriveEntryProto> entry_proto,
    drive::DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OnOperationComplete(path, properties, error, entry_proto.Pass());
}

GetFileLocationsFunction::GetFileLocationsFunction() {
}

GetFileLocationsFunction::~GetFileLocationsFunction() {
}

bool GetFileLocationsFunction::RunImpl() {
  ListValue* file_urls_as_strings = NULL;
  if (!args_->GetList(0, &file_urls_as_strings))
    return false;

  // Convert the list of strings to a list of GURLs.
  scoped_ptr<ListValue> locations(new ListValue);
  for (size_t i = 0; i < file_urls_as_strings->GetSize(); ++i) {
    std::string file_url_as_string;
    if (!file_urls_as_strings->GetString(i, &file_url_as_string))
      return false;

    fileapi::FileSystemURL url((GURL(file_url_as_string)));
    if (url.type() == fileapi::kFileSystemTypeDrive)
      locations->Append(new base::StringValue("drive"));
    else
      locations->Append(new base::StringValue("local"));
  }

  SetResult(locations.release());
  SendResponse(true);
  return true;
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
  UrlList file_urls;
  for (size_t i = 0; i < file_urls_as_strings->GetSize(); ++i) {
    std::string file_url_as_string;
    if (!file_urls_as_strings->GetString(i, &file_url_as_string))
      return false;
    file_urls.push_back(GURL(file_url_as_string));
  }

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_urls,
      base::Bind(&GetDriveFilesFunction::GetLocalPathsResponseOnUIThread,
                 this));
  return true;
}

void GetDriveFilesFunction::GetLocalPathsResponseOnUIThread(
    const SelectedFileInfoList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < files.size(); ++i) {
    DCHECK(drive::util::IsUnderDriveMountPoint(files[i].file_path));
    FilePath drive_path = drive::util::ExtractDrivePath(files[i].file_path);
    remaining_drive_paths_.push(drive_path);
  }

  local_paths_ = new ListValue;
  GetFileOrSendResponse();
}

void GetDriveFilesFunction::GetFileOrSendResponse() {
  // Send the response if all files are obtained.
  if (remaining_drive_paths_.empty()) {
    SetResult(local_paths_);
    SendResponse(true);
    return;
  }

  // Get the file on the top of the queue.
  FilePath drive_path = remaining_drive_paths_.front();

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service) {
    OnFileReady(drive::DRIVE_FILE_ERROR_FAILED,
                drive_path,
                "",  // mime_type
                drive::REGULAR_FILE);
    return;
  }

  system_service->file_system()->GetFileByPath(
      drive_path,
      base::Bind(&GetDriveFilesFunction::OnFileReady, this),
      google_apis::GetContentCallback());
}


void GetDriveFilesFunction::OnFileReady(
    drive::DriveFileError error,
    const FilePath& local_path,
    const std::string& unused_mime_type,
    drive::DriveFileType file_type) {
  FilePath drive_path = remaining_drive_paths_.front();

  if (error == drive::DRIVE_FILE_OK) {
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

GetFileTransfersFunction::GetFileTransfersFunction() {}

GetFileTransfersFunction::~GetFileTransfersFunction() {}

ListValue* GetFileTransfersFunction::GetFileTransfersList() {
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service)
    return NULL;

  google_apis::OperationProgressStatusList list =
      system_service->drive_service()->GetProgressStatusList();
  return file_manager_util::ProgressStatusVectorToListValue(
      profile_, extension_->id(), list);
}

bool GetFileTransfersFunction::RunImpl() {
  scoped_ptr<ListValue> progress_status_list(GetFileTransfersList());
  if (!progress_status_list.get()) {
    SendResponse(false);
    return false;
  }

  SetResult(progress_status_list.release());
  SendResponse(true);
  return true;
}


CancelFileTransfersFunction::CancelFileTransfersFunction() {}

CancelFileTransfersFunction::~CancelFileTransfersFunction() {}

bool CancelFileTransfersFunction::RunImpl() {
  ListValue* url_list = NULL;
  if (!args_->GetList(0, &url_list))
    return false;

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service)
    return false;

  scoped_ptr<ListValue> responses(new ListValue());
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    std::string url_as_string;
    url_list->GetString(i, &url_as_string);

    FilePath file_path = GetLocalPathFromURL(GURL(url_as_string));
    if (file_path.empty())
      continue;

    DCHECK(drive::util::IsUnderDriveMountPoint(file_path));
    file_path = drive::util::ExtractDrivePath(file_path);
    scoped_ptr<DictionaryValue> result(new DictionaryValue());
    result->SetBoolean(
        "canceled",
        system_service->drive_service()->CancelForFilePath(file_path));
    GURL file_url;
    if (file_manager_util::ConvertFileToFileSystemUrl(profile_,
            drive::util::GetSpecialRemoteRootPath().Append(file_path),
            extension_->id(),
            &file_url)) {
      result->SetString("fileUrl", file_url.spec());
    }
    responses->Append(result.release());
  }
  SetResult(responses.release());
  SendResponse(true);
  return true;
}

TransferFileFunction::TransferFileFunction() {}

TransferFileFunction::~TransferFileFunction() {}

bool TransferFileFunction::RunImpl() {
  std::string source_file_url;
  std::string destination_file_url;
  if (!args_->GetString(0, &source_file_url) ||
      !args_->GetString(1, &destination_file_url)) {
    return false;
  }

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service)
    return false;

  FilePath source_file = GetLocalPathFromURL(GURL(source_file_url));
  FilePath destination_file = GetLocalPathFromURL(GURL(destination_file_url));
  if (source_file.empty() || destination_file.empty())
    return false;

  bool source_file_under_drive =
      drive::util::IsUnderDriveMountPoint(source_file);
  bool destination_file_under_drive =
      drive::util::IsUnderDriveMountPoint(destination_file);

  if (source_file_under_drive && !destination_file_under_drive) {
    // Transfer a file from gdata to local file system.
    source_file = drive::util::ExtractDrivePath(source_file);
    system_service->file_system()->TransferFileFromRemoteToLocal(
        source_file,
        destination_file,
        base::Bind(&TransferFileFunction::OnTransferCompleted, this));
  } else if (!source_file_under_drive && destination_file_under_drive) {
    // Transfer a file from local to Drive file system
    destination_file = drive::util::ExtractDrivePath(destination_file);
    system_service->file_system()->TransferFileFromLocalToRemote(
        source_file,
        destination_file,
        base::Bind(&TransferFileFunction::OnTransferCompleted, this));
  } else {
    // Local-to-local or Drive-to-Drive file transfers should be done via
    // FileEntry.copyTo in the File API and are thus not supported here.
    NOTREACHED();
    SendResponse(false);
  }
  return true;
}

void TransferFileFunction::OnTransferCompleted(drive::DriveFileError error) {
  if (error == drive::DRIVE_FILE_OK) {
    SendResponse(true);
  } else {
    error_ = base::StringPrintf("%d", static_cast<int>(
        fileapi::PlatformFileErrorToWebFileError(
            drive::DriveFileErrorToPlatformError(error))));
    SendResponse(false);
  }
}

// Read preferences.
bool GetPreferencesFunction::RunImpl() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  const PrefService* service = profile_->GetPrefs();

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  bool drive_enabled = (system_service != NULL);

  if (drive_enabled)
    AddDriveMountPoint(profile_, extension_id(), render_view_host());

  value->SetBoolean("driveEnabled", drive_enabled);

  value->SetBoolean("cellularDisabled",
                    service->GetBoolean(prefs::kDisableDriveOverCellular));

  value->SetBoolean("hostedFilesDisabled",
                    service->GetBoolean(prefs::kDisableDriveHostedFiles));

  value->SetBoolean("use24hourClock",
                    service->GetBoolean(prefs::kUse24HourClock));

  SetResult(value.release());
  return true;
}

// Write preferences.
bool SetPreferencesFunction::RunImpl() {
  base::DictionaryValue* value = NULL;

  if (!args_->GetDictionary(0, &value) || !value)
    return false;

  PrefService* service = profile_->GetPrefs();

  bool tmp;

  if (value->GetBoolean("cellularDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveOverCellular, tmp);

  if (value->GetBoolean("hostedFilesDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveHostedFiles, tmp);

  return true;
}

SearchDriveFunction::SearchDriveFunction() {}

SearchDriveFunction::~SearchDriveFunction() {}

bool SearchDriveFunction::RunImpl() {
  DictionaryValue* search_params;
  if (!args_->GetDictionary(0, &search_params))
    return false;

  if (!search_params->GetString("query", &query_))
    return false;

  if (!search_params->GetBoolean("sharedWithMe", &shared_with_me_))
    return false;

  if (!search_params->GetString("nextFeed", &next_feed_))
    return false;

  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  BrowserContext::GetStoragePartition(profile(), site_instance)->
      GetFileSystemContext()->OpenFileSystem(
          source_url_.GetOrigin(), fileapi::kFileSystemTypeExternal, false,
          base::Bind(&SearchDriveFunction::OnFileSystemOpened, this));
  return true;
}

void SearchDriveFunction::OnFileSystemOpened(
    base::PlatformFileError result,
    const std::string& file_system_name,
    const GURL& file_system_url) {
  if (result != base::PLATFORM_FILE_OK) {
    SendResponse(false);
    return;
  }

  file_system_name_ = file_system_name;
  file_system_url_ = file_system_url;

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service || !system_service->file_system()) {
    SendResponse(false);
    return;
  }

  system_service->file_system()->Search(
      query_, shared_with_me_, GURL(next_feed_),
      base::Bind(&SearchDriveFunction::OnSearch, this));
}

void SearchDriveFunction::OnSearch(
    drive::DriveFileError error,
    const GURL& next_feed,
    scoped_ptr<std::vector<drive::SearchResultInfo> > results) {
  if (error != drive::DRIVE_FILE_OK) {
    SendResponse(false);
    return;
  }

  DCHECK(results.get());

  base::ListValue* results_list = new ListValue();

  // Convert Drive files to something File API stack can understand.
  for (size_t i = 0; i < results->size(); ++i) {
    DictionaryValue* result_dict = new DictionaryValue();

    // FileEntry fields.
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("fileSystemName", file_system_name_);
    entry->SetString("fileSystemRoot", file_system_url_.spec());
    entry->SetString("fileFullPath", "/" + results->at(i).path.value());
    entry->SetBoolean("fileIsDirectory",
                      results->at(i).entry_proto.file_info().is_directory());
    result_dict->Set("entry", entry);

    // Drive files properties.
    DictionaryValue* property_dict = new DictionaryValue();
    FillDriveFilePropertiesValue(results->at(i).entry_proto, property_dict);
    result_dict->Set("properties", property_dict);
    results_list->Append(result_dict);
  }

  base::DictionaryValue* result = new DictionaryValue();
  result->Set("results", results_list);
  result->SetString("nextFeed", next_feed.spec());

  SetResult(result);
  SendResponse(true);
}

bool ClearDriveCacheFunction::RunImpl() {
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service || !system_service->file_system())
    return false;

  // TODO(yoshiki): Receive a callback from JS-side and pass it to
  // ClearCacheAndRemountFileSystem(). http://crbug.com/140511
  system_service->ClearCacheAndRemountFileSystem(
      base::Bind(&DoNothingWithBool));

  SendResponse(true);
  return true;
}

bool ReloadDriveFunction::RunImpl() {
  // |system_service| is NULL if Drive is disabled.
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  if (!system_service)
    return false;

  system_service->ReloadAndRemountFileSystem();

  SendResponse(true);
  return true;
}

bool GetNetworkConnectionStateFunction::RunImpl() {
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (!network_library)
    return false;

  const chromeos::Network* active_network = network_library->active_network();

  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetBoolean("online", active_network && active_network->online());

  std::string type_string;
  if (!active_network)
    type_string = "none";
  else if (active_network->type() == chromeos::TYPE_CELLULAR)
    type_string = "cellular";
  else
    type_string = "ethernet";  // Currently we do not care about other types.

  value->SetString("type", type_string);
  SetResult(value.release());

  return true;
}

bool RequestDirectoryRefreshFunction::RunImpl() {
  std::string file_url_as_string;
  if (!args_->GetString(0, &file_url_as_string))
    return false;

  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  // |system_service| is NULL if Drive is disabled.
  if (!system_service || !system_service->file_system())
    return false;

  FilePath directory_path = GetVirtualPathFromURL(GURL(file_url_as_string));
  system_service->file_system()->RequestDirectoryRefresh(directory_path);

  return true;
}

ZipSelectionFunction::ZipSelectionFunction() {
}

ZipSelectionFunction::~ZipSelectionFunction() {
}

bool ZipSelectionFunction::RunImpl() {
  if (args_->GetSize() < 3) {
    return false;
  }

  // First param is the source directory URL.
  std::string dir_url;
  if (!args_->GetString(0, &dir_url) || dir_url.empty())
    return false;

  FilePath src_dir = GetLocalPathFromURL(GURL(dir_url));
  if (src_dir.empty())
    return false;

  // Second param is the list of selected file URLs.
  ListValue* selection_urls = NULL;
  args_->GetList(1, &selection_urls);
  if (!selection_urls || !selection_urls->GetSize())
    return false;

  std::vector<FilePath> files;
  for (size_t i = 0; i < selection_urls->GetSize(); ++i) {
    std::string file_url;
    selection_urls->GetString(i, &file_url);
    FilePath path = GetLocalPathFromURL(GURL(file_url));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  // Third param is the name of the output zip file.
  std::string dest_name;
  if (!args_->GetString(2, &dest_name) || dest_name.empty())
    return false;

  // Check if the dir path is under Drive cache directory.
  // TODO(hshi): support create zip file on Drive (crbug.com/158690).
  drive::DriveSystemService* system_service =
      drive::DriveSystemServiceFactory::GetForProfile(profile_);
  drive::DriveCache* cache = system_service ? system_service->cache() : NULL;
  if (cache && cache->IsUnderDriveCacheDirectory(src_dir))
    return false;

  FilePath dest_file = src_dir.Append(dest_name);
  std::vector<FilePath> src_relative_paths;
  for (size_t i = 0; i != files.size(); ++i) {
    const FilePath& file_path = files[i];

    // Obtain the relative path of |file_path| under |src_dir|.
    FilePath relative_path;
    if (!src_dir.AppendRelativePath(file_path, &relative_path))
      return false;
    src_relative_paths.push_back(relative_path);
  }

  zip_file_creator_ = new ZipFileCreator(this, src_dir, src_relative_paths,
                                         dest_file);
  zip_file_creator_->Start();

  // Keep the refcount until the zipping is complete on utility process.
  AddRef();
  return true;
}

void ZipSelectionFunction::OnZipDone(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
  Release();
}
