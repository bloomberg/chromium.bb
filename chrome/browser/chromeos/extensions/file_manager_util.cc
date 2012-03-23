// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/extensions/file_manager_util.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/simple_message_box.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
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
    ".bmp", ".jpg", ".jpeg", ".png", ".webp", ".gif", ".txt", ".html", ".htm"
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

void ViewRemovableDrive(const FilePath& dir) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(browser->profile(), dir,
                                           &virtual_path)) {
    return;
  }

  DictionaryValue arg_value;
  arg_value.SetBoolean("mountTriggered", true);

  std::string json_args;
  base::JSONWriter::Write(&arg_value, &json_args);

  std::string url = chrome::kChromeUIFileManagerURL;
  url += "?" + json_args + "#/" +
      net::EscapeUrlEncodedData(virtual_path.value(), false);

  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  browser->ShowSingletonTabRespectRef(GURL(url));
}

void OpenFileBrowser(const FilePath& full_path) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(browser->profile(), full_path,
                                           &virtual_path)) {
    return;
  }

  std::string url = chrome::kChromeUIFileManagerURL;
  url += "#/" + net::EscapeUrlEncodedData(virtual_path.value(), false);

  content::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  browser->ShowSingletonTabRespectRef(GURL(url));
}

void ViewFolder(const FilePath& dir) {
  OpenFileBrowser(dir);
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
  virtual Browser* browser() { return BrowserList::GetLastActive(); }
  virtual void Done(bool) {}
};

bool TryOpeningFileBrowser(const FilePath& full_path) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  Profile* profile = browser->profile();

  GURL url;
  if (!ConvertFileToFileSystemUrl(profile, full_path,
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
      OpenFileBrowser(full_path);
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

void ViewFile(const FilePath& full_path, bool enqueue) {
  if (!TryOpeningFileBrowser(full_path) && !TryViewingFile(full_path)) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return;
    browser::ShowErrorBox(
        browser->window()->GetNativeHandle(),
        l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_ERROR_VIEWING_FILE_TITLE,
            UTF8ToUTF16(full_path.BaseName().value())),
        l10n_util::GetStringUTF16(
            IDS_FILE_BROWSER_ERROR_VIEWING_FILE));
  }
}

bool TryViewingFile(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // There is nothing we can do if the browser is not present.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return true;

  std::string file_extension = full_path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsSupportedBrowserExtension(file_extension.data()) ||
      ShouldBeOpenedWithPdfPlugin(file_extension.data())) {
    browser->AddSelectedTabWithURL(net::FilePathToFileURL(full_path),
                                   content::PAGE_TRANSITION_LINK);
    return true;
  }
#if defined(OS_CHROMEOS)
  if (IsSupportedAVExtension(file_extension.data())) {
    MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
    mediaplayer->PopupMediaPlayer(browser);
    mediaplayer->ForcePlayMediaFile(browser->profile(), full_path);
    return true;
  }
#endif  // OS_CHROMEOS

  if (IsCRXFile(file_extension.data())) {
    InstallCRX(browser->profile(), full_path);
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

void InstallCRX(Profile* profile, const FilePath& full_path) {
  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);
  if (!service)
    return;

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(service,
                                            new ExtensionInstallUI(profile)));
  installer->set_is_gallery_install(false);
  installer->set_allow_silent_install(false);
  installer->InstallCrx(full_path);
}

// If pdf plugin is enabled, we should open pdf files in a tab.
bool ShouldBeOpenedWithPdfPlugin(const char* file_extension) {
  if (base::strcasecmp(file_extension, kPdfExtension) != 0)
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);

  webkit::WebPluginInfo plugin;
  if (!PluginService::GetInstance()->GetPluginInfoByPath(pdf_path, &plugin))
    return false;

  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser->profile());
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
