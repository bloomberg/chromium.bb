// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_handler_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_task_executor.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;

namespace file_handler_util {

const char kTaskFile[] = "file";
const char kTaskDrive[] = "drive";
const char kTaskWebIntent[] = "web-intent";
const char kTaskApp[] = "app";

namespace {

// Legacy Drive task extension prefix, used by CrackTaskID.
const char kDriveTaskExtensionPrefix[] = "drive-app:";
const size_t kDriveTaskExtensionPrefixLength =
    arraysize(kDriveTaskExtensionPrefix) - 1;

typedef std::set<const FileBrowserHandler*> FileBrowserHandlerSet;

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

const char kFileBrowserExtensionId[] = "hhaomjibdihmijegdhdafkllkbggdgoj";

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

bool IsBuiltinTask(const FileBrowserHandler* task) {
  return (task->extension_id() == kFileBrowserExtensionId ||
          task->extension_id() == extension_misc::kQuickOfficeDevExtensionId ||
          task->extension_id() == extension_misc::kQuickOfficeExtensionId);
}

bool MatchesAllURLs(const FileBrowserHandler* handler) {
  const std::set<URLPattern>& patterns =
      handler->file_url_patterns().patterns();
  for (std::set<URLPattern>::const_iterator it = patterns.begin();
       it != patterns.end();
       ++it) {
    if (it->match_all_urls())
      return true;
  }
  return false;
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
                            FileBrowserHandlerSet* results) {
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

      results->insert(action_iter->get());
    }
  }
  return true;
}

}  // namespace

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
         task_type == kTaskWebIntent ||
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
           *task_type == kTaskWebIntent ||
           *task_type == kTaskApp);
  }

  if (action_id)
    *action_id = result[2];

  return true;
}

// Find a specific handler in the handler list.
FileBrowserHandlerSet::iterator FindHandler(
    FileBrowserHandlerSet* handler_set,
    const std::string& extension_id,
    const std::string& id) {
  FileBrowserHandlerSet::iterator iter = handler_set->begin();
  while (iter != handler_set->end() &&
         !((*iter)->extension_id() == extension_id &&
           (*iter)->id() == id)) {
    iter++;
  }
  return iter;
}

// Given the list of selected files, returns array of file action tasks
// that are shared between them.
void FindDefaultTasks(Profile* profile,
                      const std::vector<GURL>& files_list,
                      const FileBrowserHandlerSet& common_tasks,
                      FileBrowserHandlerSet* default_tasks) {
  DCHECK(default_tasks);
  default_tasks->clear();

  std::set<std::string> default_ids;
  for (std::vector<GURL>::const_iterator it = files_list.begin();
       it != files_list.end(); ++it) {
    // Get the default task for this file based only on the extension (since
    // we don't have MIME types here), and add it to the set of default tasks.
    fileapi::FileSystemURL filesystem_url(*it);
    if (filesystem_url.is_valid() &&
        (filesystem_url.type() == fileapi::kFileSystemTypeDrive ||
         filesystem_url.type() == fileapi::kFileSystemTypeNativeMedia ||
         filesystem_url.type() == fileapi::kFileSystemTypeNativeLocal)) {
      std::string task_id = file_handler_util::GetDefaultTaskIdFromPrefs(
          profile, "", filesystem_url.virtual_path().Extension());
      if (!task_id.empty())
        default_ids.insert(task_id);
    }
  }

  const FileBrowserHandler* builtin_task = NULL;
  // Convert the default task IDs collected above to one of the handler pointers
  // from common_tasks.
  for (FileBrowserHandlerSet::const_iterator task_iter = common_tasks.begin();
       task_iter != common_tasks.end(); ++task_iter) {
    std::string task_id = MakeTaskID((*task_iter)->extension_id(), kTaskFile,
                                     (*task_iter)->id());
    std::set<std::string>::iterator default_iter = default_ids.find(task_id);
    if (default_iter != default_ids.end()) {
      default_tasks->insert(*task_iter);
      continue;
    }

    // If it's a built in task, remember it. If there are no default tasks among
    // common tasks, builtin task will be used as a fallback.
    // Note that builtin tasks are not overlapping, so there can be at most one
    // builtin tasks for each set of files.
    if (IsBuiltinTask(*task_iter))
      builtin_task = *task_iter;
  }

  // If there are no default tasks found, use builtin task (if found) as a
  // default.
  if (builtin_task && default_tasks->empty())
    default_tasks->insert(builtin_task);
}

