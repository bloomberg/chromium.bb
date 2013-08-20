// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/app_id.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/url_util.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/webplugininfo.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserContext;
using content::BrowserThread;
using content::PluginService;
using content::UserMetricsAction;
using extensions::Extension;
using extensions::app_file_handler_util::FindFileHandlersForFiles;
using extensions::app_file_handler_util::PathAndMimeTypeSet;
using fileapi::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

const char kCRXExtension[] = ".crx";
const char kPdfExtension[] = ".pdf";
const char kSwfExtension[] = ".swf";
// List of file extension we can open in tab.
const char* kBrowserSupportedExtensions[] = {
#if defined(GOOGLE_CHROME_BUILD)
    ".pdf", ".swf",
#endif
    ".bmp", ".jpg", ".jpeg", ".png", ".webp", ".gif", ".txt", ".html", ".htm",
    ".mhtml", ".mht", ".svg"
};

bool IsSupportedBrowserExtension(const char* file_extension) {
  for (size_t i = 0; i < arraysize(kBrowserSupportedExtensions); i++) {
    if (base::strcasecmp(file_extension, kBrowserSupportedExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool IsCRXFile(const char* file_extension) {
  return base::strcasecmp(file_extension, kCRXExtension) == 0;
}

bool IsPepperPluginEnabled(Profile* profile,
                           const base::FilePath& plugin_path) {
  content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(plugin_path);
  if (!pepper_info)
    return false;

  scoped_refptr<PluginPrefs> plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (!plugin_prefs.get())
    return false;

  return plugin_prefs->IsPluginEnabled(pepper_info->ToWebPluginInfo());
}

bool IsPdfPluginEnabled(Profile* profile) {
  base::FilePath plugin_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &plugin_path);
  return IsPepperPluginEnabled(profile, plugin_path);
}

bool IsFlashPluginEnabled(Profile* profile) {
  base::FilePath plugin_path(
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath));
  if (plugin_path.empty())
    PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &plugin_path);
  return IsPepperPluginEnabled(profile, plugin_path);
}

void OpenNewTab(Profile* profile, const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile ? profile : ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::AddSelectedTabWithURL(browser, url, content::PAGE_TRANSITION_LINK);
  // If the current browser is not tabbed then the new tab will be created
  // in a different browser. Make sure it is visible.
  browser->window()->Show();
}

// Shows a warning message box saying that the file could not be opened.
void ShowWarningMessageBox(Profile* profile, const base::FilePath& path) {
  // TODO: if FindOrCreateTabbedBrowser creates a new browser the returned
  // browser is leaked.
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(profile,
                                        chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowMessageBox(
      browser->window()->GetNativeWindow(),
      l10n_util::GetStringFUTF16(
          IDS_FILE_BROWSER_ERROR_VIEWING_FILE_TITLE,
          UTF8ToUTF16(path.BaseName().value())),
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_VIEWING_FILE),
      chrome::MESSAGE_BOX_TYPE_WARNING);
}

void InstallCRX(Browser* browser, const base::FilePath& path) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(browser->profile())->extension_service();
  CHECK(service);

  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          service,
          scoped_ptr<ExtensionInstallPrompt>(new ExtensionInstallPrompt(
              browser->profile(), NULL, NULL))));
  installer->set_error_on_unsupported_requirements(true);
  installer->set_is_gallery_install(false);
  installer->set_allow_silent_install(false);
  installer->InstallCrx(path);
}

// Called when a crx file on Drive was downloaded.
void OnCRXDownloadCallback(Browser* browser,
                           drive::FileError error,
                           const base::FilePath& file,
                           scoped_ptr<drive::ResourceEntry> entry) {
  if (error != drive::FILE_ERROR_OK)
    return;
  InstallCRX(browser, file);
}

// Grants file system access to the file browser.
bool GrantFileSystemAccessToFileBrowser(Profile* profile) {
  // File browser always runs in the site for its extension id, so that is the
  // site for which file access permissions should be granted.
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

// Opens the file specified by |url| with |task|.
void OpenFileWithTask(Profile* profile,
                      const file_tasks::TaskDescriptor& task,
                      const GURL& url) {
  // If File Browser has not been open yet then it did not request access
  // to the file system. Do it now.
  if (!GrantFileSystemAccessToFileBrowser(profile))
    return;

  fileapi::FileSystemContext* file_system_context =
      GetFileSystemContextForExtensionId(
          profile, kFileManagerAppId);

  // We are executing the task on behalf of File Browser extension.
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

// Opens the file specified with |path|. Used to implement internal handlers
// of special action IDs such as "auto-open", "open", and "select".
void OpenFileWithInternalActionId(const base::FilePath& path,
                                  const std::string& action_id) {
  DCHECK(action_id == "auto-open" ||
         action_id == "open" ||
         action_id == "select");

  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, kFileManagerAppId, &url))
    return;

  file_tasks::TaskDescriptor task(kFileManagerAppId,
                                  file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                                  action_id);
  OpenFileWithTask(profile, task, url);
}

