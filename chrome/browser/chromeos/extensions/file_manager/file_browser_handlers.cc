// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handlers.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/app_id.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/open_with_browser.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using fileapi::FileSystemURL;

namespace file_manager {
namespace file_browser_handlers {
namespace {

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
               const file_tasks::FileTaskFinishedCallback& done);

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
  file_tasks::FileTaskFinishedCallback done_;
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
    const file_tasks::FileTaskFinishedCallback& done) {
  done_ = done;

  // Get file system context for the extension to which onExecute event will be
  // sent. The file access permissions will be granted to the extension in the
  // file system context for the files in |file_urls|.
  util::GetFileSystemContextForExtensionId(
      profile_, extension_->id())->OpenFileSystem(
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
      util::GetFileSystemContextForExtensionId(
          profile_, extension_->id()));
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

// Returns true if |extension_id| and |action_id| indicate that the file
// currently being handled should be opened with the browser. This function
// is used to handle certain action IDs of the file manager.
bool ShouldBeOpenedWithBrowser(const std::string& extension_id,
                               const std::string& action_id) {

  return (extension_id == kFileManagerAppId &&
          (action_id == "view-pdf" ||
           action_id == "view-swf" ||
           action_id == "view-in-browser" ||
           action_id == "install-crx" ||
           action_id == "open-hosted-generic" ||
           action_id == "open-hosted-gdoc" ||
           action_id == "open-hosted-gsheet" ||
           action_id == "open-hosted-gslides"));
}

// Opens the files specified by |file_urls| with the browser for |profile|.
// Returns true on success. It's a failure if no files are opened.
bool OpenFilesWithBrowser(Profile* profile,
                          const std::vector<FileSystemURL>& file_urls) {
  int num_opened = 0;
  for (size_t i = 0; i < file_urls.size(); ++i) {
    const FileSystemURL& file_url = file_urls[i];
    if (chromeos::FileSystemBackend::CanHandleURL(file_url)) {
      const base::FilePath& file_path = file_url.path();
      num_opened += util::OpenFileWithBrowser(profile, file_path);
    }
  }
  return num_opened > 0;
}

}  // namespace

bool ExecuteFileBrowserHandler(
    Profile* profile,
    const Extension* extension,
    int32 tab_id,
    const std::string& action_id,
    const std::vector<FileSystemURL>& file_urls,
    const file_tasks::FileTaskFinishedCallback& done) {
  // Forbid calling undeclared handlers.
  if (!FindFileBrowserHandlerForActionId(extension, action_id))
    return false;

  // Some action IDs of the file manager's file browser handlers require the
  // files to be directly opened with the browser.
  if (ShouldBeOpenedWithBrowser(extension->id(), action_id)) {
    return OpenFilesWithBrowser(profile, file_urls);
  }

  // The executor object will be self deleted on completion.
  (new FileBrowserHandlerExecutor(
      profile, extension, tab_id, action_id))->Execute(file_urls, done);
  return true;
}

bool IsFallbackFileBrowserHandler(const file_tasks::TaskDescriptor& task) {
  return (task.task_type == file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER &&
          (task.app_id == kFileManagerAppId ||
           task.app_id == extension_misc::kQuickOfficeComponentExtensionId ||
           task.app_id == extension_misc::kQuickOfficeDevExtensionId ||
           task.app_id == extension_misc::kQuickOfficeExtensionId));
}

FileBrowserHandlerList FindDefaultFileBrowserHandlers(
    const PrefService& pref_service,
    const std::vector<base::FilePath>& file_list,
    const FileBrowserHandlerList& common_handlers) {
  FileBrowserHandlerList default_handlers;

  std::set<std::string> default_ids;
  for (std::vector<base::FilePath>::const_iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    std::string task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        pref_service, "", it->Extension());
    if (!task_id.empty())
      default_ids.insert(task_id);
  }

  const FileBrowserHandler* fallback_handler = NULL;
  // Convert the default task IDs collected above to one of the handler pointers
  // from common_handlers.
  for (size_t i = 0; i < common_handlers.size(); ++i) {
    const FileBrowserHandler* handler = common_handlers[i];
    const file_tasks::TaskDescriptor task_descriptor(
        handler->extension_id(),
        file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
        handler->id());
    const std::string task_id =
        file_tasks::TaskDescriptorToId(task_descriptor);
    std::set<std::string>::iterator default_iter = default_ids.find(task_id);
    if (default_iter != default_ids.end()) {
      default_handlers.push_back(handler);
      continue;
    }

    // Remember the first fallback handler.
    if (!fallback_handler && IsFallbackFileBrowserHandler(task_descriptor))
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

  // "watch" and "gallery" are defined in the file manager's manifest.json.
  FileBrowserHandlerList::iterator watch_iter =
      FindFileBrowserHandlerForExtensionIdAndActionId(
          &common_handlers, kFileManagerAppId, "watch");
  FileBrowserHandlerList::iterator gallery_iter =
      FindFileBrowserHandlerForExtensionIdAndActionId(
          &common_handlers, kFileManagerAppId, "gallery");
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
      FindDefaultFileBrowserHandlers(*profile->GetPrefs(),
                                     file_paths,
                                     common_handlers);

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

}  // namespace file_browser_handlers
}  // namespace file_manager