// Given the list of selected files, returns array of context menu tasks
// that are shared
bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     FileBrowserHandlerSet* common_tasks) {
  DCHECK(common_tasks);
  common_tasks->clear();

  FileBrowserHandlerSet common_task_set;
  std::set<std::string> default_task_ids;
  for (std::vector<GURL>::const_iterator it = files_list.begin();
       it != files_list.end(); ++it) {
    FileBrowserHandlerSet file_actions;
    if (!GetFileBrowserHandlers(profile, *it, &file_actions))
      return false;
    // If there is nothing to do for one file, the intersection of tasks for all
    // files will be empty at the end, and so will the default tasks.
    if (file_actions.empty())
      return true;

    // For the very first file, just copy all the elements.
    if (it == files_list.begin()) {
      common_task_set = file_actions;
    } else {
      // For all additional files, find intersection between the accumulated and
      // file specific set.
      FileBrowserHandlerSet intersection;
      std::set_intersection(common_task_set.begin(), common_task_set.end(),
                            file_actions.begin(), file_actions.end(),
                            std::inserter(intersection,
                                          intersection.begin()));
      common_task_set = intersection;
      if (common_task_set.empty())
        return true;
    }
  }

  FileBrowserHandlerSet::iterator watch_iter = FindHandler(
      &common_task_set, kFileBrowserDomain, kFileBrowserWatchTaskId);
  FileBrowserHandlerSet::iterator gallery_iter = FindHandler(
      &common_task_set, kFileBrowserDomain, kFileBrowserGalleryTaskId);
  if (watch_iter != common_task_set.end() &&
      gallery_iter != common_task_set.end()) {
    // Both "watch" and "gallery" actions are applicable which means that the
    // selection is all videos. Showing them both is confusing, so we only keep
    // the one that makes more sense ("watch" for single selection, "gallery"
    // for multiple selection).
    if (files_list.size() == 1)
      common_task_set.erase(gallery_iter);
    else
      common_task_set.erase(watch_iter);
  }

  common_tasks->swap(common_task_set);
  return true;
}

