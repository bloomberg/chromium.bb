// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_handler_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_task_executor.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handler.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "webkit/browser/chromeos/fileapi/cros_mount_point_provider.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using fileapi::FileSystemURL;

namespace file_handler_util {

const char kTaskFile[] = "file";
const char kTaskDrive[] = "drive";
const char kTaskApp[] = "app";

namespace {

// Legacy Drive task extension prefix, used by CrackTaskID.
const char kDriveTaskExtensionPrefix[] = "drive-app:";
const size_t kDriveTaskExtensionPrefixLength =
    arraysize(kDriveTaskExtensionPrefix) - 1;

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

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

// Returns process id of the process the extension is running in.
int ExtractProcessFromExtensionId(Profile* profile,
                                  const std::string& extension_id) {
  GURL extension_url =
      Extension::GetBaseURLFromExtensionId(extension_id);
  ExtensionProcessManager* manager =
    extensions::ExtensionSystem::Get(profile)->process_manager();

  SiteInstance* site_instance = manager->GetSiteInstanceForURL(extension_url);
  if (!site_instance || !site_instance->HasProcess())
    return -1;
  content::RenderProcessHost* process = site_instance->GetProcess();

  return process->GetID();
}

const FileBrowserHandler* FindFileBrowserHandler(const Extension* extension,
                                                 const std::string& action_id) {
  FileBrowserHandler::List* handler_list =
      FileBrowserHandler::GetHandlers(extension);
  for (FileBrowserHandler::List::const_iterator action_iter =
           handler_list->begin();
       action_iter != handler_list->end();
       ++action_iter) {
    if (action_iter->get()->id() == action_id)
      return action_iter->get();
  }
  return NULL;
}

unsigned int GetAccessPermissionsForFileBrowserHandler(
    const Extension* extension,
    const std::string& action_id) {
  const FileBrowserHandler* action =
      FindFileBrowserHandler(extension, action_id);
  if (!action)
    return 0;
  unsigned int result = 0;
  if (action->CanRead())
    result |= kReadOnlyFilePermissions;
  if (action->CanWrite())
    result |= kReadWriteFilePermissions;
  // TODO(tbarzic): We don't handle Create yet.
  return result;
}

std::string EscapedUtf8ToLower(const std::string& str) {
  string16 utf16 = UTF8ToUTF16(
      net::UnescapeURLComponent(str, net::UnescapeRule::NORMAL));
  return net::EscapeUrlEncodedData(
      UTF16ToUTF8(base::i18n::ToLower(utf16)),
      false /* do not replace space with plus */);
}

bool GetFileBrowserHandlers(Profile* profile,
                            const GURL& selected_file_url,
                            FileBrowserHandlerList* results) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;  // In unit-tests, we may not have an ExtensionService.