Browser* GetBrowserForUrl(GURL target_url) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(idx);
      const GURL& url = web_contents->GetLastCommittedURL();
      if (url == target_url)
        return browser;
    }
  }
  return NULL;
}

// Opens the file specified by |path| and |url| with a file handler,
// preferably the default handler for the type of the file.  Returns false if
// no file handler is found.
bool OpenFileWithFileHandler(Profile* profile,
                              const base::FilePath& path,
                              const GURL& url,
                              const std::string& mime_type,
                              const std::string& default_task_id) {
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return false;

  PathAndMimeTypeSet files;
  files.insert(std::make_pair(path, mime_type));
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
        OpenFileWithTask(profile, task, url);
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
    OpenFileWithTask(profile, task, url);
    return true;
  }
  return false;
}

// Returns true if |action_id| indicates that the file currently being
// handled should be opened with the browser (i.e. should be opened with
// OpenFileWithBrowser()).
bool ShouldBeOpenedWithBrowser(const std::string& action_id) {
  return (action_id == "view-pdf" ||
          action_id == "view-swf" ||
          action_id == "view-in-browser" ||
          action_id == "install-crx" ||
          action_id == "open-hosted-generic" ||
          action_id == "open-hosted-gdoc" ||
          action_id == "open-hosted-gsheet" ||
          action_id == "open-hosted-gslides");
}

// Opens the file specified by |path| and |url| with the file browser handler
// specified by |handler|. Returns false if failed to open the file.
bool OpenFileWithFileBrowserHandler(Profile* profile,
                                    const base::FilePath& path,
                                    const FileBrowserHandler& handler,
                                    const GURL& url) {
  std::string extension_id = handler.extension_id();
  std::string action_id = handler.id();
  Browser* browser = chrome::FindLastActiveWithProfile(profile,
      chrome::HOST_DESKTOP_TYPE_ASH);

  // If there is no browsers for the profile, bail out. Return true so warning
  // about file type not being supported is not displayed.
  if (!browser)
    return true;

  // Some action IDs of the file manager's file browser handlers require the
  // file to be directly opened with the browser.
  if (extension_id == kFileManagerAppId &&
      ShouldBeOpenedWithBrowser(action_id)) {
    return OpenFileWithBrowser(browser, path);
  }

  file_tasks::TaskDescriptor task(extension_id,
                                  file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                                  action_id);
  OpenFileWithTask(profile, task, url);
  return true;
}

// Opens the file specified by |path| with a handler (either of file browser
// handler or file handler, preferably the default handler for the type of
// the file), or opens the file with the browser. Returns false if failed to
// open the file.
bool OpenFileWithHandlerOrBrowser(Profile* profile,
                                  const base::FilePath& path) {
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, kFileManagerAppId, &url))
    return false;

  std::string mime_type = GetMimeTypeForPath(path);
  std::string default_task_id = file_tasks::GetDefaultTaskIdFromPrefs(
      profile, mime_type, path.Extension());

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
          profile, url, path);
  if (!handler) {
    return OpenFileWithFileHandler(
        profile, path, url, mime_type, default_task_id);
  }

  std::string handler_task_id = file_tasks::MakeTaskID(
        handler->extension_id(),
        file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
        handler->id());
  if (handler_task_id != default_task_id &&
      !file_browser_handlers::IsFallbackFileBrowserHandler(handler) &&
      OpenFileWithFileHandler(
          profile, path, url, mime_type, default_task_id)) {
    return true;
  }
  return OpenFileWithFileBrowserHandler(profile, path, *handler, url);
}

// Reads the alternate URL from a GDoc file. When it fails, returns a file URL
// for |file_path| as fallback.
// Note that an alternate url is a URL to open a hosted document.
GURL ReadUrlFromGDocOnBlockingPool(const base::FilePath& file_path) {
  GURL url = drive::util::ReadUrlFromGDocFile(file_path);
  if (url.is_empty())
    url = net::FilePathToFileURL(file_path);
  return url;
}

// Used to implement ViewItem().
void ContinueViewItem(Profile* profile,
                      const base::FilePath& path,
                      base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == base::PLATFORM_FILE_OK) {
    // A directory exists at |path|. Open it with the file manager.
    OpenFileWithInternalActionId(path, "open");
  } else {
    // |path| should be a file. Open it with a handler or the browser.
    if (!OpenFileWithHandlerOrBrowser(profile, path))
      ShowWarningMessageBox(profile, path);
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

string16 GetTitleFromType(ui::SelectFileDialog::Type dialog_type) {
  string16 title;
  switch (dialog_type) {
    case ui::SelectFileDialog::SELECT_NONE:
      // Full page file manager doesn't need a title.
      break;

    case ui::SelectFileDialog::SELECT_FOLDER:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_FOLDER_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_UPLOAD_FOLDER:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_UPLOAD_FOLDER_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_SAVEAS_FILE_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_OPEN_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_OPEN_FILE_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_OPEN_MULTI_FILE_TITLE);
      break;

    default:
      NOTREACHED();
  }

  return title;
}

void ViewRemovableDrive(const base::FilePath& path) {
  OpenFileWithInternalActionId(path, "auto-open");
}

