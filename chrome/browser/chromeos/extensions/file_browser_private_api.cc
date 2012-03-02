// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#endif

using chromeos::disks::DiskMountManager;
using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;

namespace {

// Error messages.
const char kFileError[] = "File error %d";
const char kInvalidFileUrl[] = "Invalid file URL";
const char kVolumeDevicePathNotFound[] = "Device path not found";

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

const int kReadWriteFilePermissions = base::PLATFORM_FILE_OPEN |
                                      base::PLATFORM_FILE_CREATE |
                                      base::PLATFORM_FILE_OPEN_ALWAYS |
                                      base::PLATFORM_FILE_CREATE_ALWAYS |
                                      base::PLATFORM_FILE_OPEN_TRUNCATED |
                                      base::PLATFORM_FILE_READ |
                                      base::PLATFORM_FILE_WRITE |
                                      base::PLATFORM_FILE_EXCLUSIVE_READ |
                                      base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                                      base::PLATFORM_FILE_ASYNC |
                                      base::PLATFORM_FILE_WRITE_ATTRIBUTES;

struct LastUsedHandler {
  LastUsedHandler(int t, const FileBrowserHandler* h, URLPatternSet p)
      : timestamp(t),
        handler(h),
        patterns(p) {
  }

  int timestamp;
  const FileBrowserHandler* handler;
  URLPatternSet patterns;
};

typedef std::vector<LastUsedHandler> LastUsedHandlerList;
typedef std::set<const FileBrowserHandler*> ActionSet;

// Breaks down task_id that is used between getFileTasks() and executeTask() on
// its building blocks. task_id field the following structure:
//     <extension-id>|<task-action-id>
// Currently, the only supported task-type is of 'context'.
bool CrackTaskIdentifier(const std::string& task_id,
                         std::string* target_extension_id,
                         std::string* action_id) {
  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);
  if (count != 2)
    return false;
  *target_extension_id = result[0];
  *action_id = result[1];
  return true;
}

std::string MakeTaskID(const char* extension_id,
                       const char*  action_id) {
  return base::StringPrintf("%s|%s", extension_id, action_id);
}

// Gets extension specified by extension id in |task_id|.
const Extension* ExtractExtensionFromTaskId(const std::string& task_id,
                                            Profile* profile) {
  // Get task details.
  std::string action_id;
  std::string extension_id;
  if (!CrackTaskIdentifier(task_id, &extension_id, &action_id))
    return NULL;

  ExtensionService* service = profile->GetExtensionService();
  if (!service)
     return NULL;

  return service->GetExtensionById(extension_id, false);
}

// Returns process id of the process the extension is running in.
int ExtractProcessFromExtensionId(const std::string& extension_id,
                                  Profile* profile) {
  GURL extension_url =
      Extension::GetBaseURLFromExtensionId(extension_id);
  ExtensionProcessManager* manager = profile->GetExtensionProcessManager();

  SiteInstance* site_instance = manager->GetSiteInstanceForURL(extension_url);
  if (!site_instance || !site_instance->HasProcess())
    return -1;
  content::RenderProcessHost* process = site_instance->GetProcess();

  return process->GetID();
}

bool GetFileBrowserHandlers(Profile* profile,
                            const GURL& selected_file_url,
                            ActionSet* results) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return false;  // In unit-tests, we may not have an ExtensionService.

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = *iter;
    if (profile->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;
    if (!extension->file_browser_handlers())
      continue;

    for (Extension::FileBrowserHandlerList::const_iterator action_iter =
             extension->file_browser_handlers()->begin();
         action_iter != extension->file_browser_handlers()->end();
         ++action_iter) {
      const FileBrowserHandler* action = action_iter->get();
      if (!action->MatchesURL(selected_file_url))
        continue;

      results->insert(action_iter->get());
    }
  }
  return true;
}

bool SortByLastUsedTimestampDesc(const LastUsedHandler& a,
                                 const LastUsedHandler& b) {
  return a.timestamp > b.timestamp;
}

// TODO(zelidrag): Wire this with ICU to make this sort I18N happy.
bool SortByTaskName(const LastUsedHandler& a, const LastUsedHandler& b) {
  return base::strcasecmp(a.handler->title().data(),
                          b.handler->title().data()) > 0;
}

void SortLastUsedHandlerList(LastUsedHandlerList *list) {
  // Sort by the last used descending.
  std::sort(list->begin(), list->end(), SortByLastUsedTimestampDesc);
  if (list->size() > 1) {
    // Sort the rest by name.
    std::sort(list->begin() + 1, list->end(), SortByTaskName);
  }
}

URLPatternSet GetAllMatchingPatterns(const FileBrowserHandler* handler,
                                     const std::vector<GURL>& files_list) {
  URLPatternSet matching_patterns;
  const URLPatternSet& patterns = handler->file_url_patterns();
  for (URLPatternSet::const_iterator pattern_it = patterns.begin();
       pattern_it != patterns.end(); ++pattern_it) {
    for (std::vector<GURL>::const_iterator file_it = files_list.begin();
         file_it != files_list.end(); ++file_it) {
      if (pattern_it->MatchesURL(*file_it)) {
        matching_patterns.AddPattern(*pattern_it);
        break;
      }
    }
  }

  return matching_patterns;
}

