// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/file_task_executor.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
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

namespace file_manager {
namespace file_tasks {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
//
// TODO(satorux): We should rename them to "file_browser_handler" and
// "file_handler" respectively when switching from preferences to
// chrome.storage crbug.com/267359
const char kFileBrowserHandlerTaskType[] = "file";
const char kFileHandlerTaskType[] = "app";
const char kDriveTaskType[] = "drive";

namespace {

// Legacy Drive task extension prefix, used by CrackTaskID.
const char kDriveTaskExtensionPrefix[] = "drive-app:";
const size_t kDriveTaskExtensionPrefixLength =
    arraysize(kDriveTaskExtensionPrefix) - 1;

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

// Finds a file browser handler that matches |action_id|. Returns NULL if not
// found.
const FileBrowserHandler* FindFileBrowserHandlerForActionId(
    const Extension* extension,
    const std::string& action_id) {
  FileBrowserHandler::List* handler_list =
      FileBrowserHandler::GetHandlers(extension);
  for (FileBrowserHandler::List::const_iterator handler_iter =
           handler_list->begin();
       handler_iter != handler_list->end();
       ++handler_iter) {
    if (handler_iter->get()->id() == action_id)
      return handler_iter->get();
  }
  return NULL;
}

std::string EscapedUtf8ToLower(const std::string& str) {
  string16 utf16 = UTF8ToUTF16(
      net::UnescapeURLComponent(str, net::UnescapeRule::NORMAL));
  return net::EscapeUrlEncodedData(
      UTF16ToUTF8(base::i18n::ToLower(utf16)),
      false /* do not replace space with plus */);
}

// Finds file browser handlers that can handle the |selected_file_url|.
FileBrowserHandlerList FindFileBrowserHandlersForURL(
    Profile* profile,
    const GURL& selected_file_url) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  // In unit-tests, we may not have an ExtensionService.
  if (!service)
    return FileBrowserHandlerList();

  // We need case-insensitive matching, and pattern in the handler is already
  // in lower case.
  const GURL lowercase_url(EscapedUtf8ToLower(selected_file_url.spec()));

  FileBrowserHandlerList results;
  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();
    if (profile->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    FileBrowserHandler::List* handler_list =
        FileBrowserHandler::GetHandlers(extension);
    if (!handler_list)
      continue;
    for (FileBrowserHandler::List::const_iterator handler_iter =
             handler_list->begin();
         handler_iter != handler_list->end();
         ++handler_iter) {
      const FileBrowserHandler* handler = handler_iter->get();
      if (!handler->MatchesURL(lowercase_url))
        continue;

      results.push_back(handler_iter->get());
    }
  }
  return results;
}

fileapi::FileSystemContext* GetFileSystemContextForExtension(
    Profile* profile,
    const std::string& extension_id) {
  GURL site = extensions::ExtensionSystem::Get(profile)->
      extension_service()->GetSiteForExtensionId(extension_id);
  return BrowserContext::GetStoragePartitionForSite(profile, site)->
      GetFileSystemContext();
}

// Checks if the file browser extension has permissions for the files in its
// file system context.
bool FileBrowserHasAccessPermissionForFiles(
    Profile* profile,
    const GURL& source_url,
    const std::string& file_browser_id,
    const std::vector<FileSystemURL>& files) {
  fileapi::ExternalFileSystemBackend* backend =
      GetFileSystemContextForExtension(profile, file_browser_id)->
      external_backend();
  if (!backend)
    return false;

  for (size_t i = 0; i < files.size(); ++i) {
    // Make sure this url really being used by the right caller extension.
    if (source_url.GetOrigin() != files[i].origin())
      return false;

    if (!chromeos::FileSystemBackend::CanHandleURL(files[i]) ||
        !backend->IsAccessAllowed(files[i])) {
      return false;
    }
  }

  return true;
}

// Finds a file browser handler that matches |extension_id| and |action_id|
// from |handler_list|.  Returns a mutable iterator to the handler if
// found. Returns handler_list->end() if not found.
FileBrowserHandlerList::iterator
FindFileBrowserHandlerForExtensionIdAndActionId(
    FileBrowserHandlerList* handler_list,
    const std::string& extension_id,
    const std::string& action_id) {
  DCHECK(handler_list);

  FileBrowserHandlerList::iterator iter = handler_list->begin();
  while (iter != handler_list->end() &&
         !((*iter)->extension_id() == extension_id &&
           (*iter)->id() == action_id)) {
    ++iter;
  }
  return iter;
}

// This class is used to execute a file browser handler task. Here's how this
// works:
//
// 1) Open the "external" file system
// 2) Set up permissions for the target files on the external file system.
// 3) Raise onExecute event with the action ID and entries of the target
//    files. The event will launch the file browser handler if not active.
// 4) In the file browser handler, onExecute event is handled and executes the
//    task in JavaScript.
//
// That said, the class itself does not execute a task. The task will be
// executed in JavaScript.
class FileBrowserHandlerExecutor {
 public:
  FileBrowserHandlerExecutor(Profile* profile,
                             const Extension* extension,
                             int32 tab_id,
                             const std::string& action_id);