  // We need case-insensitive matching, and pattern in the handler is already
  // in lower case.
  const GURL lowercase_url(EscapedUtf8ToLower(selected_file_url.spec()));

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = *iter;
    if (profile->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    FileBrowserHandler::List* handler_list =
        FileBrowserHandler::GetHandlers(extension);
    if (!handler_list)
      continue;
    for (FileBrowserHandler::List::const_iterator action_iter =
             handler_list->begin();
         action_iter != handler_list->end();
         ++action_iter) {
      const FileBrowserHandler* action = action_iter->get();
      if (!action->MatchesURL(lowercase_url))
        continue;

      results->push_back(action_iter->get());
    }
  }
  return true;
}

// Checks if the file browser extension has permissions for the files in its
// file system context.
bool FileBrowserHasAccessPermissionForFiles(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    const std::vector<FileSystemURL>& files) {
  GURL site = extensions::ExtensionSystem::Get(profile)->extension_service()->
      GetSiteForExtensionId(file_browser_id);
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_provider();

  if (!external_provider)
    return false;

  for (size_t i = 0; i < files.size(); ++i) {
    // Make sure this url really being used by the right caller extension.
    if (source_url.GetOrigin() != files[i].origin())
      return false;

    if (!chromeos::CrosMountPointProvider::CanHandleURL(files[i]) ||
        !external_provider->IsAccessAllowed(files[i])) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool IsFallbackTask(const FileBrowserHandler* task) {
  return (task->extension_id() == kFileBrowserDomain ||
          task->extension_id() ==
              extension_misc::kQuickOfficeComponentExtensionId ||
          task->extension_id() == extension_misc::kQuickOfficeDevExtensionId ||
          task->extension_id() == extension_misc::kQuickOfficeExtensionId);
}

void UpdateDefaultTask(Profile* profile,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  if (!profile || !profile->GetPrefs())
    return;

  if (!mime_types.empty()) {
    DictionaryPrefUpdate mime_type_pref(profile->GetPrefs(),
                                        prefs::kDefaultTasksByMimeType);
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
        iter != mime_types.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      mime_type_pref->SetWithoutPathExpansion(*iter, value);
    }
  }

  if (!suffixes.empty()) {
    DictionaryPrefUpdate mime_type_pref(profile->GetPrefs(),
                                        prefs::kDefaultTasksBySuffix);
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
        iter != suffixes.end(); ++iter) {
      base::StringValue* value = new base::StringValue(task_id);
      // Suffixes are case insensitive.
      std::string lower_suffix = StringToLowerASCII(*iter);
      mime_type_pref->SetWithoutPathExpansion(lower_suffix, value);
    }
  }
}

std::string GetDefaultTaskIdFromPrefs(Profile* profile,
                                      const std::string& mime_type,
                                      const std::string& suffix) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
      << " and suffix: " << suffix;
  std::string task_id;
  if (!mime_type.empty()) {
    const DictionaryValue* mime_task_prefs =
        profile->GetPrefs()->GetDictionary(prefs::kDefaultTasksByMimeType);
    DCHECK(mime_task_prefs);
    LOG_IF(ERROR, !mime_task_prefs) << "Unable to open MIME type prefs";
    if (mime_task_prefs &&
        mime_task_prefs->GetStringWithoutPathExpansion(mime_type, &task_id)) {
      VLOG(1) << "Found MIME default handler: " << task_id;
      return task_id;
    }
  }

  const DictionaryValue* suffix_task_prefs =
      profile->GetPrefs()->GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  LOG_IF(ERROR, !suffix_task_prefs) << "Unable to open suffix prefs";
  std::string lower_suffix = StringToLowerASCII(suffix);
  if (suffix_task_prefs)
    suffix_task_prefs->GetStringWithoutPathExpansion(lower_suffix, &task_id);
  VLOG_IF(1, !task_id.empty()) << "Found suffix default handler: " << task_id;
  return task_id;
}

int GetReadWritePermissions() {
  return kReadWriteFilePermissions;
}

int GetReadOnlyPermissions() {
  return kReadOnlyFilePermissions;
}

std::string MakeTaskID(const std::string& extension_id,
                       const std::string& task_type,
                       const std::string& action_id) {
  DCHECK(task_type == kTaskFile ||
         task_type == kTaskDrive ||
         task_type == kTaskApp);
  return base::StringPrintf("%s|%s|%s",
                            extension_id.c_str(),
                            task_type.c_str(),
                            action_id.c_str());
}

// Breaks down task_id that is used between getFileTasks() and executeTask() on
// its building blocks. task_id field the following structure:
//     <extension-id>|<task-type>|<task-action-id>
bool CrackTaskID(const std::string& task_id,
                 std::string* extension_id,
                 std::string* task_type,
                 std::string* action_id) {
  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);

  // Parse historic task_id parameters that only contain two parts. Drive tasks
  // are identified by a prefix "drive-app:" on the extension ID.
  if (count == 2) {
    if (StartsWithASCII(result[0], kDriveTaskExtensionPrefix, true)) {
      if (task_type)
        *task_type = kTaskDrive;

      if (extension_id)
        *extension_id = result[0].substr(kDriveTaskExtensionPrefixLength);
    } else {
      if (task_type)
        *task_type = kTaskFile;

      if (extension_id)
        *extension_id = result[0];
    }

    if (action_id)
      *action_id = result[1];

    return true;
  }

  if (count != 3)
    return false;

  if (extension_id)
    *extension_id = result[0];

  if (task_type) {
    *task_type = result[1];
    DCHECK(*task_type == kTaskFile ||
           *task_type == kTaskDrive ||
           *task_type == kTaskApp);
  }

  if (action_id)
    *action_id = result[2];

  return true;
}

