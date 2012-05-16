// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/extensions/file_manager_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/media/media_player.h"
#endif

using base::DictionaryValue;
using base::ListValue;
using content::BrowserContext;
using content::BrowserThread;
using content::PluginService;
using content::UserMetricsAction;
using file_handler_util::FileTaskExecutor;
using gdata::GDataOperationRegistry;

#define FILEBROWSER_EXTENSON_ID "hhaomjibdihmijegdhdafkllkbggdgoj"
const char kFileBrowserDomain[] = FILEBROWSER_EXTENSON_ID;

const char kFileBrowserGalleryTaskId[] = "gallery";
const char kFileBrowserMountArchiveTaskId[] = "mount-archive";

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
const char kMediaPlayerPlaylistUrl[] = FILEBROWSER_URL("playlist.html");
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
    ".mhtml", ".mht"
};
// List of file extension that can be handled with the media player.
const char* kAVExtensions[] = {
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
    ".mp3", ".m4a",
#endif
    ".flac", ".ogm", ".ogg", ".oga", ".wav",
/* TODO(zelidrag): Add unsupported ones as we enable them:
    ".mkv", ".divx", ".xvid", ".wmv", ".asf", ".mpeg", ".mpg",
    ".wma", ".aiff",
*/
};

// Keep in sync with 'open-hosted' task handler in the File Browser manifest.
const char* kGDocsExtensions[] = {
    ".gdoc", ".gsheet", ".gslides", ".gdraw", ".gtable"
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

bool IsSupportedAVExtension(const char* file_extension) {
  for (size_t i = 0; i < arraysize(kAVExtensions); i++) {
    if (base::strcasecmp(file_extension, kAVExtensions[i]) == 0) {
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
    SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case SelectFileDialog::SELECT_FOLDER:
      type_str = "folder";
      break;

    case SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}

DictionaryValue* ProgessStatusToDictionaryValue(
    Profile* profile,
    const GURL& origin_url,
    const GDataOperationRegistry::ProgressStatus& status) {
  scoped_ptr<DictionaryValue> result(new DictionaryValue());
  GURL file_url;
  if (file_manager_util::ConvertFileToFileSystemUrl(profile,
          gdata::util::GetSpecialRemoteRootPath().Append(
              FilePath(status.file_path)),
          origin_url,
          &file_url)) {
    result->SetString("fileUrl", file_url.spec());
  }

  result->SetString("transferState",
      GDataOperationRegistry::OperationTransferStateToString(
          status.transfer_state));
  result->SetString("transferType",
      GDataOperationRegistry::OperationTypeToString(status.operation_type));
  result->SetInteger("processed", static_cast<int>(status.progress_current));
  result->SetInteger("total", static_cast<int>(status.progress_total));
  return result.release();
}

void OpenNewTab(const GURL& url, Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Browser* browser = browser::FindOrCreateTabbedBrowser(
      profile ? profile : ProfileManager::GetDefaultProfileOrOffTheRecord());
  browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_LINK);
  // If the current browser is not tabbed then the new tab will be created
  // in a different browser. Make sure it is visible.
  browser->window()->Show();
}

// Shows a warning message box saying that the file could not be opened.
void ShowWarningMessageBox(Profile* profile, const FilePath& path) {
  // TODO: if FindOrCreateTabbedBrowser creates a new browser the returned
  // browser is leaked.
  Browser* browser = browser::FindOrCreateTabbedBrowser(profile);
  browser::ShowMessageBox(
      browser->window()->GetNativeHandle(),
      l10n_util::GetStringFUTF16(
          IDS_FILE_BROWSER_ERROR_VIEWING_FILE_TITLE,
          UTF8ToUTF16(path.BaseName().value())),
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_VIEWING_FILE),
      browser::MESSAGE_BOX_TYPE_WARNING);
}

// Called when a file on GData was found. Opens the file found at |file_path|
// in a new tab with a URL computed based on the |file_type|
void OnGDataFileFound(Profile* profile,
                      const FilePath& file_path,
                      gdata::GDataFileType file_type,
                      base::PlatformFileError error,
                      scoped_ptr<gdata::GDataFileProto> file_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == base::PLATFORM_FILE_OK) {
    GURL page_url;
    if (file_type == gdata::REGULAR_FILE) {
      page_url = gdata::util::GetFileResourceUrl(
          file_proto->gdata_entry().resource_id(),
          file_proto->gdata_entry().file_name());
    } else if (file_type == gdata::HOSTED_DOCUMENT) {
      page_url = GURL(file_proto->alternate_url());
    } else {
      NOTREACHED();
    }
    OpenNewTab(page_url, profile);
  } else {
    ShowWarningMessageBox(profile, file_path);
  }
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

GURL GetMediaPlayerPlaylistUrl() {
  return GURL(kMediaPlayerPlaylistUrl);
}

bool ConvertFileToFileSystemUrl(
    Profile* profile, const FilePath& full_file_path, const GURL& origin_url,
    GURL* url) {
  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(profile, full_file_path,
                                           &virtual_path)) {
    return false;
  }

  GURL base_url = fileapi::GetFileSystemRootURI(origin_url,
      fileapi::kFileSystemTypeExternal);
  *url = GURL(base_url.spec() + virtual_path.value());
  return true;
}

