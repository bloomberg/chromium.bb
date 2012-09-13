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
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_task_executor.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;

namespace file_handler_util {

// The prefix used to differentiate drive extensions from Chrome extensions.
const char FileTaskExecutor::kDriveTaskExtensionPrefix[] = "drive-app:";
const size_t FileTaskExecutor::kDriveTaskExtensionPrefixLength =
    arraysize(FileTaskExecutor::kDriveTaskExtensionPrefix) - 1;

namespace {
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
  for (Extension::FileBrowserHandlerList::const_iterator action_iter =
           extension->file_browser_handlers()->begin();
       action_iter != extension->file_browser_handlers()->end();
       ++action_iter) {
    if (action_iter->get()->id() == action_id)
      return action_iter->get();
  }
  return NULL;
}

unsigned int GetAccessPermissionsForHandler(const Extension* extension,
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
  ExtensionService* service = profile->GetExtensionService();
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
    if (!extension->file_browser_handlers())
      continue;

    for (Extension::FileBrowserHandlerList::const_iterator action_iter =
             extension->file_browser_handlers()->begin();
         action_iter != extension->file_browser_handlers()->end();
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
    if (!mime_task_prefs) {
      LOG(WARNING) << "Unable to open MIME type prefs";
      return std::string();
    }
    if (mime_task_prefs->GetStringWithoutPathExpansion(mime_type, &task_id)) {
      VLOG(1) << "Found MIME default handler: " << task_id;
      return task_id;
    }
  }

  const DictionaryValue* suffix_task_prefs =
      profile->GetPrefs()->GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  if (!suffix_task_prefs) {
    LOG(WARNING) << "Unable to open suffix prefs";
    return std::string();
  }
  std::string lower_suffix = StringToLowerASCII(suffix);
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
                       const std::string& action_id) {
  return base::StringPrintf("%s|%s", extension_id.c_str(), action_id.c_str());
}

std::string MakeDriveTaskID(const std::string& app_id,
                            const std::string& action_id) {
  return MakeTaskID(FileTaskExecutor::kDriveTaskExtensionPrefix + app_id,
                    action_id);
}

bool CrackDriveTaskID(const std::string& task_id,
                      std::string* app_id,
                      std::string* action_id) {
  std::string app_id_tmp;
  std::string action_id_tmp;
  if (!CrackTaskID(task_id, &app_id_tmp, &action_id_tmp))
    return false;
  if (StartsWithASCII(app_id_tmp,
                      FileTaskExecutor::kDriveTaskExtensionPrefix,
                      false)) {
    // Strip off the prefix from the extension ID so we convert it to an app id.
    if (app_id) {
      *app_id = app_id_tmp.substr(
          FileTaskExecutor::kDriveTaskExtensionPrefixLength);
    }
    if (action_id)
      *action_id = action_id_tmp;
    return true;
  }
  return false;
}

// Breaks down task_id that is used between getFileTasks() and executeTask() on
// its building blocks. task_id field the following structure:
//     <extension-id>|<task-action-id>
bool CrackTaskID(const std::string& task_id,
                 std::string* extension_id,
                 std::string* action_id) {
  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);
  if (count != 2)
    return false;
  if (extension_id)
    *extension_id = result[0];
  if (action_id)
    *action_id = result[1];
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

  // Convert the default task IDs collected above to one of the handler pointers
  // from common_tasks.
  for (FileBrowserHandlerSet::const_iterator task_iter = common_tasks.begin();
       task_iter != common_tasks.end(); ++task_iter) {
    std::string task_id = MakeTaskID((*task_iter)->extension_id(),
                                     (*task_iter)->id());
    for (std::set<std::string>::iterator default_iter = default_ids.begin();
         default_iter != default_ids.end(); ++default_iter) {
      if (task_id == *default_iter) {
        default_tasks->insert(*task_iter);
        break;
      }
    }
  }
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

  FindDefaultTasks(profile, file_urls, common_tasks, &default_tasks);

  // If there's none, or more than one, then we don't have a canonical default.
  if (!default_tasks.empty()) {
    // There should not be multiple default tasks for a single URL.
    DCHECK_EQ(1u, default_tasks.size());

    *handler = *default_tasks.begin();
    return true;
  }