bool GetTaskForURL(
    Profile* profile, const GURL& url, const FileBrowserHandler** handler) {
  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  FileBrowserHandlerSet default_tasks;
  FileBrowserHandlerSet common_tasks;
  if (!FindCommonTasks(profile, file_urls, &common_tasks))
    return false;

  if (common_tasks.empty())
    return false;

  FindDefaultTasks(profile, file_urls, common_tasks, &default_tasks);

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

class ExtensionTaskExecutor : public FileTaskExecutor {
 public:
  // FileTaskExecutor overrides.
  virtual bool ExecuteAndNotify(const std::vector<GURL>& file_urls,
                       const FileTaskFinishedCallback& done) OVERRIDE;

 private:
  // FileTaskExecutor is the only class allowed to create one.
  friend class FileTaskExecutor;

  ExtensionTaskExecutor(Profile* profile,
                        const GURL& source_url,
                        const std::string& file_browser_id,
                        int32 tab_id,
                        const std::string& extension_id,
                        const std::string& action_id);
  virtual ~ExtensionTaskExecutor();

  struct FileDefinition {
    FileDefinition();
    ~FileDefinition();

    GURL target_file_url;
    FilePath virtual_path;
    FilePath absolute_path;
    bool is_directory;
  };

  typedef std::vector<FileDefinition> FileDefinitionList;
  class ExecuteTasksFileSystemCallbackDispatcher;
  void RequestFileEntryOnFileThread(
      scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
      const GURL& handler_base_url,
      const scoped_refptr<const extensions::Extension>& handler,
      int handler_pid,
      const std::vector<GURL>& file_urls);

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

  // Populates |handler_host_permissions| with file path-permissions pairs that
  // will be given to the handler extension host process.
  void InitHandlerHostFileAccessPermissions(
      const FileDefinitionList& file_list,
      const extensions::Extension* handler_extension,
      const base::Closure& callback);

  // Invoked upon completion of InitHandlerHostFileAccessPermissions initiated
  // by ExecuteFileActionsOnUIThread.
  void OnInitAccessForExecuteFileActionsOnUIThread(
      const std::string& file_system_name,
      const GURL& file_system_root,
      const FileDefinitionList& file_list,
      int handler_pid);

  // Registers file permissions from |handler_host_permissions_| with
  // ChildProcessSecurityPolicy for process with id |handler_pid|.
  void SetupHandlerHostFileAccessPermissions(int handler_pid);

  int32 tab_id_;
  const std::string action_id_;
  FileTaskFinishedCallback done_;

  // (File path, permission for file path) pairs for the handler.
  std::vector<std::pair<FilePath, int> > handler_host_permissions_;
};

class WebIntentTaskExecutor : public FileTaskExecutor {
 public:
  // FileTaskExecutor overrides.
  virtual bool ExecuteAndNotify(const std::vector<GURL>& file_urls,
                                const FileTaskFinishedCallback& done) OVERRIDE;

 private:
  // FileTaskExecutor is the only class allowed to create one.
  friend class FileTaskExecutor;

  WebIntentTaskExecutor(Profile* profile,
                        const GURL& source_url,
                        const std::string& file_browser_id,
                        const std::string& extension_id,
                        const std::string& action_id);
  virtual ~WebIntentTaskExecutor();

  const std::string extension_id_;
  const std::string action_id_;
};

class AppTaskExecutor : public FileTaskExecutor {
 public:
  // FileTaskExecutor overrides.
  virtual bool ExecuteAndNotify(const std::vector<GURL>& file_urls,
                                const FileTaskFinishedCallback& done) OVERRIDE;

 private:
  // FileTaskExecutor is the only class allowed to create one.
  friend class FileTaskExecutor;

  AppTaskExecutor(Profile* profile,
                  const GURL& source_url,
                  const std::string& file_browser_id,
                  const std::string& extension_id,
                  const std::string& action_id);
  virtual ~AppTaskExecutor();

  const std::string extension_id_;
  const std::string action_id_;
};

// static
FileTaskExecutor* FileTaskExecutor::Create(Profile* profile,
                                           const GURL& source_url,
                                           const std::string& file_browser_id,
                                           int32 tab_id,
                                           const std::string& extension_id,
                                           const std::string& task_type,
                                           const std::string& action_id) {
  if (task_type == kTaskFile)
    return new ExtensionTaskExecutor(profile,
                                     source_url,
                                     file_browser_id,
                                     tab_id,
                                     extension_id,
                                     action_id);

  if (task_type == kTaskDrive)
    return new drive::DriveTaskExecutor(profile,
                                        extension_id,  // really app_id
                                        action_id);

  if (task_type == kTaskWebIntent)
    return new WebIntentTaskExecutor(profile,
                                     source_url,
                                     file_browser_id,
                                     extension_id,
                                     action_id);

  if (task_type == kTaskApp)
    return new AppTaskExecutor(profile,
                               source_url,
                               file_browser_id,
                               extension_id,
                               action_id);

  NOTREACHED();
  return NULL;
}

FileTaskExecutor::FileTaskExecutor(Profile* profile,
                                   const GURL& source_url,
                                   const std::string& file_browser_id,
                                   const std::string& extension_id)
  : profile_(profile),
    source_url_(source_url),
    file_browser_id_(file_browser_id),
    extension_id_(extension_id) {
}

FileTaskExecutor::~FileTaskExecutor() {
}

bool FileTaskExecutor::Execute(const std::vector<GURL>& file_urls) {
  return ExecuteAndNotify(file_urls, FileTaskFinishedCallback());
}

bool FileTaskExecutor::FileBrowserHasAccessPermissionForFiles(
    const std::vector<GURL>& files) {
  // Check if the file browser extension has permissions for the files in its
  // file system context.
  GURL site = extensions::ExtensionSystem::Get(profile())->extension_service()->
      GetSiteForExtensionId(file_browser_id_);
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetStoragePartitionForSite(profile(), site)->
          GetFileSystemContext()->external_provider();

  if (!external_provider)
    return false;

  for (size_t i = 0; i < files.size(); ++i) {
    fileapi::FileSystemURL url(files[i]);
    // Make sure this url really being used by the right caller extension.
    if (source_url_.GetOrigin() != url.origin())
      return false;

    if (!chromeos::CrosMountPointProvider::CanHandleURL(url) ||
        !external_provider->IsAccessAllowed(url)) {
      return false;
    }
  }

  return true;
}

// TODO(kaznacheev): Remove this method and inline its implementation at the
// only place where it is used (DriveTaskExecutor::OnAppAuthorized)
Browser* FileTaskExecutor::GetBrowser() const {
  return chrome::FindOrCreateTabbedBrowser(
      profile_ ? profile_ : ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
}

const Extension* FileTaskExecutor::GetExtension() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  return service ? service->GetExtensionById(extension_id_, false) :
                   NULL;
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
      const std::vector<GURL>& file_urls) {
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
    for (std::vector<GURL>::iterator iter = origin_file_urls_.begin();
         iter != origin_file_urls_.end();
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
      const std::vector<GURL>& file_urls)
      : executor_(executor),
        file_system_context_handler_(file_system_context_handler),
        handler_extension_(handler_extension),
        handler_pid_(handler_pid),
        action_id_(action_id),
        origin_file_urls_(file_urls) {
    DCHECK(executor_);
  }

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  bool SetupFileAccessPermissions(const GURL& origin_file_url,
                                  FileDefinition* file) {
    if (!handler_extension_.get())
      return false;

    if (handler_pid_ == 0)
      return false;

    fileapi::FileSystemURL url(origin_file_url);

    fileapi::ExternalFileSystemMountPointProvider* external_provider_handler =
        file_system_context_handler_->external_provider();

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;

    FilePath local_path = url.path();
    FilePath virtual_path = url.virtual_path();

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
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        handler_extension_->id()));
    GURL base_url = fileapi::GetFileSystemRootURI(target_origin_url,
        fileapi::kFileSystemTypeExternal);
    file->target_file_url = GURL(base_url.spec() + virtual_path.value());
    file->virtual_path = virtual_path;
    file->is_directory = file_info.is_directory;
    file->absolute_path = local_path;
    return true;
  }

  ExtensionTaskExecutor* executor_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_handler_;
  scoped_refptr<const Extension> handler_extension_;
  int handler_pid_;
  std::string action_id_;
  std::vector<GURL> origin_file_urls_;
  DISALLOW_COPY_AND_ASSIGN(ExecuteTasksFileSystemCallbackDispatcher);
};