bool ConvertFileToRelativeFileSystemPath(
    Profile* profile, const FilePath& full_file_path, FilePath* virtual_path) {
  fileapi::ExternalFileSystemMountPointProvider* provider =
      BrowserContext::GetFileSystemContext(profile)->external_provider();
  if (!provider)
    return false;

  // Find if this file path is managed by the external provider.
  if (!provider->GetVirtualPath(full_file_path, virtual_path))
    return false;

  return true;
}

GURL GetFileBrowserUrlWithParams(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_virtual_path,
    const SelectFileDialog::FileTypeInfo* file_types,
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
        extensions_list->Set(
            i, Value::CreateStringValue(file_types->extensions[i][j]));
      }

      DictionaryValue* dict = new DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == i));

      types_list->Set(i, dict);
    }
    arg_value.Set("typeList", types_list);
  }

  std::string json_args;
  base::JSONWriter::Write(&arg_value, &json_args);

  // kChromeUIFileManagerURL could not be used since query parameters are not
  // supported for it.
  std::string url = GetFileBrowserUrl().spec() +
                    '?' + net::EscapeUrlEncodedData(json_args, false);
  return GURL(url);
}

string16 GetTitleFromType(SelectFileDialog::Type dialog_type) {
  string16 title;
  switch (dialog_type) {
    case SelectFileDialog::SELECT_NONE:
      // Full page file manager doesn't need a title.
      break;

    case SelectFileDialog::SELECT_FOLDER:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_FOLDER_TITLE);
      break;

    case SelectFileDialog::SELECT_SAVEAS_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_SAVEAS_FILE_TITLE);
      break;

    case SelectFileDialog::SELECT_OPEN_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_OPEN_FILE_TITLE);
      break;

    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_OPEN_MULTI_FILE_TITLE);
      break;

    default:
      NOTREACHED();
  }

  return title;
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
      content::WebContents* web_contents =
          tab_strip->GetTabContentsAt(idx)->web_contents();
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

void OpenFileBrowser(const FilePath& path,
                     TAB_REUSE_MODE mode,
                     const std::string& flag_name) {
  if (FileManageTabExists(path, mode))
    return;

  Browser* last_active = BrowserList::GetLastActive();
  Profile* profile = last_active ? last_active->profile() :
      ProfileManager::GetDefaultProfileOrOffTheRecord();

  std::string url = chrome::kChromeUIFileManagerURL;
  if (flag_name.size()) {
    DictionaryValue arg_value;
    arg_value.SetBoolean(flag_name, "true");
    std::string query;
    base::JSONWriter::Write(&arg_value, &query);
    url += "?" + net::EscapeUrlEncodedData(query, false);
  }
  if (!path.empty()) {
    FilePath virtual_path;
    if (!ConvertFileToRelativeFileSystemPath(profile, path, &virtual_path))
      return;
    url += "#/" + net::EscapeUrlEncodedData(virtual_path.value(), false);
  }

  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  const Extension* extension =
      service->GetExtensionById(kFileBrowserDomain, false);
  if (!extension)
    return;

  extension_misc::LaunchContainer launch_container =
      service->extension_prefs()->
          GetLaunchContainer(extension, ExtensionPrefs::LAUNCH_DEFAULT);

  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  Browser::OpenApplication(
      profile, extension, launch_container, GURL(url), NEW_FOREGROUND_TAB);
}

void ViewRemovableDrive(const FilePath& path) {
  OpenFileBrowser(path, REUSE_ANY_FILE_MANAGER, "mountTriggered");
}

void ShowFileInFolder(const FilePath& path) {
  // This action changes the selection so we do not reuse existing tabs.
  OpenFileBrowser(path, REUSE_NEVER, "selectOnly");
}

void ViewFolder(const FilePath& path) {
  OpenFileBrowser(path, REUSE_SAME_PATH, std::string());
}

void OpenApplication() {
  OpenFileBrowser(FilePath(), REUSE_NEVER, std::string());
}

class StandaloneExecutor : public FileTaskExecutor {
 public:
  StandaloneExecutor(Profile * profile,
                     const GURL& source_url,
                     const std::string& extension_id,
                     const std::string& action_id)
    : FileTaskExecutor(profile, source_url, extension_id, action_id)
  {}

 protected :
  // FileTaskExecutor overrides.
  virtual Browser* browser() {
    return browser::FindOrCreateTabbedBrowser(profile());
  }
  virtual void Done(bool) {}
};

