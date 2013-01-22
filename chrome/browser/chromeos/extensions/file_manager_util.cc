// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/extensions/file_manager_util.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/media/media_player.h"
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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/pepper_plugin_info.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/plugins/webplugininfo.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserContext;
using content::BrowserThread;
using content::PluginService;
using content::UserMetricsAction;
using extensions::Extension;
using file_handler_util::FileTaskExecutor;

#define FILEBROWSER_EXTENSON_ID "hhaomjibdihmijegdhdafkllkbggdgoj"
const char kFileBrowserDomain[] = FILEBROWSER_EXTENSON_ID;

const char kFileBrowserGalleryTaskId[] = "gallery";
const char kFileBrowserMountArchiveTaskId[] = "mount-archive";
const char kFileBrowserWatchTaskId[] = "watch";
const char kFileBrowserPlayTaskId[] = "play";

const char kVideoPlayerAppName[] = "videoplayer";

namespace file_manager_util {
namespace {

#define FILEBROWSER_URL(PATH) \
    ("chrome-extension://" FILEBROWSER_EXTENSON_ID "/" PATH)
// This is the "well known" url for the file manager extension from
// browser/resources/file_manager.  In the future we may provide a way to swap
// out this file manager for an aftermarket part, but not yet.
const char kFileBrowserExtensionUrl[] = FILEBROWSER_URL("");
const char kBaseFileBrowserUrl[] = FILEBROWSER_URL("main.html");
const char kMediaPlayerUrl[] = FILEBROWSER_URL("mediaplayer.html");
const char kVideoPlayerUrl[] = FILEBROWSER_URL("video_player.html");
const char kActionChoiceUrl[] = FILEBROWSER_URL("action_choice.html");
#undef FILEBROWSER_URL
#undef FILEBROWSER_EXTENSON_ID

const char kCRXExtension[] = ".crx";
const char kPdfExtension[] = ".pdf";
// List of file extension we can open in tab.
const char* kBrowserSupportedExtensions[] = {
#if defined(GOOGLE_CHROME_BUILD)
    ".pdf",
#endif
    ".bmp", ".jpg", ".jpeg", ".png", ".webp", ".gif", ".txt", ".html", ".htm",
    ".mhtml", ".mht", ".svg"
};

// Keep in sync with 'open-hosted' task handler in the File Browser manifest.
const char* kGDocsExtensions[] = {
    ".gdoc", ".gsheet", ".gslides", ".gdraw", ".gtable", ".glink"
};

// List of all extensions we want to be shown in histogram that keep track of
// files that were unsuccessfully tried to be opened.
// The list has to be synced with histogram values.
const char* kUMATrackingExtensions[] = {
  "other", ".doc", ".docx", ".odt", ".rtf", ".pdf", ".ppt", ".pptx", ".odp",
  ".xls", ".xlsx", ".ods", ".csv", ".odf", ".rar", ".asf", ".wma", ".wmv",
  ".mov", ".mpg", ".log"
};

bool IsSupportedBrowserExtension(const char* file_extension) {
  for (size_t i = 0; i < arraysize(kBrowserSupportedExtensions); i++) {
    if (base::strcasecmp(file_extension, kBrowserSupportedExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool IsSupportedGDocsExtension(const char* file_extension) {
  for (size_t i = 0; i < arraysize(kGDocsExtensions); i++) {
    if (base::strcasecmp(file_extension, kGDocsExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool IsCRXFile(const char* file_extension) {
  return base::strcasecmp(file_extension, kCRXExtension) == 0;
}

// Returns index |ext| has in the |array|. If there is no |ext| in |array|, last
// element's index is return (last element should have irrelevant value).
int UMAExtensionIndex(const char *file_extension,
                      const char** array,
                      size_t array_size) {
  for (size_t i = 0; i < array_size; i++) {
    if (base::strcasecmp(file_extension, array[i]) == 0) {
      return i;
    }
  }
  return 0;
}

// Convert numeric dialog type to a string.
std::string GetDialogTypeAsString(
    ui::SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case ui::SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case ui::SelectFileDialog::SELECT_FOLDER:
      type_str = "folder";
      break;

    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}

DictionaryValue* ProgessStatusToDictionaryValue(
    Profile* profile,
    const std::string& extension_id,
    const google_apis::OperationProgressStatus& status) {
  scoped_ptr<DictionaryValue> result(new DictionaryValue());
  GURL file_url;
  if (file_manager_util::ConvertFileToFileSystemUrl(profile,
          drive::util::GetSpecialRemoteRootPath().Append(
              FilePath(status.file_path)),
          extension_id,
          &file_url)) {
    result->SetString("fileUrl", file_url.spec());
  }

  result->SetString("transferState",
                    OperationTransferStateToString(status.transfer_state));
  result->SetString("transferType",
                    OperationTypeToString(status.operation_type));
  result->SetInteger("processed", static_cast<int>(status.progress_current));
  result->SetInteger("total", static_cast<int>(status.progress_total));
  return result.release();
}

void OpenNewTab(const GURL& url, Profile* profile) {
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
void ShowWarningMessageBox(Profile* profile, const FilePath& path) {
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

// Called when a file on Drive was found. Opens the file found at |file_path|
// in a new tab with a URL computed based on the |file_type|
void OnDriveFileFound(Profile* profile,
                      const FilePath& file_path,
                      drive::DriveFileType file_type,
                      drive::DriveFileError error,
                      scoped_ptr<drive::DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = drive::DRIVE_FILE_ERROR_NOT_FOUND;

  if (error == drive::DRIVE_FILE_OK) {
    GURL page_url;
    if (file_type == drive::REGULAR_FILE) {
      page_url = drive::util::GetFileResourceUrl(
          entry_proto->resource_id(),
          entry_proto->base_name());
    } else if (file_type == drive::HOSTED_DOCUMENT) {
      page_url = GURL(entry_proto->file_specific_info().alternate_url());
    } else {
      NOTREACHED();
    }
    OpenNewTab(page_url, profile);
  } else {
    ShowWarningMessageBox(profile, file_path);
  }
}

void InstallCRX(Browser* browser, const FilePath& path) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(browser->profile())->extension_service();
  CHECK(service);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          service,
          new ExtensionInstallPrompt(web_contents)));
  installer->set_error_on_unsupported_requirements(true);
  installer->set_is_gallery_install(false);
  installer->set_allow_silent_install(false);
  installer->InstallCrx(path);
}

// Called when a crx file on Drive was downloaded.
void OnCRXDownloadCallback(Browser* browser,
                           drive::DriveFileError error,
                           const FilePath& file,
                           const std::string& unused_mime_type,
                           drive::DriveFileType file_type) {
  if (error != drive::DRIVE_FILE_OK || file_type != drive::REGULAR_FILE)
    return;
  InstallCRX(browser, file);
}

enum TAB_REUSE_MODE {
  REUSE_ANY_FILE_MANAGER,
  REUSE_SAME_PATH,
  REUSE_NEVER
};

bool FileManageTabExists(const FilePath& path, TAB_REUSE_MODE mode) {
  if (mode == REUSE_NEVER)
    return false;

  // We always open full-tab File Manager via chrome://files URL, never
  // chrome-extension://, so we only check against chrome://files
  const GURL origin(chrome::kChromeUIFileManagerURL);
  const std::string ref = std::string("/") + path.value();

  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(idx);
      const GURL& url = web_contents->GetURL();
      if (origin == url.GetOrigin()) {
        if (mode == REUSE_ANY_FILE_MANAGER || ref == url.ref()) {
          if (mode == REUSE_SAME_PATH && tab_strip->active_index() != idx) {
            browser->window()->Show();
            tab_strip->ActivateTabAt(idx, false);
          }
          return true;
        }
      }
    }
  }

  return false;
}

bool IsFileManagerPackaged() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kFileManagerPackaged);
}

void ExecuteHandler(Profile* profile,
                    std::string extension_id,
                    std::string action_id,
                    const GURL& url) {
  // We are executing the task on behalf of File Browser extension.
  const GURL source_url(kBaseFileBrowserUrl);

  // If File Browser has not been open yet then it did not request access
  // to the file system. Do it now.
  // File browser always runs in the site for its extension id, so that is the
  // site for which file access permissions should be granted.
  GURL site = extensions::ExtensionSystem::Get(profile)->extension_service()->
      GetSiteForExtensionId(kFileBrowserDomain);
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_provider();
  if (!external_provider)
    return;
  external_provider->GrantFullAccessToExtension(source_url.host());

  std::vector<GURL> urls;
  urls.push_back(url);
  scoped_refptr<FileTaskExecutor> executor = FileTaskExecutor::Create(profile,
      source_url, kFileBrowserDomain, 0 /* no tab id */, extension_id,
      file_handler_util::kTaskFile, action_id);
  executor->Execute(urls);
}

void OpenFileBrowser(const FilePath& path,
                     TAB_REUSE_MODE mode,
                     const std::string& action_id) {
  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));

  if (FileManageTabExists(path, mode))
    return;

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  if (IsFileManagerPackaged() && !path.value().empty()) {
    GURL url;
    if (!ConvertFileToFileSystemUrl(profile, path, kFileBrowserDomain, &url))
      return;

    // Some values of |action_id| are not listed in the manifest and are used
    // to parametrize the behavior when opening the Files app window.
    ExecuteHandler(profile, kFileBrowserDomain, action_id, url);
    return;
  }

  std::string url = chrome::kChromeUIFileManagerURL;
  if (action_id.size()) {
    DictionaryValue arg_value;
    arg_value.SetString("action", action_id);
    std::string query;
    base::JSONWriter::Write(&arg_value, &query);
    url += "?" + net::EscapeUrlEncodedData(query, false);
  }
  if (!path.empty()) {
    FilePath virtual_path;
    if (!ConvertFileToRelativeFileSystemPath(profile, kFileBrowserDomain, path,
                                             &virtual_path))
      return;
    url += "#/" + net::EscapeUrlEncodedData(virtual_path.value(), false);
  }

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(kFileBrowserDomain, false);
  if (!extension)
    return;

  application_launch::LaunchParams params(profile, extension,
                                          extension_misc::LAUNCH_WINDOW,
                                          NEW_FOREGROUND_TAB);
  params.override_url = GURL(url);
  application_launch::OpenApplication(params);
}

Browser* GetBrowserForUrl(GURL target_url) {
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(idx);
      const GURL& url = web_contents->GetURL();
      if (url == target_url)
        return browser;
    }
  }
  return NULL;
}

bool ExecuteDefaultHandler(Profile* profile, const FilePath& path) {
  GURL url;
  if (!ConvertFileToFileSystemUrl(profile, path, kFileBrowserDomain, &url))
    return false;

  const FileBrowserHandler* handler;
  if (!file_handler_util::GetTaskForURL(profile, url, &handler))
    return false;

  std::string extension_id = handler->extension_id();
  std::string action_id = handler->id();
  Browser* browser = chrome::FindLastActiveWithProfile(profile,
      chrome::HOST_DESKTOP_TYPE_ASH);

  // If there is no browsers for the profile, bail out. Return true so warning
  // about file type not being supported is not displayed.
  if (!browser)
    return true;

  if (extension_id == kFileBrowserDomain) {
    if (IsFileManagerPackaged()) {
      if (action_id == kFileBrowserGalleryTaskId ||
          action_id == kFileBrowserMountArchiveTaskId ||
          action_id == kFileBrowserPlayTaskId ||
          action_id == kFileBrowserWatchTaskId) {
        ExecuteHandler(profile, extension_id, action_id, url);
        return true;
      }
      return ExecuteBuiltinHandler(browser, path, action_id);
    }

    // Only two of the built-in File Browser tasks require opening the File
    // Browser tab.
    if (action_id == kFileBrowserGalleryTaskId ||
        action_id == kFileBrowserMountArchiveTaskId) {
      // Tab reuse currently does not work for these two tasks.
      // |gallery| tries to put the file url into the tab url but it does not
      // work on Chrome OS.
      // |mount-archive| does not even try.
      OpenFileBrowser(path, REUSE_SAME_PATH, "");
      return true;
    }
    return ExecuteBuiltinHandler(browser, path, action_id);
  }

  ExecuteHandler(profile, extension_id, action_id, url);
  return true;
}

// Reads an entire file into a string. Fails is the file is 4K or longer.
bool ReadSmallFileToString(const FilePath& path, std::string* contents) {
  FILE* file = file_util::OpenFile(path, "rb");
  if (!file) {
    return false;
  }

  char buf[1 << 12];  // 4K
  size_t len = fread(buf, 1, sizeof(buf), file);
  if (len > 0) {
    contents->append(buf, len);
  }
  file_util::CloseFile(file);

  return len < sizeof(buf);
}

// Reads JSON from a Google Docs file, extracts a document url and opens it
// in a tab.
void ReadUrlFromGDocOnFileThread(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::string contents;
  if (!ReadSmallFileToString(file_path, &contents)) {
    LOG(ERROR) << "Error reading " << file_path.value();
    return;
  }

  scoped_ptr<base::Value> root_value;
  root_value.reset(base::JSONReader::Read(contents));

  DictionaryValue* dictionary_value;
  std::string edit_url_string;
  if (!root_value.get() ||
      !root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString("url", &edit_url_string)) {
    LOG(ERROR) << "Invalid JSON in " << file_path.value();
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(OpenNewTab, GURL(edit_url_string), (Profile*)NULL));
}

// Used to implement ViewItem().
void ContinueViewItem(Profile* profile,
                      const FilePath& path,
                      base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == base::PLATFORM_FILE_OK) {
    // A directory exists at |path|. Open it with FileBrowser.
    OpenFileBrowser(path, REUSE_SAME_PATH, "open");
  } else {
    if (!ExecuteDefaultHandler(profile, path))
      ShowWarningMessageBox(profile, path);
  }
}