// Find a specific handler in the handler list.
FileBrowserHandlerList::iterator FindHandler(
    FileBrowserHandlerList* handler_list,
    const std::string& extension_id,
    const std::string& id) {
  FileBrowserHandlerList::iterator iter = handler_list->begin();
  while (iter != handler_list->end() &&
         !((*iter)->extension_id() == extension_id &&
           (*iter)->id() == id)) {
    ++iter;
  }
  return iter;
}

// Given the list of selected files, returns array of file action tasks
// that are shared between them.
void FindDefaultTasks(Profile* profile,
                      const std::vector<base::FilePath>& files_list,
                      const FileBrowserHandlerList& common_tasks,
                      FileBrowserHandlerList* default_tasks) {
  DCHECK(default_tasks);
  default_tasks->clear();

  std::set<std::string> default_ids;
  for (std::vector<base::FilePath>::const_iterator it = files_list.begin();
       it != files_list.end(); ++it) {
    std::string task_id = file_handler_util::GetDefaultTaskIdFromPrefs(
        profile, "", it->Extension());
    if (!task_id.empty())
      default_ids.insert(task_id);
  }

  const FileBrowserHandler* fallback_task = NULL;
  // Convert the default task IDs collected above to one of the handler pointers
  // from common_tasks.
  for (FileBrowserHandlerList::const_iterator task_iter = common_tasks.begin();
       task_iter != common_tasks.end(); ++task_iter) {
    std::string task_id = MakeTaskID((*task_iter)->extension_id(), kTaskFile,
                                     (*task_iter)->id());
    std::set<std::string>::iterator default_iter = default_ids.find(task_id);
    if (default_iter != default_ids.end()) {
      default_tasks->push_back(*task_iter);
      continue;
    }

    // Remember the first fallback task.
    if (!fallback_task && IsFallbackTask(*task_iter))
      fallback_task = *task_iter;
  }

  // If there are no default tasks found, use fallback as default.
  if (fallback_task && default_tasks->empty())
    default_tasks->push_back(fallback_task);
}

// Given the list of selected files, returns array of context menu tasks
// that are shared
bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     FileBrowserHandlerList* common_tasks) {
  DCHECK(common_tasks);
  common_tasks->clear();

  FileBrowserHandlerList common_task_list;
  std::set<std::string> default_task_ids;
  for (std::vector<GURL>::const_iterator it = files_list.begin();
       it != files_list.end(); ++it) {
    FileBrowserHandlerList file_actions;
    if (!GetFileBrowserHandlers(profile, *it, &file_actions))
      return false;
    // If there is nothing to do for one file, the intersection of tasks for all
    // files will be empty at the end, and so will the default tasks.
    if (file_actions.empty())
      return true;

    // For the very first file, just copy all the elements.
    if (it == files_list.begin()) {
      common_task_list = file_actions;
    } else {
      // For all additional files, find intersection between the accumulated and
      // file specific set.
      FileBrowserHandlerList intersection;
      std::set_intersection(common_task_list.begin(), common_task_list.end(),
                            file_actions.begin(), file_actions.end(),
                            std::back_inserter(intersection));
      common_task_list = intersection;
      if (common_task_list.empty())
        return true;
    }
  }

  FileBrowserHandlerList::iterator watch_iter = FindHandler(
      &common_task_list, kFileBrowserDomain, kFileBrowserWatchTaskId);
  FileBrowserHandlerList::iterator gallery_iter = FindHandler(
      &common_task_list, kFileBrowserDomain, kFileBrowserGalleryTaskId);
  if (watch_iter != common_task_list.end() &&
      gallery_iter != common_task_list.end()) {
    // Both "watch" and "gallery" actions are applicable which means that the
    // selection is all videos. Showing them both is confusing, so we only keep
    // the one that makes more sense ("watch" for single selection, "gallery"
    // for multiple selection).
    if (files_list.size() == 1)
      common_task_list.erase(gallery_iter);
    else
      common_task_list.erase(watch_iter);
  }

  common_tasks->swap(common_task_list);
  return true;
}

