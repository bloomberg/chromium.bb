// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/file_manager/app_id.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/mime_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/url_util.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserContext;
using content::BrowserThread;
using content::UserMetricsAction;
using extensions::Extension;
using extensions::app_file_handler_util::FindFileHandlersForFiles;
using extensions::app_file_handler_util::PathAndMimeTypeSet;
using fileapi::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

// Shows a warning message box saying that the file could not be opened.
void ShowWarningMessageBox(Profile* profile, const base::FilePath& file_path) {
  // TODO: if FindOrCreateTabbedBrowser creates a new browser the returned
  // browser is leaked.
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(profile,
                                        chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowMessageBox(
      browser->window()->GetNativeWindow(),
      l10n_util::GetStringFUTF16(
          IDS_FILE_BROWSER_ERROR_VIEWING_FILE_TITLE,
          UTF8ToUTF16(file_path.BaseName().value())),
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_VIEWING_FILE),
      chrome::MESSAGE_BOX_TYPE_WARNING);
}

// Grants file system access to the file manager.
bool GrantFileSystemAccessToFileBrowser(Profile* profile) {
  // The file manager always runs in the site for its extension id, so that
  // is the site for which file access permissions should be granted.
  GURL site = extensions::ExtensionSystem::Get(profile)->extension_service()->
      GetSiteForExtensionId(kFileManagerAppId);
  fileapi::ExternalFileSystemBackend* backend =
      BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_backend();
  if (!backend)
    return false;
  backend->GrantFullAccessToExtension(kFileManagerAppId);
  return true;
}

// Executes the |task| for the file specified by |url|.
void ExecuteFileTaskForUrl(Profile* profile,
                           const file_tasks::TaskDescriptor& task,
                           const GURL& url) {
  // If the file manager has not been open yet then it did not request access
  // to the file system. Do it now.
  if (!GrantFileSystemAccessToFileBrowser(profile))
    return;

  fileapi::FileSystemContext* file_system_context =
      GetFileSystemContextForExtensionId(
          profile, kFileManagerAppId);

  // We are executing the task on behalf of the file manager.
  const GURL source_url = GetFileManagerMainPageUrl();
  std::vector<FileSystemURL> urls;
  urls.push_back(file_system_context->CrackURL(url));

  file_tasks::ExecuteFileTask(
      profile,
      source_url,
      kFileManagerAppId,
      0, // no tab id
      task,
      urls,
      file_tasks::FileTaskFinishedCallback());
}

// Opens the file manager for the specified |file_path|. Used to implement
// internal handlers of special action IDs:
//
// "open" - Open the file manager for the given folder.
// "auto-open" - Open the file manager for the given removal drive and close
//               the file manager when the removal drive is unmounted.
// "select" - Open the file manager for the given file. The folder containing
//            the file will be opened with the file selected.
void OpenFileManagerWithInternalActionId(const base::FilePath& file_path,
                                         const std::string& action_id) {
  DCHECK(action_id == "auto-open" ||
         action_id == "open" ||
         action_id == "select");

  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, kFileManagerAppId, &url))
    return;

  file_tasks::TaskDescriptor task(kFileManagerAppId,
                                  file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                                  action_id);
  ExecuteFileTaskForUrl(profile, task, url);
}

// Opens the file specified by |file_path| and |url| with a file handler,
// preferably the default handler for the type of the file.  Returns false if
// no file handler is found.
bool OpenFileWithFileHandler(Profile* profile,
                              const base::FilePath& file_path,
                              const GURL& url,
                              const std::string& mime_type,
                              const std::string& default_task_id) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return false;

  PathAndMimeTypeSet files;
  files.insert(std::make_pair(file_path, mime_type));
  const extensions::FileHandlerInfo* first_handler = NULL;
  const extensions::Extension* extension_for_first_handler = NULL;

  // If we find the default handler, we execute it immediately, but otherwise,
  // we remember the first handler, and if there was no default handler, simply
  // execute the first one.
  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();

    // We don't support using hosted apps to open files.
    if (!extension->is_platform_app())
      continue;

    // We only support apps that specify "incognito: split" if in incognito
    // mode.
    if (profile->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    typedef std::vector<const extensions::FileHandlerInfo*> FileHandlerList;
    FileHandlerList file_handlers = FindFileHandlersForFiles(*extension, files);
    for (FileHandlerList::iterator i = file_handlers.begin();
         i != file_handlers.end(); ++i) {
      const extensions::FileHandlerInfo* handler = *i;
      std::string task_id = file_tasks::MakeTaskID(
          extension->id(),
          file_tasks::TASK_TYPE_FILE_HANDLER,
          handler->id);
      if (task_id == default_task_id) {
        file_tasks::TaskDescriptor task(extension->id(),
                                        file_tasks::TASK_TYPE_FILE_HANDLER,
                                        handler->id);
        ExecuteFileTaskForUrl(profile, task, url);
        return true;

      } else if (!first_handler) {
        first_handler = handler;
        extension_for_first_handler = extension;
      }
    }
  }
  if (first_handler) {
    file_tasks::TaskDescriptor task(extension_for_first_handler->id(),
                                    file_tasks::TASK_TYPE_FILE_HANDLER,
                                    first_handler->id);
    ExecuteFileTaskForUrl(profile, task, url);
    return true;
  }
  return false;
}

