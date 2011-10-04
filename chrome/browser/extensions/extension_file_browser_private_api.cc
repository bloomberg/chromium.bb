// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_browser_private_api.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/file_manager_dialog.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "ui/base/l10n/l10n_util.h"

#ifdef OS_CHROMEOS
#include "chrome/browser/chromeos/cros/cros_library.h"
#endif

// Error messages.
const char kFileError[] = "File error %d";
const char kInvalidFileUrl[] = "Invalid file URL";
const char kVolumeDevicePathNotFound[] = "Device path not found";

#ifdef OS_CHROMEOS
// Volume type strings.
const std::string kVolumeTypeFlash = "flash";
const std::string kVolumeTypeOptical = "optical";
const std::string kVolumeTypeHardDrive = "hdd";
const std::string kVolumeTypeUnknown = "undefined";
#endif

// Internal task ids.
const char kEnqueueTaskId[] = "enqueue";

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

bool GetFileBrowserHandlers(Profile* profile,
                            const GURL& selected_file_url,
                            ActionSet* results) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return false;  // In unit-tests, we may not have an ExtensionService.

  for (ExtensionList::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();
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

#ifdef OS_CHROMEOS
base::DictionaryValue* MountPointToValue(Profile* profile,
    const chromeos::MountLibrary::MountPointInfo& mount_point_info) {

    base::DictionaryValue *mount_info = new base::DictionaryValue();

    mount_info->SetString("mountType",
        chromeos::MountLibrary::MountTypeToString(mount_point_info.mount_type));

    if (mount_point_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
      GURL source_url;
      if (FileManagerUtil::ConvertFileToFileSystemUrl(profile,
              FilePath(mount_point_info.source_path),
              FileManagerUtil::GetFileBrowserExtensionUrl().GetOrigin(),
              &source_url)) {
        mount_info->SetString("sourceUrl", source_url.spec());
      }
    } else {
      mount_info->SetString("sourceUrl", mount_point_info.source_path);
    }

    FilePath relative_mount_path;
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    if (FileManagerUtil::ConvertFileToRelativeFileSystemPath(profile,
            FilePath(mount_point_info.mount_path),
            &relative_mount_path)) {
      mount_info->SetString("mountPath", relative_mount_path.value());
    }

    mount_info->SetString("mountCondition",
        chromeos::MountLibrary::MountConditionToString(
        mount_point_info.mount_condition));

    return mount_info;
}
#endif


class LocalFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit LocalFileSystemCallbackDispatcher(
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

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // Set up file permission access.
    if (!SetupFileSystemAccessPermissions()) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondSuccessOnUIThread,
            name,
            root_path));
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondFailedOnUIThread,
            error_code));
  }

 private:

  // Grants file system access permissions to file browser component.
  bool SetupFileSystemAccessPermissions() {
    if (!extension_.get())
      return false;

    // Make sure that only component extension can access the entire
    // local file system.
    if (extension_->location() != Extension::COMPONENT
#ifndef NDEBUG
      && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposePrivateExtensionApi)
#endif
        ) {
      NOTREACHED() << "Private method access by non-component extension "
                   << extension_->id();
      return false;
    }

    fileapi::FileSystemPathManager* path_manager =
        profile_->GetFileSystemContext()->path_manager();
    fileapi::ExternalFileSystemMountPointProvider* provider =
        path_manager->external_provider();
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
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new LocalFileSystemCallbackDispatcher(
              this,
              profile(),
              child_id,
              GetExtension()),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url.GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeExternal,
                            false);     // create
}