// Used to implement CheckIfDirectoryExists().
void CheckIfDirectoryExistsOnIOThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& url,
    const fileapi::FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  fileapi::FileSystemURL file_system_url(url);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  fileapi::FileSystemOperation* operation =
      file_system_context->CreateFileSystemOperation(file_system_url, &error);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error);
    return;
  }
  operation->DirectoryExists(file_system_url, callback);
}

// Checks if a directory exists at |url|.
void CheckIfDirectoryExists(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const GURL& url,
    const fileapi::FileSystemOperation::StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CheckIfDirectoryExistsOnIOThread,
                 file_system_context,
                 url,
                 google_apis::CreateRelayCallback(callback)));
}

}  // namespace

GURL GetFileBrowserExtensionUrl() {
  return GURL(kFileBrowserExtensionUrl);
}

GURL GetFileBrowserUrl() {
  return GURL(kBaseFileBrowserUrl);
}

GURL GetMediaPlayerUrl() {
  return GURL(kMediaPlayerUrl);
}

GURL GetVideoPlayerUrl(const GURL& source_url) {
  return GURL(kVideoPlayerUrl + std::string("?") + source_url.spec());
}

bool ConvertFileToFileSystemUrl(Profile* profile,
                                const FilePath& full_file_path,
                                const std::string& extension_id,
                                GURL* url) {
  GURL origin_url = Extension::GetBaseURLFromExtensionId(extension_id);
  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(profile, extension_id,
           full_file_path, &virtual_path)) {
    return false;
  }

  GURL base_url = fileapi::GetFileSystemRootURI(origin_url,
      fileapi::kFileSystemTypeExternal);
  *url = GURL(base_url.spec() + virtual_path.value());
  return true;
}