bool GetTaskForURLAndPath(Profile* profile,
                   const GURL& url,
                   const base::FilePath& file_path,
                   const FileBrowserHandler** handler) {
  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  FileBrowserHandlerList default_tasks;
  FileBrowserHandlerList common_tasks;
  if (!FindCommonTasks(profile, file_urls, &common_tasks))
    return false;

  if (common_tasks.empty())
    return false;

  std::vector<base::FilePath> file_paths;
  file_paths.push_back(file_path);

  FindDefaultTasks(profile, file_paths, common_tasks, &default_tasks);

  // If there's none, or more than one, then we don't have a canonical default.
  if (!default_tasks.empty()) {
    // There should not be multiple default tasks for a single URL.
    DCHECK_EQ(1u, default_tasks.size());

    *handler = *default_tasks.begin();
    return true;
  }

  // If there are no default tasks, use first task in the list (file manager
  // does the same in this situation).
  // TODO(tbarzic): This is so not optimal behaviour.
  *handler = *common_tasks.begin();
  return true;
}

// ExtensionTaskExecutor executes tasks with kTaskFile type.
// TODO(hashimoto): Make this non ref-counted. crbug.com/231173
class ExtensionTaskExecutor
    : public base::RefCountedThreadSafe<ExtensionTaskExecutor> {
 public:
  ExtensionTaskExecutor(Profile* profile,
                        const Extension* extension,
                        int32 tab_id,
                        const std::string& action_id);

  // Executes the task for each file. When true is returned, |done| will be run
  // with the execution result.
  bool Execute(const std::vector<FileSystemURL>& file_urls,
               const FileTaskFinishedCallback& done);

 private:
  friend class base::RefCountedThreadSafe<ExtensionTaskExecutor>;
  virtual ~ExtensionTaskExecutor();

  struct FileDefinition {
    FileDefinition();
    ~FileDefinition();

    base::FilePath virtual_path;
    base::FilePath absolute_path;
    bool is_directory;
  };

  typedef std::vector<FileDefinition> FileDefinitionList;
  class ExecuteTasksFileSystemCallbackDispatcher;

  void ExecuteDoneOnUIThread(bool success);
  void ExecuteFileActionsOnUIThread(const std::string& file_system_name,
                                    const GURL& file_system_root,
                                    const FileDefinitionList& file_list,
                                    int handler_pid);
  void SetupPermissionsAndDispatchEvent(const std::string& file_system_name,
                                        const GURL& file_system_root,
                                        const FileDefinitionList& file_list,
                                        int handler_pid_in,
                                        extensions::ExtensionHost* host);

  // Registers file permissions from |handler_host_permissions_| with
  // ChildProcessSecurityPolicy for process with id |handler_pid|.
  void SetupHandlerHostFileAccessPermissions(
      const FileDefinitionList& file_list,
      const Extension* extension,
      int handler_pid);

  Profile* profile_;
  scoped_refptr<const Extension> extension_;
  int32 tab_id_;
  const std::string action_id_;
  FileTaskFinishedCallback done_;
};

bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const std::string& file_browser_id,
                     int32 tab_id,
                     const std::string& extension_id,
                     const std::string& task_type,
                     const std::string& action_id,
                     const std::vector<FileSystemURL>& file_urls,
                     const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(profile, source_url,
                                              file_browser_id, file_urls))
    return false;

  // drive::FileTaskExecutor is responsible to handle drive tasks.
  if (task_type == kTaskDrive) {
    DCHECK_EQ("open-with", action_id);
    drive::FileTaskExecutor* executor =
        new drive::FileTaskExecutor(profile, extension_id);
    executor->Execute(file_urls, done);
    return true;
  }

  // Get the extension.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const Extension* extension = service ?
      service->GetExtensionById(extension_id, false) : NULL;
  if (!extension)
    return false;

  // Execute the task.
  if (task_type == kTaskFile) {
    scoped_refptr<ExtensionTaskExecutor> executor(
        new ExtensionTaskExecutor(profile, extension, tab_id, action_id));
    return executor->Execute(file_urls, done);
  } else if (task_type == kTaskApp) {
    for (size_t i = 0; i != file_urls.size(); ++i) {
      extensions::LaunchPlatformAppWithFileHandler(
          profile, extension, action_id, file_urls[i].path());
    }

    if (!done.is_null())
      done.Run(true);
    return true;
  }
  NOTREACHED();
  return false;
}

ExtensionTaskExecutor::FileDefinition::FileDefinition() : is_directory(false) {
}