bool RequestLocalFileSystemFunction::RunImpl() {
  if (!dispatcher() || !render_view_host() || !render_view_host()->process())
    return false;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &RequestLocalFileSystemFunction::RequestOnFileThread,
          source_url_,
          render_view_host()->process()->id()));
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
  fileapi::FileSystemPathManager* path_manager =
      profile_->GetFileSystemContext()->path_manager();
  GURL file_origin_url;
  fileapi::FileSystemType type;
  if (!CrackFileSystemURL(file_url, &file_origin_url, &type,
                          virtual_path)) {
    return false;
  }
  if (type != fileapi::kFileSystemTypeExternal)
    return false;

  FilePath root_path =
      path_manager->ValidateFileSystemRootAndGetPathOnFileThread(
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
      NewRunnableMethod(this,
          &FileWatchBrowserFunctionBase::RunFileWatchOperationOnFileThread,
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
        NewRunnableMethod(this,
            &FileWatchBrowserFunctionBase::RespondOnUIThread,
            false));
  }
  if (!PerformFileWatchOperation(local_path, virtual_path, extension_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &FileWatchBrowserFunctionBase::RespondOnUIThread,
            false));
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FileWatchBrowserFunctionBase::RespondOnUIThread,
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
#endif  // OS_CHROMEOS
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
                                        Extension::EXTENSION_ICON_BITTY,
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

class ExecuteTasksFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit ExecuteTasksFileSystemCallbackDispatcher(
      ExecuteTasksFileBrowserFunction* function,
      Profile* profile,
      int child_id,
      const GURL& source_url,
      scoped_refptr<const Extension> extension,
      const std::string task_id,
      const std::vector<GURL>& file_urls)
      : function_(function),
        profile_(profile),
        source_url_(source_url),
        extension_(extension),
        task_id_(task_id),
        origin_file_urls_(file_urls) {
    DCHECK(function_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& file_system_name,
                                 const GURL& file_system_root) OVERRIDE {
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
          NewRunnableMethod(function_,
              &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread));
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &ExecuteTasksFileBrowserFunction::ExecuteFileActionsOnUIThread,
            task_id_,
            file_system_name,
            file_system_root,
            file_list));
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &ExecuteTasksFileBrowserFunction::ExecuteFailedOnUIThread));
  }

 private:
  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  bool SetupFileAccessPermissions(const GURL& origin_file_url,
     GURL* target_file_url, FilePath* file_path, bool* is_directory) {
    if (!extension_.get())
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

    fileapi::FileSystemPathManager* path_manager =
        profile_->GetFileSystemContext()->path_manager();
    if (!path_manager->IsAccessAllowed(file_origin_url,
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
        path_manager->ValidateFileSystemRootAndGetPathOnFileThread(
          file_origin_url,
          fileapi::kFileSystemTypeExternal,
          virtual_path,
          false);     // create
    FilePath final_file_path = root_path.Append(virtual_path);

    fileapi::ExternalFileSystemMountPointProvider* external_provider =
        path_manager->external_provider();
    if (!external_provider)
      return false;

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

    // Get task details.
    std::string target_extension_id;
    std::string action_id;
    if (!CrackTaskIdentifier(task_id_, &target_extension_id,
                             &action_id)) {
      return false;
    }

    // TODO(zelidrag): Add explicit R/W + R/O permissions for non-component
    // extensions.

    // Get target extension's process.
    RenderProcessHost* target_host =
        profile_->GetExtensionProcessManager()->GetExtensionProcess(
            target_extension_id);
    if (!target_host)
      return false;

    // Grant R/O access permission to non-component extension and R/W to
    // component extensions.
    ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        target_host->id(), final_file_path,
        extension_->location() != Extension::COMPONENT ?
            kReadOnlyFilePermissions : kReadWriteFilePermissions);

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    external_provider->GrantFileAccessToExtension(target_extension_id,
                                                  virtual_path);

    // Output values.
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        target_extension_id));
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
  scoped_refptr<const Extension> extension_;
  std::string task_id_;
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
  // Get local file system instance on file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread,
          source_url_,
          task_id,
          file_urls));
  result_.reset(new base::FundamentalValue(true));
  return true;
}