// Given the list of selected files, returns array of context menu tasks
// that are shared
bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     LastUsedHandlerList* named_action_list) {
  named_action_list->clear();
  ActionSet common_tasks;
  for (std::vector<GURL>::const_iterator it = files_list.begin();
       it != files_list.end(); ++it) {
    ActionSet file_actions;
    if (!GetFileBrowserHandlers(profile, *it, &file_actions))
      return false;
    // If there is nothing to do for one file, the intersection of tasks for all
    // files will be empty at the end.
    if (!file_actions.size())
      return true;

    // For the very first file, just copy elements.
    if (it == files_list.begin()) {
      common_tasks = file_actions;
    } else {
      if (common_tasks.size()) {
        // For all additional files, find intersection between the accumulated
        // and file specific set.
        ActionSet intersection;
        std::set_intersection(common_tasks.begin(), common_tasks.end(),
                              file_actions.begin(), file_actions.end(),
                              std::inserter(intersection,
                                            intersection.begin()));
        common_tasks = intersection;
      }
    }
  }

  const DictionaryValue* prefs_tasks =
      profile->GetPrefs()->GetDictionary(prefs::kLastUsedFileBrowserHandlers);
  for (ActionSet::const_iterator iter = common_tasks.begin();
       iter != common_tasks.end(); ++iter) {
    // Get timestamp of when this task was used last time.
    int last_used_timestamp = 0;
    prefs_tasks->GetInteger(MakeTaskID((*iter)->extension_id().data(),
                                       (*iter)->id().data()),
                            &last_used_timestamp);
    URLPatternSet matching_patterns = GetAllMatchingPatterns(*iter, files_list);
    named_action_list->push_back(LastUsedHandler(last_used_timestamp, *iter,
                                                 matching_patterns));
  }

  SortLastUsedHandlerList(named_action_list);
  return true;
}

ListValue* URLPatternSetToStringList(const URLPatternSet& patterns) {
  ListValue* list = new ListValue();
  for (URLPatternSet::const_iterator it = patterns.begin();
       it != patterns.end(); ++it) {
    list->Append(new StringValue(it->GetAsString()));
  }

  return list;
}

// Update file handler usage stats.
void UpdateFileHandlerUsageStats(Profile* profile, const std::string& task_id) {
  if (!profile || !profile->GetPrefs())
    return;
  DictionaryPrefUpdate prefs_usage_update(profile->GetPrefs(),
      prefs::kLastUsedFileBrowserHandlers);
  prefs_usage_update->SetWithoutPathExpansion(task_id,
      new base::FundamentalValue(
          static_cast<int>(base::Time::Now().ToInternalValue()/
                           base::Time::kMicrosecondsPerSecond)));
}

#if defined(OS_CHROMEOS)
const DiskMountManager::Disk* GetVolumeAsDisk(const std::string& mount_path) {
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();

  DiskMountManager::MountPointMap::const_iterator mount_point_it =
      disk_mount_manager->mount_points().find(mount_path);
  if (mount_point_it == disk_mount_manager->mount_points().end())
    return NULL;

  DiskMountManager::DiskMap::const_iterator disk_it =
      disk_mount_manager->disks().find(mount_point_it->second.source_path);

  if (disk_it == disk_mount_manager->disks().end() ||
      disk_it->second->is_hidden()) {
    return NULL;
  }

  return disk_it->second;
}