ExtensionTaskExecutor::FileDefinition::~FileDefinition() {
}

class ExtensionTaskExecutor::ExecuteTasksFileSystemCallbackDispatcher {
 public:
  static fileapi::FileSystemContext::OpenFileSystemCallback CreateCallback(
      ExtensionTaskExecutor* executor,
      scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
      scoped_refptr<const Extension> handler_extension,
      int handler_pid,
      const std::string& action_id,
      const std::vector<FileSystemURL>& file_urls) {
    return base::Bind(
        &ExecuteTasksFileSystemCallbackDispatcher::DidOpenFileSystem,
        base::Owned(new ExecuteTasksFileSystemCallbackDispatcher(
            executor, file_system_context_handler, handler_extension,
            handler_pid, action_id, file_urls)));
  }

  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& file_system_name,
                         const GURL& file_system_root) {
    if (result != base::PLATFORM_FILE_OK) {
      DidFail(result);
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    ExtensionTaskExecutor::FileDefinitionList file_list;
    for (std::vector<FileSystemURL>::iterator iter = urls_.begin();
         iter != urls_.end();
         ++iter) {
      // Set up file permission access.
      ExtensionTaskExecutor::FileDefinition file;
      if (!SetupFileAccessPermissions(*iter, &file))
        continue;
      file_list.push_back(file);
    }
    if (file_list.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(
              &ExtensionTaskExecutor::ExecuteDoneOnUIThread,
              executor_,
              false));
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ExtensionTaskExecutor::ExecuteFileActionsOnUIThread,
            executor_,
            file_system_name,
            file_system_root,
            file_list,
            handler_pid_));
  }

  void DidFail(base::PlatformFileError error_code) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ExtensionTaskExecutor::ExecuteDoneOnUIThread,
            executor_,
            false));
  }

 private:
  ExecuteTasksFileSystemCallbackDispatcher(
      ExtensionTaskExecutor* executor,
      scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
      const scoped_refptr<const Extension>& handler_extension,
      int handler_pid,
      const std::string& action_id,
      const std::vector<FileSystemURL>& file_urls)
      : executor_(executor),
        file_system_context_handler_(file_system_context_handler),
        handler_extension_(handler_extension),
        handler_pid_(handler_pid),
        action_id_(action_id),
        urls_(file_urls) {
    DCHECK(executor_.get());
  }

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  bool SetupFileAccessPermissions(const FileSystemURL& url,
                                  FileDefinition* file) {
    if (!handler_extension_.get())
      return false;

    if (handler_pid_ == 0)
      return false;

    fileapi::ExternalFileSystemMountPointProvider* external_provider_handler =
        file_system_context_handler_->external_provider();

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;

    base::FilePath local_path = url.path();
    base::FilePath virtual_path = url.virtual_path();

    bool is_drive_file = url.type() == fileapi::kFileSystemTypeDrive;
    DCHECK(!is_drive_file || drive::util::IsUnderDriveMountPoint(local_path));

    // If the file is under gdata mount point, there is no actual file to be
    // found on the url.path().
    if (!is_drive_file) {
      if (!file_util::PathExists(local_path) ||
          file_util::IsLink(local_path) ||
          !file_util::GetFileInfo(local_path, &file_info)) {
        return false;
      }
    }

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    external_provider_handler->GrantFileAccessToExtension(
        handler_extension_->id(), virtual_path);

    // Output values.
    file->virtual_path = virtual_path;
    file->is_directory = file_info.is_directory;
    file->absolute_path = local_path;
    return true;
  }

  scoped_refptr<ExtensionTaskExecutor> executor_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_handler_;
  scoped_refptr<const Extension> handler_extension_;
  int handler_pid_;
  std::string action_id_;
  std::vector<FileSystemURL> urls_;
  DISALLOW_COPY_AND_ASSIGN(ExecuteTasksFileSystemCallbackDispatcher);
};

ExtensionTaskExecutor::ExtensionTaskExecutor(
    Profile* profile,
    const Extension* extension,
    int32 tab_id,
    const std::string& action_id)
    : profile_(profile),
      extension_(extension),
      tab_id_(tab_id),
      action_id_(action_id) {
}

ExtensionTaskExecutor::~ExtensionTaskExecutor() {}