void ExecuteTasksFileBrowserFunction::RequestFileEntryOnFileThread(
    const GURL& source_url, const std::string& task_id,
    const std::vector<GURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new ExecuteTasksFileSystemCallbackDispatcher(
              this,
              profile(),
              render_view_host()->process()->id(),
              source_url,
              GetExtension(),
              task_id,
              file_urls),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url.GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeExternal,
                            false);     // create
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
    TabContents* contents = browser->GetSelectedTabContents();
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
  TabContents* tab_contents =
      dispatcher()->delegate()->GetAssociatedTabContents();
  if (!tab_contents) {
    LOG(WARNING) << "No associated tab contents";
    return 0;
  }
  return ExtensionTabUtil::GetTabId(tab_contents);
}

// GetFileSystemRootPathOnFileThread can only be called from the file thread,
// so here we are. This function takes a vector of virtual paths, converts
// them to local paths and calls GetLocalPathsResponseOnUIThread with the
// result vector, on the UI thread.
void FileBrowserFunction::GetLocalPathsOnFileThread(const UrlList& file_urls,
                                                    void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePathList selected_files;

  // FilePath(virtual_path) doesn't work on win, so limit this to ChromeOS.
#if defined(OS_CHROMEOS)
  GURL origin_url = source_url().GetOrigin();
  fileapi::FileSystemPathManager* path_manager =
      profile()->GetFileSystemContext()->path_manager();

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
    FilePath root = path_manager->ValidateFileSystemRootAndGetPathOnFileThread(
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

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FileBrowserFunction::GetLocalPathsResponseOnUIThread,
          selected_files, context));
}

bool SelectFileFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }
  std::string file_url;
  args_->GetString(0, &file_url);
  UrlList file_paths;
  file_paths.push_back(GURL(file_url));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFileFunction::GetLocalPathsOnFileThread,
          file_paths, reinterpret_cast<void*>(NULL)));

  return true;
}

void SelectFileFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!context);
  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  int index;
  args_->GetInteger(1, &index);
  int32 tab_id = GetTabId();
  FileManagerDialog::OnFileSelected(tab_id, files[0], index);
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

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &ViewFilesFunction::GetLocalPathsOnFileThread,
          file_urls,
          reinterpret_cast<void*>(new std::string(internal_task_id))));

  return true;
}

void ViewFilesFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(context);
  scoped_ptr<std::string> internal_task_id(
      reinterpret_cast<std::string*>(context));
  for (FilePathList::const_iterator iter = files.begin();
       iter != files.end();
       ++iter) {
    FileManagerUtil::ViewItem(*iter,
                              *(internal_task_id.get()) == kEnqueueTaskId ||
                                  // Start the first one, enqueue others.
                                  iter != files.begin());
  }
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

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SelectFilesFunction::GetLocalPathsOnFileThread,
          file_urls, reinterpret_cast<void*>(NULL)));

  return true;
}

void SelectFilesFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!context);
  int32 tab_id = GetTabId();
  FileManagerDialog::OnMultiFilesSelected(tab_id, files);
  SendResponse(true);
}

bool CancelFileDialogFunction::RunImpl() {
  int32 tab_id = GetTabId();
  FileManagerDialog::OnFileSelectionCanceled(tab_id);
  SendResponse(true);
  return true;
}

AddMountFunction::AddMountFunction() {
}

AddMountFunction::~AddMountFunction() {
}

bool AddMountFunction::RunImpl() {
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

  UrlList file_paths;
  file_paths.push_back(GURL(file_url));

#if defined(OS_CHROMEOS)
  chromeos::MountPathOptions options;
  if (args_->GetSize() == 3) {
    DictionaryValue *dict;
    if (!args_->GetDictionary(2, &dict)) {
      NOTREACHED();
    }

    for (base::DictionaryValue::key_iterator it = dict->begin_keys();
         it != dict->end_keys();
         ++it) {
      std::string value;
      if (!dict->GetString(*it, &value)) {
        NOTREACHED();
      }

      options.push_back(chromeos::MountPathOptions::value_type((*it).c_str(),
                                                               value.c_str()));
    }
  }

  MountParamaters* params = new MountParamaters(mount_type_str, options);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &AddMountFunction::GetLocalPathsOnFileThread,
          file_paths, reinterpret_cast<void*>(params)));