base::DictionaryValue* CreateValueFromDisk(
    Profile* profile,
    const DiskMountManager::Disk* volume) {
  base::DictionaryValue* volume_info = new base::DictionaryValue();

  std::string mount_path;
  if (!volume->mount_path().empty()) {
    FilePath relative_mount_path;
    file_manager_util::ConvertFileToRelativeFileSystemPath(profile,
        FilePath(volume->mount_path()), &relative_mount_path);
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
    const GURL& extension_source_url) {

  base::DictionaryValue *mount_info = new base::DictionaryValue();

  mount_info->SetString("mountType",
                        DiskMountManager::MountTypeToString(
                            mount_point_info.mount_type));

  if (mount_point_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
    GURL source_url;
    if (file_manager_util::ConvertFileToFileSystemUrl(profile,
            FilePath(mount_point_info.source_path),
            extension_source_url.GetOrigin(),
            &source_url)) {
      mount_info->SetString("sourceUrl", source_url.spec());
     }
  } else {
    mount_info->SetString("sourceUrl", mount_point_info.source_path);
  }

  FilePath relative_mount_path;
  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  if (file_manager_util::ConvertFileToRelativeFileSystemPath(profile,
          FilePath(mount_point_info.mount_path),
          &relative_mount_path)) {
    mount_info->SetString("mountPath", relative_mount_path.value());
  }

  mount_info->SetString("mountCondition",
      DiskMountManager::MountConditionToString(
          mount_point_info.mount_condition));

  return mount_info;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

class RequestLocalFileSystemFunction::LocalFileSystemCallbackDispatcher {
 public:
  static fileapi::FileSystemContext::OpenFileSystemCallback CreateCallback(
      RequestLocalFileSystemFunction* function,
      Profile* profile,
      int child_id,
      scoped_refptr<const Extension> extension) {
    return base::Bind(
        &LocalFileSystemCallbackDispatcher::DidOpenFileSystem,
        base::Owned(new LocalFileSystemCallbackDispatcher(
            function, profile, child_id, extension)));
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
      Profile* profile,
      int child_id,
      scoped_refptr<const Extension> extension)
      : function_(function),
        profile_(profile),
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
        BrowserContext::GetFileSystemContext(profile_)->external_provider();
    if (!provider)
      return false;

    // Grant full access to File API from this component extension.
    provider->GrantFullAccessToExtension(extension_->id());

    // Grant R/W file permissions to the renderer hosting component
    // extension for all paths exposed by our local file system provider.
    std::vector<FilePath> root_dirs = provider->GetRootDirectories();
    for (std::vector<FilePath>::iterator iter = root_dirs.begin();
         iter != root_dirs.end();
         ++iter) {
      ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
          child_id_, *iter, kReadWriteFilePermissions);
    }
    return true;
  }

  RequestLocalFileSystemFunction* function_;
  Profile* profile_;
  // Renderer process id.
  int child_id_;
  // Extension source URL.
  scoped_refptr<const Extension> extension_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemCallbackDispatcher);
};

void RequestLocalFileSystemFunction::RequestOnFileThread(
    const GURL& source_url, int child_id) {
  GURL origin_url = source_url.GetOrigin();
  BrowserContext::GetFileSystemContext(profile())->OpenFileSystem(
      origin_url, fileapi::kFileSystemTypeExternal, false, // create
      LocalFileSystemCallbackDispatcher::CreateCallback(
          this,
          profile(),
          child_id,
          GetExtension()));
}

bool RequestLocalFileSystemFunction::RunImpl() {
  if (!dispatcher() || !render_view_host() || !render_view_host()->process())
    return false;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &RequestLocalFileSystemFunction::RequestOnFileThread,
          this,
          source_url_,
          render_view_host()->process()->GetID()));
  // Will finish asynchronously.
  return true;
}

void RequestLocalFileSystemFunction::RespondSuccessOnUIThread(
    const std::string& name, const GURL& root_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetString("name", name);
  dict->SetString("path", root_path.spec());
  dict->SetInteger("error", base::PLATFORM_FILE_OK);
  SendResponse(true);
}

void RequestLocalFileSystemFunction::RespondFailedOnUIThread(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  error_ = base::StringPrintf(kFileError, static_cast<int>(error_code));
  SendResponse(false);
}

bool FileWatchBrowserFunctionBase::GetLocalFilePath(
    const GURL& file_url, FilePath* local_path, FilePath* virtual_path) {
  GURL file_origin_url;
  fileapi::FileSystemType type;
  if (!CrackFileSystemURL(file_url, &file_origin_url, &type,
                          virtual_path)) {
    return false;
  }
  if (type != fileapi::kFileSystemTypeExternal)
    return false;

  FilePath root_path = BrowserContext::GetFileSystemContext(profile_)->
      external_provider()->GetFileSystemRootPathOnFileThread(
          file_origin_url,
          fileapi::kFileSystemTypeExternal,
          *virtual_path,
          false);
  if (root_path == FilePath())
    return false;

  *local_path = root_path.Append(*virtual_path);
  return true;
}

void FileWatchBrowserFunctionBase::RespondOnUIThread(bool success) {
  result_.reset(Value::CreateBooleanValue(success));
  SendResponse(success);
}

bool FileWatchBrowserFunctionBase::RunImpl() {
  if (!render_view_host() || !render_view_host()->process())
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
          file_watch_url,
          extension_id()));

  return true;
}

void FileWatchBrowserFunctionBase::RunFileWatchOperationOnFileThread(
    const GURL& file_url, const std::string& extension_id) {
  FilePath local_path;
  FilePath virtual_path;
  if (!GetLocalFilePath(file_url, &local_path, &virtual_path) ||
      local_path == FilePath()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &FileWatchBrowserFunctionBase::RespondOnUIThread,
            this,
            false));
  }
  if (!PerformFileWatchOperation(local_path, virtual_path, extension_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &FileWatchBrowserFunctionBase::RespondOnUIThread,
            this,
            false));
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &FileWatchBrowserFunctionBase::RespondOnUIThread,
          this,
          true));
}