  // Executes the task for each file. |done| will be run with the result.
  void Execute(const std::vector<FileSystemURL>& file_urls,
               const FileTaskFinishedCallback& done);

 private:
  // This object is responsible to delete itself.
  virtual ~FileBrowserHandlerExecutor();

  struct FileDefinition {
    FileDefinition();
    ~FileDefinition();

    base::FilePath virtual_path;
    base::FilePath absolute_path;
    bool is_directory;
  };

  typedef std::vector<FileDefinition> FileDefinitionList;

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  static FileDefinitionList SetupFileAccessPermissions(
      scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
      const scoped_refptr<const Extension>& handler_extension,
      const std::vector<FileSystemURL>& file_urls);

  void DidOpenFileSystem(const std::vector<FileSystemURL>& file_urls,
                         base::PlatformFileError result,
                         const std::string& file_system_name,
                         const GURL& file_system_root);

  void ExecuteDoneOnUIThread(bool success);
  void ExecuteFileActionsOnUIThread(const std::string& file_system_name,
                                    const GURL& file_system_root,
                                    const FileDefinitionList& file_list);
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
  base::WeakPtrFactory<FileBrowserHandlerExecutor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserHandlerExecutor);
};

FileBrowserHandlerExecutor::FileDefinition::FileDefinition()
    : is_directory(false) {
}

FileBrowserHandlerExecutor::FileDefinition::~FileDefinition() {
}

// static
FileBrowserHandlerExecutor::FileDefinitionList
FileBrowserHandlerExecutor::SetupFileAccessPermissions(
    scoped_refptr<fileapi::FileSystemContext> file_system_context_handler,
    const scoped_refptr<const Extension>& handler_extension,
    const std::vector<FileSystemURL>& file_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(handler_extension.get());

  fileapi::ExternalFileSystemBackend* backend =
      file_system_context_handler->external_backend();

  FileDefinitionList file_list;
  for (size_t i = 0; i < file_urls.size(); ++i) {
    const FileSystemURL& url = file_urls[i];

    // Check if this file system entry exists first.
    base::PlatformFileInfo file_info;

    base::FilePath local_path = url.path();
    base::FilePath virtual_path = url.virtual_path();

    bool is_drive_file = url.type() == fileapi::kFileSystemTypeDrive;
    DCHECK(!is_drive_file || drive::util::IsUnderDriveMountPoint(local_path));

    // If the file is under drive mount point, there is no actual file to be
    // found on the url.path().
    if (!is_drive_file) {
      if (!base::PathExists(local_path) ||
          file_util::IsLink(local_path) ||
          !file_util::GetFileInfo(local_path, &file_info)) {
        continue;
      }
    }

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    backend->GrantFileAccessToExtension(
        handler_extension->id(), virtual_path);

    // Output values.
    FileDefinition file;
    file.virtual_path = virtual_path;
    file.is_directory = file_info.is_directory;
    file.absolute_path = local_path;
    file_list.push_back(file);
  }
  return file_list;
}

FileBrowserHandlerExecutor::FileBrowserHandlerExecutor(
    Profile* profile,
    const Extension* extension,
    int32 tab_id,
    const std::string& action_id)
    : profile_(profile),
      extension_(extension),
      tab_id_(tab_id),
      action_id_(action_id),
      weak_ptr_factory_(this) {
}

FileBrowserHandlerExecutor::~FileBrowserHandlerExecutor() {}

void FileBrowserHandlerExecutor::Execute(
    const std::vector<FileSystemURL>& file_urls,
    const FileTaskFinishedCallback& done) {
  done_ = done;

  // Get file system context for the extension to which onExecute event will be
  // sent. The file access permissions will be granted to the extension in the
  // file system context for the files in |file_urls|.
  GetFileSystemContextForExtension(profile_, extension_->id())->OpenFileSystem(
      Extension::GetBaseURLFromExtensionId(extension_->id()).GetOrigin(),
      fileapi::kFileSystemTypeExternal,
      fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::Bind(&FileBrowserHandlerExecutor::DidOpenFileSystem,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_urls));
}

void FileBrowserHandlerExecutor::DidOpenFileSystem(
    const std::vector<FileSystemURL>& file_urls,
    base::PlatformFileError result,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  if (result != base::PLATFORM_FILE_OK) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context(
      GetFileSystemContextForExtension(profile_, extension_->id()));
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SetupFileAccessPermissions,
                 file_system_context,
                 extension_,
                 file_urls),
      base::Bind(&FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_system_name,
                 file_system_root));
}