bool TryOpeningFileBrowser(Profile* profile, const FilePath& path) {
  GURL url;
  if (!ConvertFileToFileSystemUrl(profile, path,
      GetFileBrowserExtensionUrl().GetOrigin(), &url))
    return false;

  const FileBrowserHandler* handler;
  if (!file_handler_util::GetDefaultTask(profile, url, &handler))
    return false;

  std::string extension_id = handler->extension_id();
  std::string action_id = handler->id();
  if (extension_id == kFileBrowserDomain) {
    // Only two of the built-in File Browser tasks require opening the File
    // Browser tab. Others just end up calling TryViewingFile.
    if (action_id == kFileBrowserGalleryTaskId ||
        action_id == kFileBrowserMountArchiveTaskId) {
      // Tab reuse currently does not work for these two tasks.
      // |gallery| tries to put the file url into the tab url but it does not
      // work on Chrome OS.
      // |mount-archive| does not even try.
      OpenFileBrowser(path, REUSE_SAME_PATH, "");
      return true;
    }
  } else {
    // We are executing the task on behalf of File Browser extension.
    const GURL source_url(kBaseFileBrowserUrl);

    // If File Browser has not been open yet then it did not request access
    // to the file system. Do it now.
    fileapi::ExternalFileSystemMountPointProvider* external_provider =
        BrowserContext::GetFileSystemContext(profile)->external_provider();
    if (!external_provider)
      return false;
    external_provider->GrantFullAccessToExtension(source_url.host());

    std::vector<GURL> urls;
    urls.push_back(url);
    scoped_refptr<StandaloneExecutor> executor = new StandaloneExecutor(
        profile, source_url, extension_id, action_id);
    executor->Execute(urls);
    return true;
  }
  return false;
}

void ViewFile(const FilePath& path) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  if (!TryOpeningFileBrowser(profile, path) &&
      !TryViewingFile(profile, path)) {
    ShowWarningMessageBox(profile, path);
  }
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

bool TryViewingFile(Profile* profile, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string file_extension = path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsSupportedBrowserExtension(file_extension.data()) ||
      ShouldBeOpenedWithPdfPlugin(profile, file_extension.data())) {
    GURL page_url = net::FilePathToFileURL(path);
#if defined(OS_CHROMEOS)
    // Override gdata resource to point to internal handler instead of file:
    // URL.
    if (gdata::util::GetSpecialRemoteRootPath().IsParent(path)) {
      gdata::GDataSystemService* system_service =
          gdata::GDataSystemServiceFactory::GetForProfile(profile);
      if (!system_service)
        return false;

      // Open the file once the file is found.
      system_service->file_system()->GetFileInfoByPathAsync(
          gdata::util::ExtractGDataPath(path),
          base::Bind(&OnGDataFileFound, profile, path, gdata::REGULAR_FILE));
      return true;
    }
#endif
    OpenNewTab(page_url, (Profile*)NULL);
    return true;
  }

  if (IsSupportedGDocsExtension(file_extension.data())) {
    if (gdata::util::GetSpecialRemoteRootPath().IsParent(path)) {
      // The file is on Google Docs. Get the Docs from the GData service.
      gdata::GDataSystemService* system_service =
          gdata::GDataSystemServiceFactory::GetForProfile(profile);
      if (!system_service)
        return false;

      system_service->file_system()->GetFileInfoByPathAsync(
          gdata::util::ExtractGDataPath(path),
          base::Bind(&OnGDataFileFound, profile, path,
                     gdata::HOSTED_DOCUMENT));
    } else {
      // The file is local (downloaded from an attachment or otherwise copied).
      // Parse the file to extract the Docs url and open this url.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&ReadUrlFromGDocOnFileThread, path));
    }
    return true;
  }

#if defined(OS_CHROMEOS)
  if (IsSupportedAVExtension(file_extension.data())) {
    GURL url;
    if (!ConvertFileToFileSystemUrl(profile, path,
        GetFileBrowserExtensionUrl().GetOrigin(), &url))
      return false;
    MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
    mediaplayer->PopupMediaPlayer();
    mediaplayer->ForcePlayMediaURL(url);
    return true;
  }
#endif  // OS_CHROMEOS

  if (IsCRXFile(file_extension.data())) {
    InstallCRX(profile, path);
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

void InstallCRX(Profile* profile, const FilePath& path) {
  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);
  if (!service)
    return;

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(service,
                                            new ExtensionInstallUI(profile)));
  installer->set_is_gallery_install(false);
  installer->set_allow_silent_install(false);
  installer->InstallCrx(path);
}

// If pdf plugin is enabled, we should open pdf files in a tab.
bool ShouldBeOpenedWithPdfPlugin(Profile* profile, const char* file_extension) {
  if (base::strcasecmp(file_extension, kPdfExtension) != 0)
    return false;

  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);

  webkit::WebPluginInfo plugin;
  if (!PluginService::GetInstance()->GetPluginInfoByPath(pdf_path, &plugin))
    return false;

  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (!plugin_prefs)
    return false;

  return plugin_prefs->IsPluginEnabled(plugin);
}

ListValue* ProgressStatusVectorToListValue(
    Profile* profile, const GURL& origin_url,
    const std::vector<GDataOperationRegistry::ProgressStatus>& list) {
  scoped_ptr<ListValue> result_list(new ListValue());
  for (std::vector<
          GDataOperationRegistry::ProgressStatus>::const_iterator iter =
              list.begin();
       iter != list.end(); ++iter) {
    result_list->Append(
        ProgessStatusToDictionaryValue(profile, origin_url, *iter));
  }
  return result_list.release();
}

}  // namespace file_manager_util