bool ConvertFileToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const FilePath& full_file_path,
    FilePath* virtual_path) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  // May be NULL during unit_tests.
  if (!service)
    return false;

  // File browser APIs are ment to be used only from extension context, so the
  // extension's site is the one in whose file system context the virtual path
  // should be found.
  GURL site = service->GetSiteForExtensionId(extension_id);
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_provider();
  if (!provider)
    return false;

  // Find if this file path is managed by the external provider.
  if (!provider->GetVirtualPath(full_file_path, virtual_path))
    return false;

  return true;
}

GURL GetFileBrowserUrlWithParams(
    ui::SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_virtual_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  arg_value.SetString("defaultPath", default_virtual_path.value());
  arg_value.SetString("defaultExtension", default_extension);

  if (file_types) {
    ListValue* types_list = new ListValue();
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      ListValue* extensions_list = new ListValue();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->Append(
            new base::StringValue(file_types->extensions[i][j]));
      }

      DictionaryValue* dict = new DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      // file_type_index is 1-based. 0 means no selection at all.
      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == (i + 1)));

      types_list->Set(i, dict);
    }
    arg_value.Set("typeList", types_list);

    arg_value.SetBoolean("includeAllFiles", file_types->include_all_files);
  }

  // Disable showing GDrive unless it's specifically supported.
  arg_value.SetBoolean("disableGData",
      !file_types || !file_types->support_gdata);

  std::string json_args;
  base::JSONWriter::Write(&arg_value, &json_args);

  // kChromeUIFileManagerURL could not be used since query parameters are not
  // supported for it.
  std::string url = GetFileBrowserUrl().spec() +
                    '?' + net::EscapeUrlEncodedData(json_args, false);
  return GURL(url);
}

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