ExtensionTaskExecutor::ExtensionTaskExecutor(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    int tab_id,
    const std::string& extension_id,
    const std::string& action_id)
    : FileTaskExecutor(profile, source_url, file_browser_id, extension_id),
      tab_id_(tab_id),
      action_id_(action_id) {
}

ExtensionTaskExecutor::~ExtensionTaskExecutor() {}

bool ExtensionTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(file_urls))
    return false;

  scoped_refptr<const Extension> handler = GetExtension();
  if (!handler.get())
    return false;

  int handler_pid = ExtractProcessFromExtensionId(profile(), handler->id());
  if (handler_pid <= 0) {
    if (!handler->has_lazy_background_page())
      return false;
  }

  done_ = done;

  // Get file system context for the extension to which onExecute event will be
  // send. The file access permissions will be granted to the extension in the
  // file system context for the files in |file_urls|.
  GURL site = extensions::ExtensionSystem::Get(profile())->extension_service()->
      GetSiteForExtensionId(handler->id());
  scoped_refptr<fileapi::FileSystemContext> file_system_context_handler =
      BrowserContext::GetStoragePartitionForSite(profile(), site)->
      GetFileSystemContext();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ExtensionTaskExecutor::RequestFileEntryOnFileThread,
          this,
          file_system_context_handler,
          Extension::GetBaseURLFromExtensionId(handler->id()),
          handler,
          handler_pid,
          file_urls));
  return true;
}

void ExtensionTaskExecutor::RequestFileEntryOnFileThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
    const GURL& handler_base_url,
    const scoped_refptr<const Extension>& handler,
    int handler_pid,
    const std::vector<GURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  GURL origin_url = handler_base_url.GetOrigin();
  file_system_context_handler->OpenFileSystem(
      origin_url, fileapi::kFileSystemTypeExternal, false, // create
      ExecuteTasksFileSystemCallbackDispatcher::CreateCallback(
          this,
          file_system_context_handler,
          handler,
          handler_pid,
          action_id_,
          file_urls));
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

  const Extension* extension = GetExtension();
  if (!extension) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  InitHandlerHostFileAccessPermissions(
      file_list,
      extension,
      base::Bind(
          &ExtensionTaskExecutor::OnInitAccessForExecuteFileActionsOnUIThread,
          this,
          file_system_name,
          file_system_root,
          file_list,
          handler_pid));
}