  // If there are no default tasks set, try to match one of the builtin tasks
  // (excluding those that match all urls).
  for (FileBrowserHandlerSet::const_iterator it = common_tasks.begin();
       it != common_tasks.end();
       ++it) {
     if ((*it)->extension_id() == kFileBrowserDomain && !MatchesAllURLs(*it)) {
       *handler = *it;
       return true;
     }
   }

  return false;
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
                        const GURL source_url,
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
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
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
      const std::string& action_id,
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

  // Helper function to get the extension pointer.
  const extensions::Extension* GetExtension();

  const GURL source_url_;
  const std::string extension_id_;
  const std::string action_id_;
  FileTaskFinishedCallback done_;

  // (File path, permission for file path) pairs for the handler.
  std::vector<std::pair<FilePath, int> > handler_host_permissions_;
};

// static
FileTaskExecutor* FileTaskExecutor::Create(Profile* profile,
                                           const GURL source_url,
                                           const std::string& extension_id,
                                           const std::string& action_id) {
  // Check out the extension ID and see if this is a drive task,
  // and instantiate drive-specific executor if so.
  if (StartsWithASCII(extension_id,
                      FileTaskExecutor::kDriveTaskExtensionPrefix,
                      false)) {
    return new gdata::DriveTaskExecutor(profile,
                                        extension_id, // really app_id
                                        action_id);
  } else {
    return new ExtensionTaskExecutor(profile,
                                     source_url,
                                     extension_id,
                                     action_id);
  }
}

FileTaskExecutor::FileTaskExecutor(Profile* profile) : profile_(profile) {
}

FileTaskExecutor::~FileTaskExecutor() {
}

bool FileTaskExecutor::Execute(const std::vector<GURL>& file_urls) {
  return ExecuteAndNotify(file_urls, FileTaskFinishedCallback());
}

Browser* FileTaskExecutor::GetBrowser() const {
  return browser::FindOrCreateTabbedBrowser(
      profile_ ? profile_ : ProfileManager::GetDefaultProfileOrOffTheRecord());
}

ExtensionTaskExecutor::FileDefinition::FileDefinition() : is_directory(false) {
}

ExtensionTaskExecutor::FileDefinition::~FileDefinition() {
}

