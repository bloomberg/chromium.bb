// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_browser_handlers.h"

#include <stddef.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_with_browser.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_util.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using storage::FileSystemURL;
using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionList;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;

namespace file_manager {
namespace file_browser_handlers {
namespace {

// Returns process id of the process the extension is running in.
int ExtractProcessFromExtensionId(Profile* profile,
                                  const std::string& extension_id) {
  GURL extension_url =
      Extension::GetBaseURLFromExtensionId(extension_id);
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(profile);

  scoped_refptr<SiteInstance> site_instance =
      manager->GetSiteInstanceForURL(extension_url);
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
  base::string16 utf16 = base::UTF8ToUTF16(
      net::UnescapeURLComponent(str, net::UnescapeRule::NORMAL));
  return net::EscapeUrlEncodedData(
      base::UTF16ToUTF8(base::i18n::ToLower(utf16)),
      false /* do not replace space with plus */);
}

// Finds file browser handlers that can handle the |selected_file_url|.
FileBrowserHandlerList FindFileBrowserHandlersForURL(
    Profile* profile,
    const GURL& selected_file_url) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  // In unit-tests, we may not have an ExtensionRegistry.
  if (!registry)
    return FileBrowserHandlerList();

  // We need case-insensitive matching, and pattern in the handler is already
  // in lower case.
  const GURL lowercase_url(EscapedUtf8ToLower(selected_file_url.spec()));

  FileBrowserHandlerList results;
  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    if (profile->IsOffTheRecord() &&
        !extensions::util::IsIncognitoEnabled(extension->id(), profile))
      continue;
    FileBrowserHandler::List* handler_list =
        FileBrowserHandler::GetHandlers(extension.get());
    if (!handler_list)
      continue;
    for (FileBrowserHandler::List::const_iterator handler_iter =
             handler_list->begin();
         handler_iter != handler_list->end();
         ++handler_iter) {
      const FileBrowserHandler* handler = handler_iter->get();
      if (!handler->MatchesURL(lowercase_url))
        continue;
      // Filter out Files app from handling ZIP files via a handler, as it's
      // now handled by new ZIP unpacker extension based on File System Provider
      // API.
      const URLPattern zip_pattern(URLPattern::SCHEME_EXTENSION,
                                   "chrome-extension://*/*.zip");
      if (handler->extension_id() == kFileManagerAppId &&
          zip_pattern.MatchesURL(selected_file_url) &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              chromeos::switches::kDisableNewZIPUnpacker)) {
        continue;
      }
      results.push_back(handler);
    }
  }
  return results;
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
                             const std::string& action_id);

  // Executes the task for each file. |done| will be run with the result.
  void Execute(const std::vector<FileSystemURL>& file_urls,
               const file_tasks::FileTaskFinishedCallback& done);

 private:
  // This object is responsible to delete itself.
  virtual ~FileBrowserHandlerExecutor();

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  static std::unique_ptr<FileDefinitionList> SetupFileAccessPermissions(
      scoped_refptr<storage::FileSystemContext> file_system_context_handler,
      const scoped_refptr<const Extension>& handler_extension,
      const std::vector<FileSystemURL>& file_urls);

  void ExecuteDoneOnUIThread(bool success);
  void ExecuteAfterSetupFileAccess(
      std::unique_ptr<FileDefinitionList> file_list);
  void ExecuteFileActionsOnUIThread(
      std::unique_ptr<FileDefinitionList> file_definition_list,
      std::unique_ptr<EntryDefinitionList> entry_definition_list);
  void SetupPermissionsAndDispatchEvent(
      std::unique_ptr<FileDefinitionList> file_definition_list,
      std::unique_ptr<EntryDefinitionList> entry_definition_list,
      int handler_pid_in,
      extensions::ExtensionHost* host);

  // Registers file permissions from |handler_host_permissions_| with
  // ChildProcessSecurityPolicy for process with id |handler_pid|.
  void SetupHandlerHostFileAccessPermissions(
      FileDefinitionList* file_definition_list,
      const Extension* extension,
      int handler_pid);

  Profile* profile_;
  scoped_refptr<const Extension> extension_;
  const std::string action_id_;
  file_tasks::FileTaskFinishedCallback done_;
  base::WeakPtrFactory<FileBrowserHandlerExecutor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserHandlerExecutor);
};