void ExtensionTaskExecutor::OnInitAccessForExecuteFileActionsOnUIThread(
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list,
    int handler_pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension = GetExtension();
  if (!extension) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  if (handler_pid > 0) {
    SetupPermissionsAndDispatchEvent(file_system_name, file_system_root,
        file_list, handler_pid, NULL);
  } else {
    // We have to wake the handler background page before we proceed.
    extensions::LazyBackgroundTaskQueue* queue =
        extensions::ExtensionSystem::Get(profile())->
        lazy_background_task_queue();
    if (!queue->ShouldEnqueueTask(profile(), extension)) {
      ExecuteDoneOnUIThread(false);
      return;
    }
    queue->AddPendingTask(
        profile(), extension_id(),
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
      extensions::ExtensionSystem::Get(profile())->event_router();
  if (!event_router) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  SetupHandlerHostFileAccessPermissions(handler_pid);

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(new base::StringValue(action_id_));
  DictionaryValue* details = new DictionaryValue();
  event_args->Append(details);
  // Get file definitions. These will be replaced with Entry instances by
  // chromeHidden.Event.dispatchEvent() method from event_binding.js.
  ListValue* files_urls = new ListValue();
  details->Set("entries", files_urls);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    DictionaryValue* file_def = new DictionaryValue();
    files_urls->Append(file_def);
    file_def->SetString("fileSystemName", file_system_name);
    file_def->SetString("fileSystemRoot", file_system_root.spec());
    FilePath root(FILE_PATH_LITERAL("/"));
    FilePath full_path = root.Append(iter->virtual_path);
    file_def->SetString("fileFullPath", full_path.value());
    file_def->SetBoolean("fileIsDirectory", iter->is_directory);
  }

  details->SetInteger("tab_id", tab_id_);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      "fileBrowserHandler.onExecute", event_args.Pass()));
  event->restrict_to_profile = profile();
  event_router->DispatchEventToExtension(extension_id(), event.Pass());

  ExecuteDoneOnUIThread(true);
}

void ExtensionTaskExecutor::InitHandlerHostFileAccessPermissions(
    const FileDefinitionList& file_list,
    const Extension* handler_extension,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<std::vector<FilePath> > gdata_paths(new std::vector<FilePath>);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    // Setup permission for file's absolute file.
    handler_host_permissions_.push_back(std::make_pair(iter->absolute_path,
        GetAccessPermissionsForFileBrowserHandler(handler_extension,
                                                  action_id_)));

    if (drive::util::IsUnderDriveMountPoint(iter->absolute_path))
      gdata_paths->push_back(iter->virtual_path);
  }

  if (gdata_paths->empty()) {
    // Invoke callback if none of the files are on gdata mount point.
    callback.Run();
    return;
  }

  // For files on gdata mount point, we'll have to give handler host permissions
  // for their cache paths. This has to be called on UI thread.
  drive::util::InsertDriveCachePathsPermissions(profile(),
                                                gdata_paths.Pass(),
                                                &handler_host_permissions_,
                                                callback);
}

void ExtensionTaskExecutor::SetupHandlerHostFileAccessPermissions(
    int handler_pid) {
  for (size_t i = 0; i < handler_host_permissions_.size(); i++) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        handler_pid,
        handler_host_permissions_[i].first,
        handler_host_permissions_[i].second);
  }

  // We don't need this anymore.
  handler_host_permissions_.clear();
}

WebIntentTaskExecutor::WebIntentTaskExecutor(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    const std::string& extension_id,
    const std::string& action_id)
    : FileTaskExecutor(profile, source_url, file_browser_id, extension_id),
      action_id_(action_id) {
}

WebIntentTaskExecutor::~WebIntentTaskExecutor() {}

bool WebIntentTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(file_urls))
    return false;

  for (size_t i = 0; i != file_urls.size(); ++i) {
    fileapi::FileSystemURL url(file_urls[i]);
    extensions::LaunchPlatformAppWithPath(profile(), GetExtension(),
                                          url.path());
  }

  if (!done.is_null())
    done.Run(true);

  return true;
}

AppTaskExecutor::AppTaskExecutor(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    const std::string& extension_id,
    const std::string& action_id)
    : FileTaskExecutor(profile, source_url, file_browser_id,  extension_id),
      action_id_(action_id) {
}

AppTaskExecutor::~AppTaskExecutor() {}

bool AppTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(file_urls))
    return false;

  for (size_t i = 0; i != file_urls.size(); ++i) {
    fileapi::FileSystemURL url(file_urls[i]);
    extensions::LaunchPlatformAppWithFileHandler(profile(), GetExtension(),
        action_id_, url.path());
  }

  if (!done.is_null())
    done.Run(true);

  return true;
}

} // namespace file_handler_util