void FileBrowserHandlerExecutor::ExecuteDoneOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!done_.is_null())
    done_.Run(success);
  delete this;
}

void FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread(
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FileDefinitionList& file_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (file_list.empty()) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  int handler_pid = ExtractProcessFromExtensionId(profile_, extension_->id());
  if (handler_pid <= 0 &&
      !extensions::BackgroundInfo::HasLazyBackgroundPage(extension_.get())) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  if (handler_pid > 0) {
    SetupPermissionsAndDispatchEvent(file_system_name, file_system_root,
                                     file_list, handler_pid, NULL);
  } else {
    // We have to wake the handler background page before we proceed.
    extensions::LazyBackgroundTaskQueue* queue =
        extensions::ExtensionSystem::Get(profile_)->
        lazy_background_task_queue();
    if (!queue->ShouldEnqueueTask(profile_, extension_.get())) {
      ExecuteDoneOnUIThread(false);
      return;
    }
    queue->AddPendingTask(
        profile_, extension_->id(),
        base::Bind(
            &FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent,
            weak_ptr_factory_.GetWeakPtr(),
            file_system_name,
            file_system_root,
            file_list,
            handler_pid));
  }
}

void FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent(
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

  SetupHandlerHostFileAccessPermissions(
      file_list, extension_.get(), handler_pid);

  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(new base::StringValue(action_id_));
  DictionaryValue* details = new DictionaryValue();
  event_args->Append(details);
  // Get file definitions. These will be replaced with Entry instances by
  // dispatchEvent() method from event_binding.js.
  ListValue* file_entries = new ListValue();
  details->Set("entries", file_entries);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    DictionaryValue* file_def = new DictionaryValue();
    file_entries->Append(file_def);
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

void FileBrowserHandlerExecutor::SetupHandlerHostFileAccessPermissions(
    const FileDefinitionList& file_list,
    const Extension* extension,
    int handler_pid) {
  const FileBrowserHandler* action = FindFileBrowserHandlerForActionId(
      extension_, action_id_);
  for (FileDefinitionList::const_iterator iter = file_list.begin();
       iter != file_list.end();
       ++iter) {
    if (!action)
      continue;
    if (action->CanRead()) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
          handler_pid, iter->absolute_path);
    }
    if (action->CanWrite()) {
      content::ChildProcessSecurityPolicy::GetInstance()->
          GrantCreateReadWriteFile(handler_pid, iter->absolute_path);
    }
  }
}