// static
std::unique_ptr<FileDefinitionList>
FileBrowserHandlerExecutor::SetupFileAccessPermissions(
    scoped_refptr<storage::FileSystemContext> file_system_context_handler,
    const scoped_refptr<const Extension>& handler_extension,
    const std::vector<FileSystemURL>& file_urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(handler_extension.get());

  storage::ExternalFileSystemBackend* backend =
      file_system_context_handler->external_backend();

  std::unique_ptr<FileDefinitionList> file_definition_list(
      new FileDefinitionList);
  for (size_t i = 0; i < file_urls.size(); ++i) {
    const FileSystemURL& url = file_urls[i];

    // Check if this file system entry exists first.
    base::File::Info file_info;

    base::FilePath local_path = url.path();
    base::FilePath virtual_path = url.virtual_path();

    const bool is_drive_file = url.type() == storage::kFileSystemTypeDrive;
    DCHECK(!is_drive_file || drive::util::IsUnderDriveMountPoint(local_path));

    const bool is_native_file =
        url.type() == storage::kFileSystemTypeNativeLocal ||
        url.type() == storage::kFileSystemTypeRestrictedNativeLocal;

    // If the file is from a physical volume, actual file must be found.
    if (is_native_file) {
      if (!base::PathExists(local_path) ||
          base::IsLink(local_path) ||
          !base::GetFileInfo(local_path, &file_info)) {
        continue;
      }
    }

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    backend->GrantFileAccessToExtension(handler_extension->id(), virtual_path);

    // Output values.
    FileDefinition file_definition;
    file_definition.virtual_path = virtual_path;
    file_definition.is_directory = file_info.is_directory;
    file_definition.absolute_path = local_path;
    file_definition_list->push_back(file_definition);
  }

  return file_definition_list;
}

FileBrowserHandlerExecutor::FileBrowserHandlerExecutor(
    Profile* profile,
    const Extension* extension,
    const std::string& action_id)
    : profile_(profile),
      extension_(extension),
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
  scoped_refptr<storage::FileSystemContext> file_system_context(
      util::GetFileSystemContextForExtensionId(profile_, extension_->id()));

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SetupFileAccessPermissions,
                 file_system_context,
                 extension_,
                 file_urls),
      base::Bind(&FileBrowserHandlerExecutor::ExecuteAfterSetupFileAccess,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FileBrowserHandlerExecutor::ExecuteAfterSetupFileAccess(
    std::unique_ptr<FileDefinitionList> file_definition_list) {
  // Outlives the conversion process, since bound to the callback.
  const FileDefinitionList& file_definition_list_ref =
      *file_definition_list.get();
  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      profile_,
      extension_->id(),
      file_definition_list_ref,
      base::Bind(&FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&file_definition_list)));
}

void FileBrowserHandlerExecutor::ExecuteDoneOnUIThread(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!done_.is_null())
    done_.Run(
        success
            ? extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT
            : extensions::api::file_manager_private::TASK_RESULT_FAILED);
  delete this;
}

void FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread(
    std::unique_ptr<FileDefinitionList> file_definition_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (file_definition_list->empty() || entry_definition_list->empty()) {
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
    SetupPermissionsAndDispatchEvent(std::move(file_definition_list),
                                     std::move(entry_definition_list),
                                     handler_pid, NULL);
  } else {
    // We have to wake the handler background page before we proceed.
    extensions::LazyBackgroundTaskQueue* queue =
        extensions::LazyBackgroundTaskQueue::Get(profile_);
    if (!queue->ShouldEnqueueTask(profile_, extension_.get())) {
      ExecuteDoneOnUIThread(false);
      return;
    }
    queue->AddPendingTask(
        profile_, extension_->id(),
        base::Bind(
            &FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(std::move(file_definition_list)),
            base::Passed(std::move(entry_definition_list)), handler_pid));
  }
}

void FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent(
    std::unique_ptr<FileDefinitionList> file_definition_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list,
    int handler_pid_in,
    extensions::ExtensionHost* host) {
  int handler_pid = host ? host->render_process_host()->GetID() :
      handler_pid_in;

  if (handler_pid <= 0) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  extensions::EventRouter* router = extensions::EventRouter::Get(profile_);
  if (!router) {
    ExecuteDoneOnUIThread(false);
    return;
  }

  SetupHandlerHostFileAccessPermissions(
      file_definition_list.get(), extension_.get(), handler_pid);

  std::unique_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->AppendString(action_id_);
  auto details = base::MakeUnique<base::DictionaryValue>();
  // Get file definitions. These will be replaced with Entry instances by
  // dispatchEvent() method from event_binding.js.
  auto file_entries = base::MakeUnique<base::ListValue>();

  for (EntryDefinitionList::const_iterator iter =
           entry_definition_list->begin();
       iter != entry_definition_list->end();
       ++iter) {
    auto file_def = base::MakeUnique<base::DictionaryValue>();
    file_def->SetString("fileSystemName", iter->file_system_name);
    file_def->SetString("fileSystemRoot", iter->file_system_root_url);
    file_def->SetString("fileFullPath",
                        "/" + iter->full_path.AsUTF8Unsafe());
    file_def->SetBoolean("fileIsDirectory", iter->is_directory);
    file_entries->Append(std::move(file_def));
  }

  details->Set("entries", std::move(file_entries));
  event_args->Append(std::move(details));
  auto event = base::MakeUnique<extensions::Event>(
      extensions::events::FILE_BROWSER_HANDLER_ON_EXECUTE,
      "fileBrowserHandler.onExecute", std::move(event_args), profile_);
  router->DispatchEventToExtension(extension_->id(), std::move(event));

  ExecuteDoneOnUIThread(true);
}

void FileBrowserHandlerExecutor::SetupHandlerHostFileAccessPermissions(
    FileDefinitionList* file_definition_list,
    const Extension* extension,
    int handler_pid) {
  const FileBrowserHandler* action =
      FindFileBrowserHandlerForActionId(extension_.get(), action_id_);
  for (FileDefinitionList::const_iterator iter = file_definition_list->begin();
       iter != file_definition_list->end();
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
      num_opened += util::OpenFileWithBrowser(profile, file_url) ? 1 : 0;
    }
  }
  return num_opened > 0;
}

}  // namespace

bool ExecuteFileBrowserHandler(
    Profile* profile,
    const Extension* extension,
    const std::string& action_id,
    const std::vector<FileSystemURL>& file_urls,
    const file_tasks::FileTaskFinishedCallback& done) {
  // Forbid calling undeclared handlers.
  if (!FindFileBrowserHandlerForActionId(extension, action_id))
    return false;

  // Some action IDs of the file manager's file browser handlers require the
  // files to be directly opened with the browser.
  if (ShouldBeOpenedWithBrowser(extension->id(), action_id)) {
    const bool result = OpenFilesWithBrowser(profile, file_urls);
    if (result && !done.is_null())
      done.Run(extensions::api::file_manager_private::TASK_RESULT_OPENED);
    return result;
  }

  // The executor object will be self deleted on completion.
  (new FileBrowserHandlerExecutor(
      profile, extension, action_id))->Execute(file_urls, done);
  return true;
}

FileBrowserHandlerList FindFileBrowserHandlers(
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
      std::set<const FileBrowserHandler*> common_handler_set(
          common_handlers.begin(), common_handlers.end());

      for (FileBrowserHandlerList::const_iterator itr = handlers.begin();
           itr != handlers.end(); ++itr) {
        if (base::ContainsKey(common_handler_set, *itr))
          intersection.push_back(*itr);
      }

      std::swap(common_handlers, intersection);
      if (common_handlers.empty())
        return FileBrowserHandlerList();
    }
  }

  return common_handlers;
}

}  // namespace file_browser_handlers
}  // namespace file_manager