void ViewRemovableDrive(const FilePath& path) {
  OpenFileBrowser(path, REUSE_ANY_FILE_MANAGER, "auto-open");
}

void OpenActionChoiceDialog(const FilePath& path) {
  const int kDialogWidth = 394;
  // TODO(dgozman): remove 50, which is a title height once popup window
  // will have no title.
  const int kDialogHeight = 316 + 50;

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(profile, kFileBrowserDomain, path,
                                           &virtual_path))
    return;
  std::string url = kActionChoiceUrl;
  url += "#/" + net::EscapeUrlEncodedData(virtual_path.value(), false);
  GURL dialog_url(url);

  const gfx::Size screen = ash::Shell::GetScreen()->GetPrimaryDisplay().size();
  const gfx::Rect bounds((screen.width() - kDialogWidth) / 2,
                         (screen.height() - kDialogHeight) / 2,
                         kDialogWidth,
                         kDialogHeight);

  Browser* browser = GetBrowserForUrl(dialog_url);

  if (!browser) {
    browser = new Browser(
        Browser::CreateParams::CreateForApp(Browser::TYPE_POPUP,
                                            "action_choice",
                                            bounds,
                                            profile));

    chrome::AddSelectedTabWithURL(browser, dialog_url,
                                  content::PAGE_TRANSITION_LINK);
  }
  browser->window()->Show();
}