#endif  // OS_CHROMEOS

  return true;
}

void AddMountFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(context);

  if (!files.size()) {
    SendResponse(false);
    return;
  }

#ifdef OS_CHROMEOS
  scoped_ptr<MountParamaters> params(
      reinterpret_cast<MountParamaters*>(context));
  const std::string& mount_type_str = params->mount_type;
  const chromeos::MountPathOptions& options = params->mount_options;
  FilePath::StringType source_file = files[0].value();

  chromeos::MountLibrary *mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();

  chromeos::MountType mount_type =
      mount_lib->MountTypeFromString(mount_type_str);
  if (mount_type == chromeos::MOUNT_TYPE_INVALID) {
    error_ = "Invalid mount type";
    SendResponse(false);
    return;
  }

  mount_lib->MountPath(source_file.data(), mount_type, options);
#endif

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
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &RemoveMountFunction::GetLocalPathsOnFileThread,
          file_paths, reinterpret_cast<void*>(NULL)));
  return true;
}

void RemoveMountFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!context);

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
#ifdef OS_CHROMEOS
  chromeos::CrosLibrary::Get()->GetMountLibrary()->UnmountPath(
      files[0].value().c_str());
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

#ifdef OS_CHROMEOS
  chromeos::MountLibrary *mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  chromeos::MountLibrary::MountPointMap mount_points =
      mount_lib->mount_points();

  for (chromeos::MountLibrary::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end();
       ++it) {
    mounts->Append(MountPointToValue(profile_, it->second));
  }
#endif

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

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &GetSizeStatsFunction::GetLocalPathsOnFileThread,
          mount_paths, reinterpret_cast<void*>(NULL)));
  return true;
}

void GetSizeStatsFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &GetSizeStatsFunction::CallGetSizeStatsOnFileThread,
          files[0].value().c_str()));
}

void GetSizeStatsFunction::CallGetSizeStatsOnFileThread(
    const char* mount_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  size_t total_size_kb = 0;
  size_t remaining_size_kb = 0;
#ifdef OS_CHROMEOS
  chromeos::CrosLibrary::Get()->GetMountLibrary()->GetSizeStatsOnFileThread(
      mount_path, &total_size_kb, &remaining_size_kb);
#endif

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread,
          mount_path, total_size_kb, remaining_size_kb));
}

void GetSizeStatsFunction::GetSizeStatsCallbackOnUIThread(
    const char* mount_path, size_t total_size_kb, size_t remaining_size_kb) {
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

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &FormatDeviceFunction::GetLocalPathsOnFileThread,
          file_paths, reinterpret_cast<void*>(NULL)));
  return true;
}

void FormatDeviceFunction::GetLocalPathsResponseOnUIThread(
    const FilePathList& files, void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }

#ifdef OS_CHROMEOS
  chromeos::CrosLibrary::Get()->GetMountLibrary()->FormatMountedDevice(
      files[0].value().c_str());
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

  std::string volume_device_path;
  if (!args_->GetString(0, &volume_device_path)) {
    NOTREACHED();
  }

#ifdef OS_CHROMEOS
  chromeos::MountLibrary* mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  chromeos::MountLibrary::DiskMap::const_iterator volume_it =
      mount_lib->disks().find(volume_device_path);

  if (volume_it != mount_lib->disks().end() &&
      !volume_it->second->is_hidden()) {
    chromeos::MountLibrary::Disk* volume = volume_it->second;
    DictionaryValue* volume_info = new DictionaryValue();
    result_.reset(volume_info);
    // Localising mount path.
    std::string mount_path;
    if (!volume->mount_path().empty()) {
      FilePath relative_mount_path;
      FileManagerUtil::ConvertFileToRelativeFileSystemPath(profile_,
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
        DeviceTypeToString(volume->device_type()));
    volume_info->SetInteger("totalSize", volume->total_size());
    volume_info->SetBoolean("isParent", volume->is_parent());
    volume_info->SetBoolean("isReadOnly", volume->is_read_only());
    volume_info->SetBoolean("hasMedia", volume->has_media());
    volume_info->SetBoolean("isOnBootDevice", volume->on_boot_device());

    return true;
  }
#endif
  error_ = kVolumeDevicePathNotFound;
  return false;
}