bool AddFileWatchBrowserFunction::PerformFileWatchOperation(
    const FilePath& local_path, const FilePath& virtual_path,
    const std::string& extension_id) {
#if defined(OS_CHROMEOS)
  return profile_->GetExtensionService()->file_browser_event_router()->
      AddFileWatch(local_path, virtual_path, extension_id);
#else
  return true;
#endif  // defined(OS_CHROMEOS)
}

bool RemoveFileWatchBrowserFunction::PerformFileWatchOperation(
    const FilePath& local_path, const FilePath& unused,
    const std::string& extension_id) {
#if defined(OS_CHROMEOS)
  profile_->GetExtensionService()->file_browser_event_router()->
      RemoveFileWatch(local_path, extension_id);
#endif
  return true;
}

bool GetDefaultFileBrowserHandler(
    Profile* profile, const GURL& url, const FileBrowserHandler** handler) {
  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  LastUsedHandlerList common_tasks;
  if (!FindCommonTasks(profile, file_urls, &common_tasks))
    return false;

  if (common_tasks.size() == 0)
    return false;

  *handler = common_tasks[0].handler;
  return true;
}

bool GetFileTasksFileBrowserFunction::RunImpl() {
  ListValue* files_list = NULL;
  if (!args_->GetList(0, &files_list))
    return false;

  std::vector<GURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); ++i) {
    std::string file_url;
    if (!files_list->GetString(i, &file_url))
      return false;

    // We need case-insensitive matching, and pattern in handler is already
    // in lower case.
    StringToLowerASCII(&file_url);
    file_urls.push_back(GURL(file_url));
  }

  ListValue* result_list = new ListValue();
  result_.reset(result_list);

  LastUsedHandlerList common_tasks;
  if (!FindCommonTasks(profile_, file_urls, &common_tasks))
    return false;

  ExtensionService* service = profile_->GetExtensionService();
  for (LastUsedHandlerList::const_iterator iter = common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const FileBrowserHandler* handler = iter->handler;
    const std::string extension_id = handler->extension_id();
    const Extension* extension = service->GetExtensionById(extension_id, false);
    CHECK(extension);
    DictionaryValue* task = new DictionaryValue();
    task->SetString("taskId", MakeTaskID(extension_id.data(),
                                         handler->id().data()));
    task->SetString("title", handler->title());
    task->Set("patterns", URLPatternSetToStringList(iter->patterns));
    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    GURL icon =
        ExtensionIconSource::GetIconURL(extension,
                                        ExtensionIconSet::EXTENSION_ICON_BITTY,
                                        ExtensionIconSet::MATCH_BIGGER,
                                        false, NULL);     // grayscale
    task->SetString("iconUrl", icon.spec());
    result_list->Append(task);
  }

  // TODO(zelidrag, serya): Add intent content tasks to result_list once we
  // implement that API.
  SendResponse(true);
  return true;
}