// Executes a file browser handler specified by |extension| of the given
// action ID for |file_urls|. Returns false if undeclared handlers are
// found. |done| is on completion. See also the comment at ExecuteFileTask()
// for other parameters.
bool ExecuteFileBrowserHandler(Profile* profile,
                               const Extension* extension,
                               int32 tab_id,
                               const std::string& action_id,
                               const std::vector<FileSystemURL>& file_urls,
                               const FileTaskFinishedCallback& done) {
  // Forbid calling undeclared handlers.
  if (!FindFileBrowserHandlerForActionId(extension, action_id))
    return false;

  // The executor object will be self deleted on completion.
  (new FileBrowserHandlerExecutor(
      profile, extension, tab_id, action_id))->Execute(file_urls, done);
  return true;
}

}  // namespace

bool IsFallbackFileBrowserHandler(const FileBrowserHandler* handler) {
  const std::string& extension_id = handler->extension_id();
  return (extension_id == kFileBrowserDomain ||
          extension_id == extension_misc::kQuickOfficeComponentExtensionId ||
          extension_id == extension_misc::kQuickOfficeDevExtensionId ||
          extension_id == extension_misc::kQuickOfficeExtensionId);
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

std::string MakeTaskID(const std::string& extension_id,
                       const std::string& task_type,
                       const std::string& action_id) {
  DCHECK(task_type == kFileBrowserHandlerTaskType ||
         task_type == kDriveTaskType ||
         task_type == kFileHandlerTaskType);
  return base::StringPrintf("%s|%s|%s",
                            extension_id.c_str(),
                            task_type.c_str(),
                            action_id.c_str());
}

bool CrackTaskID(const std::string& task_id,
                 std::string* extension_id,
                 std::string* task_type,
                 std::string* action_id) {
  std::vector<std::string> result;
  int count = Tokenize(task_id, std::string("|"), &result);

  // Parse a legacy task ID that only contain two parts. Drive tasks are
  // identified by a prefix "drive-app:" on the extension ID. The legacy task
  // IDs can be stored in preferences.
  // TODO(satorux): We should get rid of this code: crbug.com/267359.
  if (count == 2) {
    if (StartsWithASCII(result[0], kDriveTaskExtensionPrefix, true)) {
      if (task_type)
        *task_type = kDriveTaskType;

      if (extension_id)
        *extension_id = result[0].substr(kDriveTaskExtensionPrefixLength);
    } else {
      if (task_type)
        *task_type = kFileBrowserHandlerTaskType;

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
    DCHECK(*task_type == kFileBrowserHandlerTaskType ||
           *task_type == kDriveTaskType ||
           *task_type == kFileHandlerTaskType);
  }

  if (action_id)
    *action_id = result[2];

  return true;
}

FileBrowserHandlerList FindDefaultFileBrowserHandlers(
    Profile* profile,
    const std::vector<base::FilePath>& file_list,
    const FileBrowserHandlerList& common_handlers) {
  FileBrowserHandlerList default_handlers;

  std::set<std::string> default_ids;
  for (std::vector<base::FilePath>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    std::string task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        profile, "", it->Extension());
    if (!task_id.empty())
      default_ids.insert(task_id);
  }

  const FileBrowserHandler* fallback_handler = NULL;
  // Convert the default task IDs collected above to one of the handler pointers
  // from common_handlers.
  for (size_t i = 0; i < common_handlers.size(); ++i) {
    const FileBrowserHandler* handler = common_handlers[i];
    std::string task_id = MakeTaskID(handler->extension_id(),
                                     kFileBrowserHandlerTaskType,
                                     handler->id());
    std::set<std::string>::iterator default_iter = default_ids.find(task_id);
    if (default_iter != default_ids.end()) {
      default_handlers.push_back(handler);
      continue;
    }

    // Remember the first fallback handler.
    if (!fallback_handler && IsFallbackFileBrowserHandler(handler))
      fallback_handler = handler;
  }

  // If there are no default handlers found, use fallback as default.
  if (fallback_handler && default_handlers.empty())
    default_handlers.push_back(fallback_handler);

  return default_handlers;
}

FileBrowserHandlerList FindCommonFileBrowserHandlers(
    Profile* profile,
    const std::vector<GURL>& file_list) {
  FileBrowserHandlerList common_handlers;
  for (std::vector<GURL>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    FileBrowserHandlerList handlers =
        FindFileBrowserHandlersForURL(profile, *it);
    // If there is nothing to do for one file, the intersection of handlers
    // for all files will be empty at the end, so no need to check further.
    if (handlers.empty())
      return FileBrowserHandlerList();

    // For the very first file, just copy all the elements.
    if (it == file_list.begin()) {
      common_handlers = handlers;
    } else {
      // For all additional files, find intersection between the accumulated and
      // file specific set.
      FileBrowserHandlerList intersection;
      std::set_intersection(common_handlers.begin(), common_handlers.end(),
                            handlers.begin(), handlers.end(),
                            std::back_inserter(intersection));
      common_handlers = intersection;
      if (common_handlers.empty())
        return FileBrowserHandlerList();
    }
  }

  FileBrowserHandlerList::iterator watch_iter =
      FindFileBrowserHandlerForExtensionIdAndActionId(
          &common_handlers, kFileBrowserDomain, kFileBrowserWatchTaskId);
  FileBrowserHandlerList::iterator gallery_iter =
      FindFileBrowserHandlerForExtensionIdAndActionId(
          &common_handlers, kFileBrowserDomain, kFileBrowserGalleryTaskId);
  if (watch_iter != common_handlers.end() &&
      gallery_iter != common_handlers.end()) {
    // Both "watch" and "gallery" actions are applicable which means that the
    // selection is all videos. Showing them both is confusing, so we only keep
    // the one that makes more sense ("watch" for single selection, "gallery"
    // for multiple selection).
    if (file_list.size() == 1)
      common_handlers.erase(gallery_iter);
    else
      common_handlers.erase(watch_iter);
  }

  return common_handlers;
}

const FileBrowserHandler* FindFileBrowserHandlerForURLAndPath(
    Profile* profile,
    const GURL& url,
    const base::FilePath& file_path) {
  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  FileBrowserHandlerList common_handlers =
      FindCommonFileBrowserHandlers(profile, file_urls);
  if (common_handlers.empty())
    return NULL;

  std::vector<base::FilePath> file_paths;
  file_paths.push_back(file_path);

  FileBrowserHandlerList default_handlers =
      FindDefaultFileBrowserHandlers(profile, file_paths, common_handlers);

  // If there's none, or more than one, then we don't have a canonical default.
  if (!default_handlers.empty()) {
    // There should not be multiple default handlers for a single URL.
    DCHECK_EQ(1u, default_handlers.size());

    return *default_handlers.begin();
  }

  // If there are no default handlers, use first handler in the list (file
  // manager does the same in this situation).  TODO(tbarzic): This is not so
  // optimal behaviour.
  return *common_handlers.begin();
}

bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const std::string& file_browser_id,
                     int32 tab_id,
                     const std::string& app_id,
                     const std::string& task_type,
                     const std::string& action_id,
                     const std::vector<FileSystemURL>& file_urls,
                     const FileTaskFinishedCallback& done) {
  if (!FileBrowserHasAccessPermissionForFiles(profile, source_url,
                                              file_browser_id, file_urls))
    return false;

  // drive::FileTaskExecutor is responsible to handle drive tasks.
  if (task_type == kDriveTaskType) {
    DCHECK_EQ("open-with", action_id);
    drive::FileTaskExecutor* executor =
        new drive::FileTaskExecutor(profile, app_id);
    executor->Execute(file_urls, done);
    return true;
  }

  // Get the extension.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const Extension* extension = service ?
      service->GetExtensionById(app_id, false) : NULL;
  if (!extension)
    return false;

  // Execute the task.
  if (task_type == kFileBrowserHandlerTaskType) {
    return ExecuteFileBrowserHandler(profile,
                                     extension,
                                     tab_id,
                                     action_id,
                                     file_urls,
                                     done);
  } else if (task_type == kFileHandlerTaskType) {
    for (size_t i = 0; i != file_urls.size(); ++i) {
      apps::LaunchPlatformAppWithFileHandler(
          profile, extension, action_id, file_urls[i].path());
    }

    if (!done.is_null())
      done.Run(true);
    return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace file_tasks
}  // namespace file_manager