#ifdef OS_CHROMEOS
const std::string& GetVolumeMetadataFunction::DeviceTypeToString(
    chromeos::DeviceType type) {
  switch (type) {
    case chromeos::FLASH:
      return kVolumeTypeFlash;
    case chromeos::OPTICAL:
      return kVolumeTypeOptical;
    case chromeos::HDD:
      return kVolumeTypeHardDrive;
    default:
      break;
  }
  return kVolumeTypeUnknown;
}
#endif

bool FileDialogStringsFunction::RunImpl() {
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());

#define SET_STRING(ns, id) \
  dict->SetString(#id, l10n_util::GetStringUTF16(ns##_##id))

  SET_STRING(IDS, WEB_FONT_FAMILY);
  SET_STRING(IDS, WEB_FONT_SIZE);

  SET_STRING(IDS_FILE_BROWSER, ROOT_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, ARCHIVE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, REMOVABLE_DIRECTORY_LABEL);
  SET_STRING(IDS_FILE_BROWSER, NAME_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, SIZE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, TYPE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, DATE_COLUMN_LABEL);
  SET_STRING(IDS_FILE_BROWSER, PREVIEW_COLUMN_LABEL);

  SET_STRING(IDS_FILE_BROWSER, DOWNLOADS_DIRECTORY_WARNING);

  SET_STRING(IDS_FILE_BROWSER, ERROR_CREATING_FOLDER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_INVALID_CHARACTER);
  SET_STRING(IDS_FILE_BROWSER, ERROR_RESERVED_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_HIDDEN_NAME);
  SET_STRING(IDS_FILE_BROWSER, ERROR_WHITESPACE_NAME);
  SET_STRING(IDS_FILE_BROWSER, NEW_FOLDER_PROMPT);
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
  SET_STRING(IDS_FILE_BROWSER, UNMOUNT_ARCHIVE);
  SET_STRING(IDS_FILE_BROWSER, FORMAT_DEVICE);

  SET_STRING(IDS_FILE_BROWSER, GALLERY);

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

  SET_STRING(IDS_FILE_BROWSER, SELECTION_COPIED);
  SET_STRING(IDS_FILE_BROWSER, SELECTION_CUT);
  SET_STRING(IDS_FILE_BROWSER, PASTE_STARTED);
  SET_STRING(IDS_FILE_BROWSER, PASTE_SOME_PROGRESS);
  SET_STRING(IDS_FILE_BROWSER, PASTE_COMPLETE);
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
  SET_STRING(IDS_FILE_BROWSER, NOTHING_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, ONE_FILE_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, ONE_DIRECTORY_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_FILES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_DIRECTORIES_SELECTED);
  SET_STRING(IDS_FILE_BROWSER, MANY_ENTRIES_SELECTED);

  SET_STRING(IDS_FILE_BROWSER, PLAYBACK_ERROR);

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
  SET_STRING(IDS_FILE_BROWSER, PLAIN_TEXT_FILE_TYPE);

  SET_STRING(IDS_FILEBROWSER, ENQUEUE);
#undef SET_STRING

  // TODO(serya): Create a new string in .grd file for this one in M13.
  dict->SetString("PREVIEW_IMAGE",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_VIEW_CERT_BUTTON));
  dict->SetString("PLAY_MEDIA",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAY));
#if defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableArchives))
    dict->SetString("ENABLE_ARCHIVES", "true");
#endif

  return true;
}