bool ExtensionTaskExecutor::Execute(const std::vector<FileSystemURL>& file_urls,
                                    const FileTaskFinishedCallback& done) {
  // Forbid calling undeclared handlers.
  if (!FindFileBrowserHandler(extension_, action_id_))
    return false;

  int extension_pid = ExtractProcessFromExtensionId(profile_, extension_->id());
  if (extension_pid <= 0) {
    if (!extensions::BackgroundInfo::HasLazyBackgroundPage(extension_))
      return false;
  }

  done_ = done;

  // Get file system context for the extension to which onExecute event will be
  // send. The file access permissions will be granted to the extension in the
  // file system context for the files in |file_urls|.
  GURL site = extensions::ExtensionSystem::Get(profile_)->extension_service()->
      GetSiteForExtensionId(extension_->id());
  scoped_refptr<fileapi::FileSystemContext> file_system_context_handler =
      BrowserContext::GetStoragePartitionForSite(profile_, site)->
      GetFileSystemContext();

  GURL origin_url =
      Extension::GetBaseURLFromExtensionId(extension_->id()).GetOrigin();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&fileapi::FileSystemContext::OpenFileSystem,
                 file_system_context_handler,
                 origin_url, fileapi::kFileSystemTypeExternal,
                 fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
                 ExecuteTasksFileSystemCallbackDispatcher::CreateCallback(
                     this,
                     file_system_context_handler,
                     extension_,
                     extension_pid,
                     action_id_,
                     file_urls)));
  return true;
}

void ExtensionTaskExecutor::ExecuteDoneOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!done_.is_null())
    done_.Run(success);
  done_.Reset();
}

void ExtensionTaskExecutor::ExecuteFileActionsOnUIThread(
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list,
    int handler_pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (handler_pid > 0) {
    SetupPermissionsAndDispatchEvent(file_system_name, file_system_root,
        file_list, handler_pid, NULL);
  } else {
    // We have to wake the handler background page before we proceed.
    extensions::LazyBackgroundTaskQueue* queue =
        extensions::ExtensionSystem::Get(profile_)->
        lazy_background_task_queue();
    if (!queue->ShouldEnqueueTask(profile_, extension_)) {
      ExecuteDoneOnUIThread(false);
      return;
    }
    queue->AddPendingTask(
        profile_, extension_->id(),
        base::Bind(&ExtensionTaskExecutor::SetupPermissionsAndDispatchEvent,
                   this, file_system_name, file_system_root, file_list,
                   handler_pid));
  }
}

void ExtensionTaskExecutor::SetupPermissionsAndDispatchEvent(
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list,
    int handler_pid_in,
    extensions::ExtensionHost* host) {
  int handler_pid = host ? host->render_process_host()->GetID() :
                           handler_pid_in;

  if (handler_pid <= 0) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!event_router) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  SetupHandlerHostFileAccessPermissions(file_list, extension_, handler_pid);

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(new base::StringValue(action_id_));
  DictionaryValue* details = new DictionaryValue();
  event_args->Append(details);
  // Get file definitions. These will be replaced with Entry instances by
  // dispatchEvent() method from event_binding.js.
  ListValue* files_urls = new ListValue();
  details->Set("entries", files_urls);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    DictionaryValue* file_def = new DictionaryValue();
    files_urls->Append(file_def);
    file_def->SetString("fileSystemName", file_system_name);
    file_def->SetString("fileSystemRoot", file_system_root.spec());
    base::FilePath root(FILE_PATH_LITERAL("/"));
    base::FilePath full_path = root.Append(iter->virtual_path);
    file_def->SetString("fileFullPath", full_path.value());
    file_def->SetBoolean("fileIsDirectory", iter->is_directory);
  }

  details->SetInteger("tab_id", tab_id_);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      "fileBrowserHandler.onExecute", event_args.Pass()));
  event->restrict_to_profile = profile_;
  event_router->DispatchEventToExtension(extension_->id(), event.Pass());

  ExecuteDoneOnUIThread(true);
}

void ExtensionTaskExecutor::SetupHandlerHostFileAccessPermissions(
    const FileDefinitionList& file_list,
    const Extension* extension,
    int handler_pid) {
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        handler_pid,
        iter->absolute_path,
        GetAccessPermissionsForFileBrowserHandler(extension_, action_id_));
  }
}

} // namespace file_handler_util