void OpenActionChoiceDialog(const base::FilePath& path, bool advanced_mode) {
  const int kDialogWidth = 394;
  // TODO(dgozman): remove 50, which is a title height once popup window
  // will have no title.
  const int kDialogHeight = 316 + 50;

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  base::FilePath virtual_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(
          profile, kFileManagerAppId, path, &virtual_path))
    return;
  GURL dialog_url = GetActionChoiceUrl(virtual_path, advanced_mode);

  const gfx::Size screen = ash::Shell::GetScreen()->GetPrimaryDisplay().size();
  const gfx::Rect bounds((screen.width() - kDialogWidth) / 2,
                         (screen.height() - kDialogHeight) / 2,
                         kDialogWidth,
                         kDialogHeight);

  Browser* browser = GetBrowserForUrl(dialog_url);

  if (browser) {
    browser->window()->Show();
    return;
  }

  ExtensionService* service = extensions::ExtensionSystem::Get(
    profile ? profile : ProfileManager::GetDefaultProfileOrOffTheRecord())->
        extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(kFileManagerAppId, false);
  if (!extension)
    return;

  chrome::AppLaunchParams params(profile, extension,
                                 extension_misc::LAUNCH_WINDOW,
                                 NEW_FOREGROUND_TAB);
  params.override_url = dialog_url;
  params.override_bounds = bounds;
  chrome::OpenApplication(params);
}

void ViewItem(const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, kFileManagerAppId, &url) ||
      !GrantFileSystemAccessToFileBrowser(profile)) {
    ShowWarningMessageBox(profile, path);
    return;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      GetFileSystemContextForExtensionId(
          profile, kFileManagerAppId);

  CheckIfDirectoryExists(file_system_context, url,
                         base::Bind(&ContinueViewItem, profile, path));
}

void ShowFileInFolder(const base::FilePath& path) {
  // This action changes the selection so we do not reuse existing tabs.
  OpenFileWithInternalActionId(path, "select");
}

bool OpenFileWithBrowser(Browser* browser, const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = browser->profile();
  std::string file_extension = path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsSupportedBrowserExtension(file_extension.data()) ||
      ShouldBeOpenedWithPlugin(profile, file_extension.data())) {
    GURL page_url = net::FilePathToFileURL(path);
    // Override drive resource to point to internal handler instead of file URL.
    if (drive::util::IsUnderDriveMountPoint(path)) {
      page_url = drive::util::FilePathToDriveURL(
          drive::util::ExtractDrivePath(path));
    }
    OpenNewTab(profile, page_url);
    return true;
  }

  if (drive::util::HasGDocFileExtension(path)) {
    if (drive::util::IsUnderDriveMountPoint(path)) {
      // The file is on Google Docs. Open with drive URL.
      GURL url = drive::util::FilePathToDriveURL(
          drive::util::ExtractDrivePath(path));
      OpenNewTab(profile, url);
    } else {
      // The file is local (downloaded from an attachment or otherwise copied).
      // Parse the file to extract the Docs url and open this url.
      base::PostTaskAndReplyWithResult(
          BrowserThread::GetBlockingPool(),
          FROM_HERE,
          base::Bind(&ReadUrlFromGDocOnBlockingPool, path),
          base::Bind(&OpenNewTab, static_cast<Profile*>(NULL)));
    }
    return true;
  }

  if (IsCRXFile(file_extension.data())) {
    if (drive::util::IsUnderDriveMountPoint(path)) {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile);
      if (!integration_service)
        return false;
      integration_service->file_system()->GetFileByPath(
          drive::util::ExtractDrivePath(path),
          base::Bind(&OnCRXDownloadCallback, browser));
    } else {
      InstallCRX(browser, path);
    }
    return true;
  }

  // Failed to open the file of unknown type.
  LOG(WARNING) << "Unknown file type: " << path.value();
  return false;
}

// If a bundled plugin is enabled, we should open pdf/swf files in a tab.
bool ShouldBeOpenedWithPlugin(Profile* profile, const char* file_extension) {
  if (LowerCaseEqualsASCII(file_extension, kPdfExtension))
    return IsPdfPluginEnabled(profile);
  if (LowerCaseEqualsASCII(file_extension, kSwfExtension))
    return IsFlashPluginEnabled(profile);
  return false;
}

std::string GetMimeTypeForPath(const base::FilePath& file_path) {
  const base::FilePath::StringType file_extension =
      StringToLowerASCII(file_path.Extension());

  // TODO(thorogood): Rearchitect this call so it can run on the File thread;
  // GetMimeTypeFromFile requires this on Linux. Right now, we use
  // Chrome-level knowledge only.
  std::string mime_type;
  if (file_extension.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(file_extension.substr(1),
                                              &mime_type)) {
    // If the file doesn't have an extension or its mime-type cannot be
    // determined, then indicate that it has the empty mime-type. This will
    // only be matched if the Web Intents accepts "*" or "*/*".
    return "";
  } else {
    return mime_type;
  }
}

}  // namespace util
}  // namespace file_manager