class
  ExecuteTasksFileBrowserFunction::ExecuteTasksFileSystemCallbackDispatcher {
 public:
  static fileapi::FileSystemContext::OpenFileSystemCallback CreateCallback(
      ExecuteTasksFileBrowserFunction* function,
      Profile* profile,
      const GURL& source_url,
      const std::string task_id,
      scoped_refptr<const Extension> handler_extension,
      int handler_pid,
      const std::vector<GURL>& file_urls) {
    return base::Bind(
        &ExecuteTasksFileSystemCallbackDispatcher::DidOpenFileSystem,
        base::Owned(new ExecuteTasksFileSystemCallbackDispatcher(
            function, profile, source_url, task_id, handler_extension,
            handler_pid, file_urls)));
  }

  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& file_system_name,
                         const GURL& file_system_root) {
    if (result != base::PLATFORM_FILE_OK) {
      DidFail(result);
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    ExecuteTasksFileBrowserFunction::FileDefinitionList file_list;
    for (std::vector<GURL>::iterator iter = origin_file_urls_.begin();
         iter != origin_file_urls_.end();
         ++iter) {
      // Set up file permission access.
      ExecuteTasksFileBrowserFunction::FileDefinition file;
      if (!SetupFileAccessPermissions(*iter, &file.target_file_url,
                                      &file.virtual_path, &file.is_directory)) {
        continue;
      }
      file_list.push_back(file);
    }
    if (file_list.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(
              &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread,
              function_));
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ExecuteTasksFileBrowserFunction::ExecuteFileActionsOnUIThread,
            function_,
            task_id_,
            file_system_name,
            file_system_root,
            file_list));
  }

  void DidFail(base::PlatformFileError error_code) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread,
            function_));
  }

 private:
  ExecuteTasksFileSystemCallbackDispatcher(
      ExecuteTasksFileBrowserFunction* function,
      Profile* profile,
      const GURL& source_url,
      const std::string& task_id,
      const scoped_refptr<const Extension>& handler_extension,
      int handler_pid,
      const std::vector<GURL>& file_urls)
      : function_(function),
        profile_(profile),
        source_url_(source_url),
        task_id_(task_id),
        handler_extension_(handler_extension),
        handler_pid_(handler_pid),
        origin_file_urls_(file_urls) {
    DCHECK(function_);
  }

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  bool SetupFileAccessPermissions(const GURL& origin_file_url,
      GURL* target_file_url, FilePath* file_path, bool* is_directory) {
    if (!handler_extension_.get())
      return false;

    if (handler_pid_ == 0)
      return false;

    GURL file_origin_url;
    FilePath virtual_path;
    fileapi::FileSystemType type;
    if (!CrackFileSystemURL(origin_file_url, &file_origin_url, &type,
                            &virtual_path)) {
      return false;
    }

    if (type != fileapi::kFileSystemTypeExternal)
      return false;

    fileapi::ExternalFileSystemMountPointProvider* external_provider =
        BrowserContext::GetFileSystemContext(profile_)->external_provider();
    if (!external_provider)
      return false;

    if (!external_provider->IsAccessAllowed(file_origin_url,
                                            type,
                                            virtual_path)) {
      return false;
    }

    // Make sure this url really being used by the right caller extension.
    if (source_url_.GetOrigin() != file_origin_url) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return false;
    }

    FilePath root_path =
        external_provider->GetFileSystemRootPathOnFileThread(
          file_origin_url,
          fileapi::kFileSystemTypeExternal,
          virtual_path,
          false);     // create
    FilePath final_file_path = root_path.Append(virtual_path);

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;

    if (!file_util::PathExists(final_file_path) ||
        file_util::IsLink(final_file_path) ||
        !file_util::GetFileInfo(final_file_path, &file_info))
      return false;

    // TODO(zelidrag): Let's just prevent all symlinks for now. We don't want a
    // USB drive content to point to something in the rest of the file system.
    // Ideally, we should permit symlinks within the boundary of the same
    // virtual mount point.
    if (file_info.is_symbolic_link)
      return false;

    // TODO(tbarzic): Add explicit R/W + R/O permissions for non-component
    // extensions.

    // Grant R/O access permission to non-component extension and R/W to
    // component extensions.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        handler_pid_,
        final_file_path,
        handler_extension_->location() != Extension::COMPONENT ?
            kReadOnlyFilePermissions : kReadWriteFilePermissions);

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    external_provider->GrantFileAccessToExtension(handler_extension_->id(),
                                                  virtual_path);

    // Output values.
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        handler_extension_->id()));
    GURL base_url = fileapi::GetFileSystemRootURI(target_origin_url,
        fileapi::kFileSystemTypeExternal);
    *target_file_url = GURL(base_url.spec() + virtual_path.value());
    FilePath root(FILE_PATH_LITERAL("/"));
    *file_path = root.Append(virtual_path);
    *is_directory = file_info.is_directory;
    return true;
  }

  ExecuteTasksFileBrowserFunction* function_;
  Profile* profile_;
  // Extension source URL.
  GURL source_url_;
  std::string task_id_;
  scoped_refptr<const Extension> handler_extension_;
  int handler_pid_;
  std::vector<GURL> origin_file_urls_;
  DISALLOW_COPY_AND_ASSIGN(ExecuteTasksFileSystemCallbackDispatcher);
};

bool ExecuteTasksFileBrowserFunction::RunImpl() {
  // First param is task id that was to the extension with getFileTasks call.
  std::string task_id;
  if (!args_->GetString(0, &task_id) || !task_id.size())
    return false;

  // The second param is the list of files that need to be executed with this
  // task.
  ListValue* files_list = NULL;
  if (!args_->GetList(1, &files_list))
    return false;

  if (!files_list->GetSize())
    return true;

  return InitiateFileTaskExecution(task_id, files_list);
}

bool ExecuteTasksFileBrowserFunction::InitiateFileTaskExecution(
    const std::string& task_id, ListValue* files_list) {
  std::vector<GURL> file_urls;
  for (size_t i = 0; i < files_list->GetSize(); i++) {
    std::string origin_file_url;
    if (!files_list->GetString(i, &origin_file_url)) {
      error_ = kInvalidFileUrl;
      return false;
    }
    file_urls.push_back(GURL(origin_file_url));
  }

  scoped_refptr<const Extension> handler =
      ExtractExtensionFromTaskId(task_id, profile());
  if (!handler.get())
    return false;

  int handler_pid = ExtractProcessFromExtensionId(handler->id(), profile());
  if (handler_pid < 0)
    return false;

  // Get local file system instance on file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread,
          this,
          task_id,
          Extension::GetBaseURLFromExtensionId(handler->id()),
          handler,
          handler_pid,
          file_urls));
  result_.reset(new base::FundamentalValue(true));
  return true;
}

void ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread(
    const std::string& task_id,
    const GURL& handler_base_url,
    const scoped_refptr<const Extension>& handler,
    int handler_pid,
    const std::vector<GURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  GURL origin_url = handler_base_url.GetOrigin();
  BrowserContext::GetFileSystemContext(profile())->OpenFileSystem(
      origin_url, fileapi::kFileSystemTypeExternal, false, // create
      ExecuteTasksFileSystemCallbackDispatcher::CreateCallback(
          this,
          profile(),
          source_url_,
          task_id,
          handler,
          handler_pid,
          file_urls));
}

void ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread() {
  SendResponse(false);
}


void ExecuteTasksFileBrowserFunction::ExecuteFileActionsOnUIThread(
    const std::string& task_id,
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;
  // Get task details.
  std::string handler_extension_id;
  std::string action_id;
  if (!CrackTaskIdentifier(task_id, &handler_extension_id,
                           &action_id)) {
    LOG(WARNING) << "Invalid task " << task_id;
    SendResponse(false);
    return;
  }

  const Extension* extension = service->GetExtensionById(handler_extension_id,
                                                         false);
  if (!extension) {
    SendResponse(false);
    return;
  }

  ExtensionEventRouter* event_router = profile_->GetExtensionEventRouter();
  if (!event_router) {
    SendResponse(false);
    return;
  }

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(Value::CreateStringValue(action_id));
  DictionaryValue* details = new DictionaryValue();
  event_args->Append(details);
  // Get file definitions. These will be replaced with Entry instances by
  // chromeHidden.Event.dispatchJSON() method from even_binding.js.
  ListValue* files_urls = new ListValue();
  details->Set("entries", files_urls);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    DictionaryValue* file_def = new DictionaryValue();
    files_urls->Append(file_def);
    file_def->SetString("fileSystemName", file_system_name);
    file_def->SetString("fileSystemRoot", file_system_root.spec());
    file_def->SetString("fileFullPath", iter->virtual_path.value());
    file_def->SetBoolean("fileIsDirectory", iter->is_directory);
  }
  // Get tab id.
  Browser* browser = GetCurrentBrowser();
  if (browser) {
    WebContents* contents = browser->GetSelectedWebContents();
    if (contents)
      details->SetInteger("tab_id", ExtensionTabUtil::GetTabId(contents));
  }

  UpdateFileHandlerUsageStats(profile_, task_id);

  std::string json_args;
  base::JSONWriter::Write(event_args.get(), false, &json_args);
  event_router->DispatchEventToExtension(
      handler_extension_id, std::string("fileBrowserHandler.onExecute"),
      json_args, profile_,
      GURL());
  SendResponse(true);
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
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &FileBrowserFunction::GetLocalPathsOnFileThread,
          this,
          file_urls, callback));
}

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls |callback| with the result vector, on the UI
// thread.
void FileBrowserFunction::GetLocalPathsOnFileThread(
    const UrlList& file_urls,
    GetLocalPathsCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePathList selected_files;

  // FilePath(virtual_path) doesn't work on win, so limit this to ChromeOS.
#if defined(OS_CHROMEOS)
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  if (!provider) {
    LOG(WARNING) << "External provider is not available";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, selected_files));
    return;
  }

  GURL origin_url = source_url().GetOrigin();
  size_t len = file_urls.size();
  selected_files.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    const GURL& file_url = file_urls[i];
    GURL file_origin_url;
    FilePath virtual_path;
    fileapi::FileSystemType type;
    if (!CrackFileSystemURL(file_url, &file_origin_url, &type,
                            &virtual_path)) {
      continue;
    }
    if (type != fileapi::kFileSystemTypeExternal) {
      NOTREACHED();
      continue;
    }
    FilePath root = provider->GetFileSystemRootPathOnFileThread(
        origin_url,
        fileapi::kFileSystemTypeExternal,
        FilePath(virtual_path),
        false);
    if (!root.empty()) {
      selected_files.push_back(root.Append(virtual_path));
    } else {
      LOG(WARNING) << "GetLocalPathsOnFileThread failed "
                   << file_url.spec();
    }
  }
#endif

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
    const FilePathList& files) {
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
      base::Bind(&ViewFilesFunction::GetLocalPathsResponseOnUIThread,
                 this,
                 internal_task_id));
  return true;
}