void ViewItem(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  GURL url;
  if (!ConvertFileToFileSystemUrl(profile, path, kFileBrowserDomain, &url)) {
    ShowWarningMessageBox(profile, path);
    return;
  }

  GURL site = extensions::ExtensionSystem::Get(profile)->extension_service()->
      GetSiteForExtensionId(kFileBrowserDomain);
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartitionForSite(profile, site)->
      GetFileSystemContext();

  CheckIfDirectoryExists(file_system_context, url,
                         base::Bind(&ContinueViewItem, profile, path));
}

void ShowFileInFolder(const FilePath& path) {
  // This action changes the selection so we do not reuse existing tabs.
  OpenFileBrowser(path, REUSE_NEVER, "select");
}

bool ExecuteBuiltinHandler(Browser* browser, const FilePath& path,
    const std::string& internal_task_id) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = browser->profile();
  std::string file_extension = path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsSupportedBrowserExtension(file_extension.data()) ||
      ShouldBeOpenedWithPdfPlugin(profile, file_extension.data())) {
    GURL page_url = net::FilePathToFileURL(path);
    // Override gdata resource to point to internal handler instead of file:
    // URL.
    if (drive::util::GetSpecialRemoteRootPath().IsParent(path)) {
      drive::DriveSystemService* system_service =
          drive::DriveSystemServiceFactory::GetForProfile(profile);
      if (!system_service)
        return false;

      // Open the file once the file is found.
      system_service->file_system()->GetEntryInfoByPath(
          drive::util::ExtractDrivePath(path),
          base::Bind(&OnDriveFileFound, profile, path, drive::REGULAR_FILE));
      return true;
    }
    OpenNewTab(page_url, NULL);
    return true;
  }

  if (IsSupportedGDocsExtension(file_extension.data())) {
    if (drive::util::GetSpecialRemoteRootPath().IsParent(path)) {
      // The file is on Google Docs. Get the Docs from the Drive service.
      drive::DriveSystemService* system_service =
          drive::DriveSystemServiceFactory::GetForProfile(profile);
      if (!system_service)
        return false;

      system_service->file_system()->GetEntryInfoByPath(
          drive::util::ExtractDrivePath(path),
          base::Bind(&OnDriveFileFound, profile, path,
                     drive::HOSTED_DOCUMENT));
    } else {
      // The file is local (downloaded from an attachment or otherwise copied).
      // Parse the file to extract the Docs url and open this url.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&ReadUrlFromGDocOnFileThread, path));
    }
    return true;
  }

  if (!IsFileManagerPackaged()) {
    if (internal_task_id == kFileBrowserPlayTaskId) {
      GURL url;
      if (!ConvertFileToFileSystemUrl(profile, path, kFileBrowserDomain, &url))
        return false;
      MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
      mediaplayer->PopupMediaPlayer();
      mediaplayer->ForcePlayMediaURL(url);
      return true;
    }
    if (internal_task_id == kFileBrowserWatchTaskId) {
      GURL url;
      if (!ConvertFileToFileSystemUrl(profile, path, kFileBrowserDomain, &url))
        return false;

      ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
      if (!service)
        return false;

      const extensions::Extension* extension =
        service->GetExtensionById(kFileBrowserDomain, false);
      if (!extension)
        return false;

      application_launch::LaunchParams params(profile, extension,
                                              extension_misc::LAUNCH_WINDOW,
                                              NEW_FOREGROUND_TAB);
      params.override_url = GetVideoPlayerUrl(url);
      application_launch::OpenApplication(params);
      return true;
    }
  }

  if (IsCRXFile(file_extension.data())) {
    if (drive::util::IsUnderDriveMountPoint(path)) {
      drive::DriveSystemService* system_service =
          drive::DriveSystemServiceFactory::GetForProfile(profile);
      if (!system_service)
        return false;
      system_service->file_system()->GetFileByPath(
          drive::util::ExtractDrivePath(path),
          base::Bind(&OnCRXDownloadCallback, browser),
          google_apis::GetContentCallback());
    } else {
      InstallCRX(browser, path);
    }
    return true;
  }

  // Unknown file type. Record UMA and show an error message.
  size_t extension_index = UMAExtensionIndex(file_extension.data(),
                                             kUMATrackingExtensions,
                                             arraysize(kUMATrackingExtensions));
  UMA_HISTOGRAM_ENUMERATION("FileBrowser.OpeningFileType",
                            extension_index,
                            arraysize(kUMATrackingExtensions) - 1);
  return false;
}

// If pdf plugin is enabled, we should open pdf files in a tab.
bool ShouldBeOpenedWithPdfPlugin(Profile* profile, const char* file_extension) {
  if (base::strcasecmp(file_extension, kPdfExtension) != 0)
    return false;

  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);

  content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(pdf_path);
  if (!pepper_info)
    return false;

  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (!plugin_prefs)
    return false;

  return plugin_prefs->IsPluginEnabled(pepper_info->ToWebPluginInfo());
}

ListValue* ProgressStatusVectorToListValue(
    Profile* profile,
    const std::string& extension_id,
    const google_apis::OperationProgressStatusList& list) {
  scoped_ptr<ListValue> result_list(new ListValue());
  for (google_apis::OperationProgressStatusList::const_iterator iter =
           list.begin();
       iter != list.end(); ++iter) {
    result_list->Append(
        ProgessStatusToDictionaryValue(profile, extension_id, *iter));
  }
  return result_list.release();
}

}  // namespace file_manager_util