// Opens the file specified by |file_path| and |url| with the file browser
// handler specified by |handler|. Returns false if failed to open the file.
bool OpenFileWithFileBrowserHandler(Profile* profile,
                                    const base::FilePath& file_path,
                                    const FileBrowserHandler& handler,
                                    const GURL& url) {
  file_tasks::TaskDescriptor task(handler.extension_id(),
                                  file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                                  handler.id());
  ExecuteFileTaskForUrl(profile, task, url);
  return true;
}

// Opens the file specified by |file_path| with a handler (either of file
// browser handler or file handler, preferably the default handler for the
// type of the file), or opens the file with the browser. Returns false if
// failed to open the file.
bool OpenFileWithHandler(Profile* profile, const base::FilePath& file_path) {
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, kFileManagerAppId, &url))
    return false;

  std::string mime_type = GetMimeTypeForPath(file_path);
  std::string default_task_id = file_tasks::GetDefaultTaskIdFromPrefs(
      profile, mime_type, file_path.Extension());

  // We choose the file handler from the following in decreasing priority or
  // fail if none support the file type:
  // 1. default file browser handler
  // 2. default file handler
  // 3. a fallback handler (e.g. opening in the browser)
  // 4. non-default file handler
  // 5. non-default file browser handler
  // Note that there can be at most one of default extension and default app.
  const FileBrowserHandler* handler =
      file_browser_handlers::FindFileBrowserHandlerForURLAndPath(
          profile, url, file_path);
  if (!handler) {
    return OpenFileWithFileHandler(
        profile, file_path, url, mime_type, default_task_id);
  }

  std::string handler_task_id = file_tasks::MakeTaskID(
        handler->extension_id(),
        file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
        handler->id());
  if (handler_task_id != default_task_id &&
      !file_browser_handlers::IsFallbackFileBrowserHandler(handler) &&
      OpenFileWithFileHandler(
          profile, file_path, url, mime_type, default_task_id)) {
    return true;
  }
  return OpenFileWithFileBrowserHandler(profile, file_path, *handler, url);
}

// Used to implement OpenItem().
void ContinueOpenItem(Profile* profile,
                      const base::FilePath& file_path,
                      base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == base::PLATFORM_FILE_OK) {
    // A directory exists at |file_path|. Open it with the file manager.
    OpenFileManagerWithInternalActionId(file_path, "open");
  } else {
    // |file_path| should be a file. Open it with a handler.
    if (!OpenFileWithHandler(profile, file_path))
      ShowWarningMessageBox(profile, file_path);
  }
}

// Used to implement CheckIfDirectoryExists().
void CheckIfDirectoryExistsOnIOThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& url,
    const fileapi::FileSystemOperationRunner::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  fileapi::FileSystemURL file_system_url = file_system_context->CrackURL(url);
  file_system_context->operation_runner()->DirectoryExists(
      file_system_url, callback);
}

// Checks if a directory exists at |url|.
void CheckIfDirectoryExists(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& url,
    const fileapi::FileSystemOperationRunner::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CheckIfDirectoryExistsOnIOThread,
                 file_system_context,
                 url,
                 google_apis::CreateRelayCallback(callback)));
}

}  // namespace

void OpenRemovableDrive(const base::FilePath& file_path) {
  OpenFileManagerWithInternalActionId(file_path, "auto-open");
}

void OpenItem(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, kFileManagerAppId, &url) ||
      !GrantFileSystemAccessToFileBrowser(profile)) {
    ShowWarningMessageBox(profile, file_path);
    return;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      GetFileSystemContextForExtensionId(
          profile, kFileManagerAppId);

  CheckIfDirectoryExists(file_system_context, url,
                         base::Bind(&ContinueOpenItem, profile, file_path));
}

void ShowItemInFolder(const base::FilePath& file_path) {
  // This action changes the selection so we do not reuse existing tabs.
  OpenFileManagerWithInternalActionId(file_path, "select");
}

}  // namespace util
}  // namespace file_manager