void ViewFilesFunction::GetLocalPathsResponseOnUIThread(
    const std::string& internal_task_id,
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool success = true;
  for (FilePathList::const_iterator iter = files.begin();
       iter != files.end();
       ++iter) {
    bool handled = file_manager_util::TryViewingFile(*iter);
    // If there is no default browser-defined handler for viewing this type
    // of file, try to see if we have any extension installed for it instead.
    if (!handled && files.size() == 1)
      success = false;
  }
  result_.reset(Value::CreateBooleanValue(success));
  SendResponse(true);
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
    const FilePathList& files) {
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

#if defined(OS_CHROMEOS)
  chromeos::MountType mount_type =
      DiskMountManager::MountTypeFromString(mount_type_str);
  switch (mount_type) {
    case chromeos::MOUNT_TYPE_INVALID: {
      error_ = "Invalid mount type";
      SendResponse(false);
      break;
    }
    case chromeos::MOUNT_TYPE_GDATA: {
      gdata::GDataFileSystem* file_system =
          gdata::GDataFileSystemFactory::GetForProfile(profile_);
      file_system->Authenticate(
          base::Bind(&AddMountFunction::OnGDataAuthentication,
                     this));
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
#endif  // defined(OS_CHROMEOS)

  return true;
}


void AddMountFunction::AddGDataMountPoint() {
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  const FilePath mount_point = gdata::util::GetGDataMountPointPath();
  if (!provider || provider->HasMountPoint(mount_point))
    return;

  provider->AddRemoteMountPoint(mount_point,
                                new gdata::GDataFileSystemProxy(profile_));
}

void AddMountFunction::RaiseGDataMountEvent(gdata::GDataErrorCode error,
                                            const std::string auth_token) {
  chromeos::MountError error_code = error == gdata::HTTP_SUCCESS ?
      chromeos::MOUNT_ERROR_NONE : chromeos::MOUNT_ERROR_NOT_AUTHENTICATED;
  DiskMountManager::MountPointInfo mount_info(
      gdata::util::GetGDataMountPointPathAsString(),
      gdata::util::GetGDataMountPointPathAsString(),
      auth_token,
      chromeos::MOUNT_TYPE_GDATA,
      chromeos::disks::MOUNT_CONDITION_NONE);
  // Raise mount event
  profile_->GetExtensionService()->file_browser_event_router()->MountCompleted(
      DiskMountManager::MOUNTING,
      error_code,
      mount_info);
}

void AddMountFunction::OnGDataAuthentication(gdata::GDataErrorCode error,
                                             const std::string& token) {
  if (error == gdata::HTTP_SUCCESS)
    AddGDataMountPoint();

  RaiseGDataMountEvent(error, token);
  SendResponse(true);
}

void AddMountFunction::GetLocalPathsResponseOnUIThread(
    const std::string& mount_type_str,
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!files.size()) {
    SendResponse(false);
    return;
  }

#if defined(OS_CHROMEOS)
  FilePath::StringType source_file = files[0].value();
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  disk_mount_manager->MountPath(source_file.data(),
      DiskMountManager::MountTypeFromString(mount_type_str));
#endif  // defined(OS_CHROMEOS)

  SendResponse(true);
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
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
#if defined(OS_CHROMEOS)
  DiskMountManager::GetInstance()->UnmountPath(files[0].value());
#endif

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
  result_.reset(mounts);

#if defined(OS_CHROMEOS)
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  DiskMountManager::MountPointMap mount_points =
      disk_mount_manager->mount_points();

  for (DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end();
       ++it) {
    mounts->Append(CreateValueFromMountPoint(profile_, it->second,
                   source_url_));
  }
#endif  // defined(OS_CHROMEOS)

  SendResponse(true);
  return true;
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

  UrlList mount_paths;
  mount_paths.push_back(GURL(mount_url));

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      mount_paths,
      base::Bind(&GetSizeStatsFunction::GetLocalPathsResponseOnUIThread, this));
  return true;
}

void GetSizeStatsFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &GetSizeStatsFunction::CallGetSizeStatsOnFileThread,
          this,
          files[0].value()));
}

void GetSizeStatsFunction::CallGetSizeStatsOnFileThread(
    const std::string& mount_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  size_t total_size_kb = 0;
  size_t remaining_size_kb = 0;
#if defined(OS_CHROMEOS)
  DiskMountManager::GetInstance()->
      GetSizeStatsOnFileThread(mount_path, &total_size_kb, &remaining_size_kb);
#endif

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread,
          this,
          mount_path, total_size_kb, remaining_size_kb));
}

void GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread(
    const std::string&  mount_path,
    size_t total_size_kb,
    size_t remaining_size_kb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* sizes = new base::DictionaryValue();
  result_.reset(sizes);

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

  UrlList file_paths;
  file_paths.push_back(GURL(volume_file_url));

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_paths,
      base::Bind(&FormatDeviceFunction::GetLocalPathsResponseOnUIThread, this));
  return true;
}

void FormatDeviceFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }

#if defined(OS_CHROMEOS)
  DiskMountManager::GetInstance()->FormatMountedDevice(files[0].value());
#endif

  SendResponse(true);
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
  }

  UrlList file_paths;
  file_paths.push_back(GURL(volume_mount_url));

  GetLocalPathsOnFileThreadAndRunCallbackOnUIThread(
      file_paths,
      base::Bind(&GetVolumeMetadataFunction::GetLocalPathsResponseOnUIThread,
                 this));

  return true;
}

void GetVolumeMetadataFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should contain volume's mount path.
  if (files.size() != 1) {
    error_ = "Invalid mount path.";
    SendResponse(false);
    return;
  }

  result_.reset();

#if defined(OS_CHROMEOS)
  const DiskMountManager::Disk* volume = GetVolumeAsDisk(files[0].value());
  if (volume) {
    DictionaryValue* volume_info =
        CreateValueFromDisk(profile_, volume);
    result_.reset(volume_info);
  }