class ExtensionTaskExecutor::ExecuteTasksFileSystemCallbackDispatcher {
 public:
  static fileapi::FileSystemContext::OpenFileSystemCallback CreateCallback(
      ExtensionTaskExecutor* executor,
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      const GURL& source_url,
      scoped_refptr<const Extension> handler_extension,
      int handler_pid,
      const std::string& action_id,
      const std::vector<GURL>& file_urls) {
    return base::Bind(
        &ExecuteTasksFileSystemCallbackDispatcher::DidOpenFileSystem,
        base::Owned(new ExecuteTasksFileSystemCallbackDispatcher(
            executor, file_system_context, source_url, handler_extension,
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
      if (!SetupFileAccessPermissions(*iter, &file)) {
        continue;
      }
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
      scoped_refptr<fileapi::FileSystemContext> file_system_context,
      const GURL& source_url,
      const scoped_refptr<const Extension>& handler_extension,
      int handler_pid,
      const std::string& action_id,
      const std::vector<GURL>& file_urls)
      : executor_(executor),
        file_system_context_(file_system_context),
        source_url_(source_url),
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
    if (!chromeos::CrosMountPointProvider::CanHandleURL(url))
      return false;

    fileapi::ExternalFileSystemMountPointProvider* external_provider =
        file_system_context_->external_provider();
    if (!external_provider || !external_provider->IsAccessAllowed(url))
      return false;

    // Make sure this url really being used by the right caller extension.
    if (source_url_.GetOrigin() != url.origin()) {
      DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
      return false;
    }

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;

    FilePath local_path = url.path();
    FilePath virtual_path = url.virtual_path();

    bool is_drive_file = url.type() == fileapi::kFileSystemTypeDrive;
    DCHECK(!is_drive_file || gdata::util::IsUnderDriveMountPoint(local_path));

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
    external_provider->GrantFileAccessToExtension(handler_extension_->id(),
                                                  virtual_path);

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
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  // Extension source URL.
  GURL source_url_;
  scoped_refptr<const Extension> handler_extension_;
  int handler_pid_;
  std::string action_id_;
  std::vector<GURL> origin_file_urls_;
  DISALLOW_COPY_AND_ASSIGN(ExecuteTasksFileSystemCallbackDispatcher);
};

ExtensionTaskExecutor::ExtensionTaskExecutor(
    Profile* profile,
    const GURL source_url,
    const std::string& extension_id,
    const std::string& action_id)
  : FileTaskExecutor(profile),
    source_url_(source_url),
    extension_id_(extension_id),
    action_id_(action_id) {
}

ExtensionTaskExecutor::~ExtensionTaskExecutor() {}

bool ExtensionTaskExecutor::ExecuteAndNotify(
    const std::vector<GURL>& file_urls,
    const FileTaskFinishedCallback& done) {
  ExtensionService* service = profile()->GetExtensionService();
  if (!service)
    return false;

  scoped_refptr<const Extension> handler =
      service->GetExtensionById(extension_id_, false);

  if (!handler.get())
    return false;

  int handler_pid = ExtractProcessFromExtensionId(handler->id(), profile());
  if (handler_pid <= 0) {
    if (!handler->has_lazy_background_page())
      return false;
  }

  done_ = done;

  // Get local file system instance on file thread.
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetFileSystemContext(profile());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ExtensionTaskExecutor::RequestFileEntryOnFileThread,
          this,
          file_system_context,
          Extension::GetBaseURLFromExtensionId(handler->id()),
          handler,
          handler_pid,
          file_urls));
  return true;
}

void ExtensionTaskExecutor::RequestFileEntryOnFileThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& handler_base_url,
    const scoped_refptr<const Extension>& handler,
    int handler_pid,
    const std::vector<GURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  GURL origin_url = handler_base_url.GetOrigin();
  file_system_context->OpenFileSystem(
      origin_url, fileapi::kFileSystemTypeExternal, false, // create
      ExecuteTasksFileSystemCallbackDispatcher::CreateCallback(
          this,
          file_system_context,
          source_url_,
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

const Extension* ExtensionTaskExecutor::GetExtension() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* service = profile()->GetExtensionService();
  return service ? service->GetExtensionById(extension_id_, false) :
                   NULL;
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
      action_id_,
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
        profile(), extension_id_,
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

  extensions::EventRouter* event_router = profile()->GetExtensionEventRouter();
  if (!event_router) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  SetupHandlerHostFileAccessPermissions(handler_pid);

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(Value::CreateStringValue(action_id_));
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

  // Get tab id.
  Browser* current_browser = GetBrowser();
  if (current_browser) {
    WebContents* contents = chrome::GetActiveWebContents(current_browser);
    if (contents)
      details->SetInteger("tab_id", ExtensionTabUtil::GetTabId(contents));
  }

  event_router->DispatchEventToExtension(
      extension_id_, std::string("fileBrowserHandler.onExecute"),
      event_args.Pass(), profile(), GURL());
  ExecuteDoneOnUIThread(true);
}

void ExtensionTaskExecutor::InitHandlerHostFileAccessPermissions(
    const FileDefinitionList& file_list,
    const Extension* handler_extension,
    const std::string& action_id,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<std::vector<FilePath> > gdata_paths(new std::vector<FilePath>);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    // Setup permission for file's absolute file.
    handler_host_permissions_.push_back(std::make_pair(
        iter->absolute_path,
        GetAccessPermissionsForHandler(handler_extension, action_id)));

    if (gdata::util::IsUnderDriveMountPoint(iter->absolute_path))
      gdata_paths->push_back(iter->virtual_path);
  }

  if (gdata_paths->empty()) {
    // Invoke callback if none of the files are on gdata mount point.
    callback.Run();
    return;
  }

  // For files on gdata mount point, we'll have to give handler host permissions
  // for their cache paths. This has to be called on UI thread.
  gdata::util::InsertDriveCachePathsPermissions(profile(),
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

} // namespace file_handler_util