#endif

  SendResponse(true);
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
  result_.reset(Value::CreateBooleanValue(
      browser && browser->window() && browser->window()->IsFullscreen()));
  return true;
}

bool FileDialogStringsFunction::RunImpl() {
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());

#define SET_STRING(ns, id) \
  dict->SetString(#id, l10n_util::GetStringUTF16(ns##_##id))

  SET_STRING(IDS, WEB_FONT_FAMILY);
  SET_STRING(IDS, WEB_FONT_SIZE);

  SET_STRING(IDS_FILE_BROWSER, ROOT_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, ARCHIVE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, REMOVABLE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, GDATA_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, NAME_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_B);
  SET_STRING(IDS_FILE_BROWSER, SIZE_KB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_MB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_GB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_TB);
  SET_STRING(IDS_FILE_BROWSER, SIZE_PB);
  SET_STRING(IDS_FILE_BROWSER, TYPE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DATE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, PREVIEW_COLUMN_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_WARNING);

  SET_STRING(IDS_FILE_BROWSER, ERROR_CREATING_FOLDER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_INVALID_CHARACTER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_RESERVED_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_HIDDEN_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_WHITESPACE_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_NEW_FOLDER_EMPTY_NAME);
  SET_STRING(IDS_FILE_BROWSER, NEW_FOLDER_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, FILENAME_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DIMENSIONS_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DIMENSIONS_FORMAT);

  SET_STRING(IDS_FILE_BROWSER, EJECT_BUTTON);
  SET_STRING(IDS_FILE_BROWSER, IMAGE_DIMENSIONS);
  SET_STRING(IDS_FILE_BROWSER, VOLUME_LABEL);
  SET_STRING(IDS_FILE_BROWSER, READ_ONLY);

  SET_STRING(IDS_FILE_BROWSER, ARCHIVE_MOUNT_FAILED);
  SET_STRING(IDS_FILE_BROWSER, MOUNT_ARCHIVE);
  SET_STRING(IDS_FILE_BROWSER, FORMAT_DEVICE);

  SET_STRING(IDS_FILE_BROWSER, GALLERY);
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
  // Reusing the string, but with alias starting with GALLERY.
  dict->SetString("GALLERY_FILE_HIDDEN_NAME",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_HIDDEN_NAME));

  SET_STRING(IDS_FILE_BROWSER, CONFIRM_OVERWRITE_FILE);
  SET_STRING(IDS_FILE_BROWSER, FILE_ALREADY_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, DIRECTORY_ALREADY_EXISTS);
  SET_STRING(IDS_FILE_BROWSER, ERROR_RENAMING);
  SET_STRING(IDS_FILE_BROWSER, RENAME_PROMPT);
  SET_STRING(IDS_FILE_BROWSER, RENAME_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_DELETING);
  SET_STRING(IDS_FILE_BROWSER, DELETE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_MOVING);
  SET_STRING(IDS_FILE_BROWSER, MOVE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, ERROR_PASTING);
  SET_STRING(IDS_FILE_BROWSER, PASTE_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, COPY_BUTTON_LABEL);
  SET_STRING(IDS_FILE_BROWSER, CUT_BUTTON_LABEL);

  SET_STRING(IDS_FILE_BROWSER, PASTE_ITEMS_REMAINING);
  SET_STRING(IDS_FILE_BROWSER, PASTE_CANCELLED);
  SET_STRING(IDS_FILE_BROWSER, PASTE_TARGET_EXISTS_ERROR);
  SET_STRING(IDS_FILE_BROWSER, PASTE_FILESYSTEM_ERROR);
  SET_STRING(IDS_FILE_BROWSER, PASTE_UNEXPECTED_ERROR);

  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_FLASH);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_HDD);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_OPTICAL);
  SET_STRING(IDS_FILE_BROWSER, DEVICE_TYPE_UNDEFINED);

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

  SET_STRING(IDS_FILE_BROWSER, SELECT_FOLDER_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_OPEN_MULTI_FILE_TITLE);
  SET_STRING(IDS_FILE_BROWSER, SELECT_SAVEAS_FILE_TITLE);

  SET_STRING(IDS_FILE_BROWSER, COMPUTING_SELECTION);
  SET_STRING(IDS_FILE_BROWSER, ONE_FILE_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, ONE_DIRECTORY_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_FILES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_DIRECTORIES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_ENTRIES_SELECTED);

  SET_STRING(IDS_FILE_BROWSER, PLAYBACK_ERROR);
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
  SET_STRING(IDS_FILE_BROWSER, GSLIDES_DOCUMENT_FILE_TYPE);
  SET_STRING(IDS_FILE_BROWSER, GTABLE_DOCUMENT_FILE_TYPE);

  SET_STRING(IDS_FILE_BROWSER, ENQUEUE);
#undef SET_STRING

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(dict);

  dict->SetString("PLAY_MEDIA",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAY));

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableGData))
      dict->SetString("ENABLE_GDATA", "1");

  return true;
}
