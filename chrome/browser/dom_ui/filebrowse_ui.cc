// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/filebrowse_ui.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_file_job.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;
static const char kPropertyPath[] = "path";
static const char kPropertyTitle[] = "title";
static const char kPropertyDirectory[] = "isDirectory";
static const char kPicasawebUserPrefix[] =
    "http://picasaweb.google.com/data/feed/api/user/";
static const char kPicasawebDefault[] = "/albumid/default";
static const char kPicasawebDropBox[] = "/home";
static const char kPicasawebBaseUrl[] = "http://picasaweb.google.com/";
static const char kMediaPath[] = "/media";
static const char kFilebrowseURLHash[] = "chrome://filebrowse#";
static const int kPopupLeft = 0;
static const int kPopupTop = 0;

class FileBrowseUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  FileBrowseUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~FileBrowseUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(FileBrowseUIHTMLSource);
};

class TaskProxy;

// The handler for Javascript messages related to the "filebrowse" view.
class FilebrowseHandler : public net::DirectoryLister::DirectoryListerDelegate,
                          public DOMMessageHandler,
#if defined(OS_CHROMEOS)
                          public chromeos::MountLibrary::Observer,
#endif
                          public base::SupportsWeakPtr<FilebrowseHandler>,
                          public URLFetcher::Delegate,
                          public DownloadManager::Observer,
                          public DownloadItem::Observer {
 public:
  FilebrowseHandler();
  virtual ~FilebrowseHandler();

  // Init work after Attach.
  void Init();

  // DirectoryLister::DirectoryListerDelegate methods:
  virtual void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int error);

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

#if defined(OS_CHROMEOS)
  void MountChanged(chromeos::MountLibrary* obj,
                    chromeos::MountEventType evt,
                    const std::string& path);
#endif

  // DownloadItem::Observer interface
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadFileCompleted(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

  // DownloadManager::Observer interface
  virtual void ModelChanged();

  // Callback for the "getRoots" message.
  void HandleGetRoots(const ListValue* args);

  void GetChildrenForPath(FilePath& path, bool is_refresh);

  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const net::URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

  // Callback for the "getChildren" message.
  void HandleGetChildren(const ListValue* args);
 // Callback for the "refreshDirectory" message.
  void HandleRefreshDirectory(const ListValue* args);
  void HandleIsAdvancedEnabled(const ListValue* args);

  // Callback for the "getMetadata" message.
  void HandleGetMetadata(const ListValue* args);

 // Callback for the "openNewWindow" message.
  void OpenNewFullWindow(const ListValue* args);
  void OpenNewPopupWindow(const ListValue* args);

  // Callback for the "uploadToPicasaweb" message.
  void UploadToPicasaweb(const ListValue* args);

  // Callback for the "getDownloads" message.
  void HandleGetDownloads(const ListValue* args);

  void HandleCreateNewFolder(const ListValue* args);

  void PlayMediaFile(const ListValue* args);
  void EnqueueMediaFile(const ListValue* args);

  void HandleDeleteFile(const ListValue* args);
  void HandleCopyFile(const ListValue* value);
  void CopyFile(const FilePath& src, const FilePath& dest);
  void DeleteFile(const FilePath& path);
  void FireDeleteComplete(const FilePath& path);
  void FireCopyComplete(const FilePath& src, const FilePath& dest);

  void HandlePauseToggleDownload(const ListValue* args);

  void HandleCancelDownload(const ListValue* args);
  void HandleAllowDownload(const ListValue* args);

  void ReadInFile();
  void FireUploadComplete();

  void SendPicasawebRequest();

  // Callback for the "validateSavePath" message.
  void HandleValidateSavePath(const ListValue* args);

  // Validate a save path on file thread.
  void ValidateSavePathOnFileThread(const FilePath& save_path);

  // Fire save path validation result to JS onValidatedSavePath.
  void FireOnValidatedSavePathOnUIThread(bool valid, const FilePath& save_path);

 private:

  // Retrieves downloads from the DownloadManager and updates the page.
  void UpdateDownloadList();

  void OpenNewWindow(const ListValue* args, bool popup);

  // Clear all download items and their observers.
  void ClearDownloadItems();

  // Send the current list of downloads to the page.
  void SendCurrentDownloads();

  void SendNewDownload(DownloadItem* download);

  scoped_ptr<ListValue> filelist_value_;
  FilePath currentpath_;
  Profile* profile_;
  TabContents* tab_contents_;
  std::string current_file_contents_;
  std::string current_file_uploaded_;
  int upload_response_code_;
  TaskProxy* current_task_;
  scoped_refptr<net::DirectoryLister> lister_;
  bool is_refresh_;
  scoped_ptr<URLFetcher> fetch_;

  DownloadManager* download_manager_;
  typedef std::vector<DownloadItem*> DownloadList;
  DownloadList active_download_items_;
  DownloadList download_items_;
  bool got_first_download_list_;
  DISALLOW_COPY_AND_ASSIGN(FilebrowseHandler);
};

class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  TaskProxy(const base::WeakPtr<FilebrowseHandler>& handler,
            const FilePath& path, const FilePath& dest)
      : handler_(handler),
        src_(path),
        dest_(dest) {}
  TaskProxy(const base::WeakPtr<FilebrowseHandler>& handler,
            const FilePath& path)
      : handler_(handler),
        src_(path) {}
  void ReadInFileProxy() {
    if (handler_) {
      handler_->ReadInFile();
    }
  }
  void DeleteFetcher(URLFetcher* fetch) {
    delete fetch;
  }
  void SendPicasawebRequestProxy() {
    if (handler_) {
      handler_->SendPicasawebRequest();
    }
  }
  void FireUploadCompleteProxy() {
    if (handler_) {
      handler_->FireUploadComplete();
    }
  }

  void DeleteFileProxy() {
    if (handler_) {
      handler_->DeleteFile(src_);
    }
  }

  void CopyFileProxy() {
    if (handler_) {
      handler_->CopyFile(src_, dest_);
    }
  }

  void FireDeleteCompleteProxy() {
    if (handler_) {
      handler_->FireDeleteComplete(src_);
    }
  }
  void FireCopyCompleteProxy() {
    if (handler_) {
      handler_->FireCopyComplete(src_, dest_);
    }
  }

  void ValidateSavePathOnFileThread() {
    if (handler_)
      handler_->ValidateSavePathOnFileThread(src_);
  }
  void FireOnValidatedSavePathOnUIThread(bool valid,
                                         const FilePath& save_path) {
    if (handler_)
      handler_->FireOnValidatedSavePathOnUIThread(valid, save_path);
  }

 private:
  base::WeakPtr<FilebrowseHandler> handler_;
  FilePath src_;
  FilePath dest_;
  friend class base::RefCountedThreadSafe<TaskProxy>;
  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
};


////////////////////////////////////////////////////////////////////////////////
//
// FileBrowseHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

FileBrowseUIHTMLSource::FileBrowseUIHTMLSource()
    : DataSource(chrome::kChromeUIFileBrowseHost, MessageLoop::current()) {
}

void FileBrowseUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_off_the_record,
                                              int request_id) {
  DictionaryValue localized_strings;
  // TODO(dhg): Add stirings to localized strings, also add more strings
  // that are currently hardcoded.
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_TITLE));
  localized_strings.SetString("pause",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_PAUSE));
  localized_strings.SetString("resume",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_RESUME));
  localized_strings.SetString("scanning",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_SCANNING));
  localized_strings.SetString("confirmdelete",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_CONFIRM_DELETE));
  localized_strings.SetString("confirmyes",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_CONFIRM_YES));
  localized_strings.SetString("confirmcancel",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_CONFIRM_CANCEL));
  localized_strings.SetString("allowdownload",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_CONFIRM_DOWNLOAD));
  localized_strings.SetString("filenameprompt",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_PROMPT_FILENAME));
  localized_strings.SetString("save",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_SAVE));
  localized_strings.SetString("newfolder",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_NEW_FOLDER));
  localized_strings.SetString("open",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_OPEN));
  localized_strings.SetString("picasaweb",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_UPLOAD_PICASAWEB));
  localized_strings.SetString("flickr",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_UPLOAD_FLICKR));
  localized_strings.SetString("email",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_UPLOAD_EMAIL));
  localized_strings.SetString("delete",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_DELETE));
  localized_strings.SetString("enqueue",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_ENQUEUE));
  localized_strings.SetString("mediapath", kMediaPath);
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }
  // TODO(viettrungluu): this is wrong -- FilePath's need not be Unicode.
  localized_strings.SetString("downloadpath", default_download_path.value());
  localized_strings.SetString("error_unknown_file_type",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_ERROR_UNKNOWN_FILE_TYPE));
  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece filebrowse_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_FILEBROWSE_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      filebrowse_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// FilebrowseHandler
//
////////////////////////////////////////////////////////////////////////////////
FilebrowseHandler::FilebrowseHandler()
    : profile_(NULL),
      tab_contents_(NULL),
      is_refresh_(false),
      fetch_(NULL),
      download_manager_(NULL),
      got_first_download_list_(false) {
  lister_ = NULL;
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  lib->AddObserver(this);
#endif
}

FilebrowseHandler::~FilebrowseHandler() {
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  lib->RemoveObserver(this);
#endif
  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
  }

  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
  URLFetcher* fetch = fetch_.release();
  if (fetch) {
    TaskProxy* task = new TaskProxy(AsWeakPtr(), currentpath_);
    task->AddRef();
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            task, &TaskProxy::DeleteFetcher, fetch));
  }
}

DOMMessageHandler* FilebrowseHandler::Attach(DOMUI* dom_ui) {
  // Create our favicon data source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new DOMUIFavIconSource(dom_ui->GetProfile()))));
  profile_ = dom_ui->GetProfile();
  tab_contents_ = dom_ui->tab_contents();
  return DOMMessageHandler::Attach(dom_ui);
}

void FilebrowseHandler::Init() {
  download_manager_ = profile_->GetDownloadManager();
  download_manager_->AddObserver(this);
  TaskProxy* task = new TaskProxy(AsWeakPtr(), currentpath_);
  task->AddRef();
  current_task_ = task;
  static bool sent_request = false;
  if (!sent_request) {
    // If we have not sent a request before, we should do one in order to
    // ensure that we have the correct cookies. This is for uploads.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            task, &TaskProxy::SendPicasawebRequestProxy));
    sent_request = true;
  }
}

void FilebrowseHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getRoots",
      NewCallback(this, &FilebrowseHandler::HandleGetRoots));
  dom_ui_->RegisterMessageCallback("getChildren",
      NewCallback(this, &FilebrowseHandler::HandleGetChildren));
  dom_ui_->RegisterMessageCallback("getMetadata",
      NewCallback(this, &FilebrowseHandler::HandleGetMetadata));
  dom_ui_->RegisterMessageCallback("openNewPopupWindow",
      NewCallback(this, &FilebrowseHandler::OpenNewPopupWindow));
  dom_ui_->RegisterMessageCallback("openNewFullWindow",
      NewCallback(this, &FilebrowseHandler::OpenNewFullWindow));
  dom_ui_->RegisterMessageCallback("uploadToPicasaweb",
      NewCallback(this, &FilebrowseHandler::UploadToPicasaweb));
  dom_ui_->RegisterMessageCallback("getDownloads",
      NewCallback(this, &FilebrowseHandler::HandleGetDownloads));
  dom_ui_->RegisterMessageCallback("createNewFolder",
      NewCallback(this, &FilebrowseHandler::HandleCreateNewFolder));
  dom_ui_->RegisterMessageCallback("playMediaFile",
      NewCallback(this, &FilebrowseHandler::PlayMediaFile));
  dom_ui_->RegisterMessageCallback("enqueueMediaFile",
      NewCallback(this, &FilebrowseHandler::EnqueueMediaFile));
  dom_ui_->RegisterMessageCallback("pauseToggleDownload",
      NewCallback(this, &FilebrowseHandler::HandlePauseToggleDownload));
  dom_ui_->RegisterMessageCallback("deleteFile",
      NewCallback(this, &FilebrowseHandler::HandleDeleteFile));
  dom_ui_->RegisterMessageCallback("copyFile",
      NewCallback(this, &FilebrowseHandler::HandleCopyFile));
  dom_ui_->RegisterMessageCallback("cancelDownload",
      NewCallback(this, &FilebrowseHandler::HandleCancelDownload));
  dom_ui_->RegisterMessageCallback("allowDownload",
      NewCallback(this, &FilebrowseHandler::HandleAllowDownload));
  dom_ui_->RegisterMessageCallback("refreshDirectory",
      NewCallback(this, &FilebrowseHandler::HandleRefreshDirectory));
  dom_ui_->RegisterMessageCallback("isAdvancedEnabled",
      NewCallback(this, &FilebrowseHandler::HandleIsAdvancedEnabled));
  dom_ui_->RegisterMessageCallback("validateSavePath",
      NewCallback(this, &FilebrowseHandler::HandleValidateSavePath));
}


void FilebrowseHandler::FireDeleteComplete(const FilePath& path) {
  // We notify the UI by telling it to refresh its contents.
  FilePath dir_path = path.DirName();
  GetChildrenForPath(dir_path, true);
};

void FilebrowseHandler::FireCopyComplete(const FilePath& src,
                                         const FilePath& dest) {
  // Notify the UI somehow.
  FilePath dir_path = dest.DirName();
  GetChildrenForPath(dir_path, true);
};

void FilebrowseHandler::FireUploadComplete() {
#if defined(OS_CHROMEOS)
  DictionaryValue info_value;
  info_value.SetString("path", current_file_uploaded_);

  std::string username;
  chromeos::UserManager* user_man = chromeos::UserManager::Get();
  username = user_man->logged_in_user().email();

  if (username.empty()) {
    LOG(ERROR) << "Unable to get username";
    return;
  }
  int location = username.find_first_of('@',0);
  if (location <= 0) {
    LOG(ERROR) << "Username not formatted correctly";
    return;
  }
  username = username.erase(username.find_first_of('@',0));
  std::string picture_url = kPicasawebBaseUrl;
  picture_url += username;
  picture_url += kPicasawebDropBox;
  info_value.SetString("url", picture_url);
  info_value.SetInteger("status_code", upload_response_code_);
  dom_ui_->CallJavascriptFunction(L"uploadComplete", info_value);
#endif
}

#if defined(OS_CHROMEOS)
void FilebrowseHandler::MountChanged(chromeos::MountLibrary* obj,
                                     chromeos::MountEventType evt,
                                     const std::string& path) {
  if (evt == chromeos::DISK_REMOVED ||
      evt == chromeos::DISK_CHANGED) {
    dom_ui_->CallJavascriptFunction(L"rootsChanged");
  }
}
#endif

void FilebrowseHandler::OnURLFetchComplete(const URLFetcher* source,
                                           const GURL& url,
                                           const net::URLRequestStatus& status,
                                           int response_code,
                                           const ResponseCookies& cookies,
                                           const std::string& data) {
  upload_response_code_ = response_code;
  VLOG(1) << "Response code: " << response_code;
  VLOG(1) << "Request url: " << url;
  if (StartsWithASCII(url.spec(), kPicasawebUserPrefix, true)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(current_task_, &TaskProxy::FireUploadCompleteProxy));
  }
  fetch_.reset();
}

void FilebrowseHandler::HandleGetRoots(const ListValue* args) {
  ListValue results_value;
  DictionaryValue info_value;
  // TODO(dhg): add other entries, make this more general
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  const chromeos::MountLibrary::DiskVector& disks = lib->disks();

  for (size_t i = 0; i < disks.size(); ++i) {
    if (!disks[i].mount_path.empty()) {
      DictionaryValue* page_value = new DictionaryValue();
      page_value->SetString(kPropertyPath, disks[i].mount_path);
      FilePath currentpath(disks[i].mount_path);
      std::string filename;
      filename = currentpath.BaseName().value();
      page_value->SetString(kPropertyTitle, filename);
      page_value->SetBoolean(kPropertyDirectory, true);
      results_value.Append(page_value);
    }
  }
#else
  DictionaryValue* page_value = new DictionaryValue();
  page_value->SetString(kPropertyPath, "/media");
  page_value->SetString(kPropertyTitle, "Removeable");
  page_value->SetBoolean(kPropertyDirectory, true);

  results_value.Append(page_value);
#endif
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }

  DictionaryValue* download_value = new DictionaryValue();
  download_value->SetString(kPropertyPath, default_download_path.value());
  download_value->SetString(kPropertyTitle, "File Shelf");
  download_value->SetBoolean(kPropertyDirectory, true);

  results_value.Append(download_value);

  info_value.SetString("functionCall", "getRoots");
  info_value.SetString(kPropertyPath, "");
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, results_value);
}

void FilebrowseHandler::HandleCreateNewFolder(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = WideToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);

  if (!file_util::CreateDirectory(currentpath))
    LOG(ERROR) << "unable to create directory";
#endif
}

void FilebrowseHandler::PlayMediaFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string url = WideToUTF8(ExtractStringValue(args));
  GURL gurl(url);

  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
  mediaplayer->ForcePlayMediaURL(gurl, browser);
#endif
}

void FilebrowseHandler::EnqueueMediaFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string url = WideToUTF8(ExtractStringValue(args));
  GURL gurl(url);

  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
  mediaplayer->EnqueueMediaURL(gurl, browser);
#endif
}

void FilebrowseHandler::HandleIsAdvancedEnabled(const ListValue* args) {
#if defined(OS_CHROMEOS)
  bool is_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAdvancedFileSystem);
  bool mp_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaPlayer);
  DictionaryValue info_value;
  info_value.SetBoolean("enabled", is_enabled);
  info_value.SetBoolean("mpEnabled", mp_enabled);
  dom_ui_->CallJavascriptFunction(L"enabledResult",
                                  info_value);

#endif
}

void FilebrowseHandler::HandleRefreshDirectory(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = WideToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);
  GetChildrenForPath(currentpath, true);
#endif
}

void FilebrowseHandler::HandlePauseToggleDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= (int)active_download_items_.size()) {
    return;
  }
  DownloadItem* item = active_download_items_[id];
  item->TogglePause();
#endif
}

void FilebrowseHandler::HandleAllowDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= (int)active_download_items_.size()) {
    return;
  }

  DownloadItem* item = active_download_items_[id];
  download_manager_->DangerousDownloadValidated(item);
#endif
}

void FilebrowseHandler::HandleCancelDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= (int)active_download_items_.size()) {
    return;
  }
  DownloadItem* item = active_download_items_[id];
  FilePath path = item->full_path();
  item->Cancel(true);
  FilePath dir_path = path.DirName();
  item->Remove(true);
  GetChildrenForPath(dir_path, true);
#endif
}

void FilebrowseHandler::OpenNewFullWindow(const ListValue* args) {
  OpenNewWindow(args, false);
}

void FilebrowseHandler::OpenNewPopupWindow(const ListValue* args) {
  OpenNewWindow(args, true);
}

void FilebrowseHandler::OpenNewWindow(const ListValue* args, bool popup) {
  std::string url = WideToUTF8(ExtractStringValue(args));
  Browser* browser = popup ?
      Browser::CreateForType(Browser::TYPE_APP_PANEL, profile_) :
      BrowserList::GetLastActive();
  browser::NavigateParams params(browser, GURL(url), PageTransition::LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  // TODO(beng): The following two calls should be automatic by Navigate().
  if (popup) {
    // TODO(dhg): Remove these from being hardcoded. Allow javascript
    // to specify.
    params.browser->window()->SetBounds(gfx::Rect(0, 0, 400, 300));
  }
  params.browser->window()->Show();
}

void FilebrowseHandler::SendPicasawebRequest() {
#if defined(OS_CHROMEOS)
  chromeos::UserManager* user_man = chromeos::UserManager::Get();
  std::string username = user_man->logged_in_user().email();

  if (username.empty()) {
    LOG(ERROR) << "Unable to get username";
    return;
  }

  fetch_.reset(URLFetcher::Create(0,
                                  GURL(kPicasawebBaseUrl),
                                  URLFetcher::GET,
                                  this));
  fetch_->set_request_context(profile_->GetRequestContext());
  fetch_->Start();
#endif
}

void FilebrowseHandler::ReadInFile() {
#if defined(OS_CHROMEOS)
  // Get the users username
  std::string username;
  chromeos::UserManager* user_man = chromeos::UserManager::Get();
  username = user_man->logged_in_user().email();

  if (username.empty()) {
    LOG(ERROR) << "Unable to get username";
    return;
  }
  int location = username.find_first_of('@',0);
  if (location <= 0) {
    LOG(ERROR) << "Username not formatted correctly";
    return;
  }
  username = username.erase(username.find_first_of('@',0));
  std::string url = kPicasawebUserPrefix;
  url += username;
  url += kPicasawebDefault;

  FilePath currentpath(current_file_uploaded_);
  // Get the filename
  std::string filename;
  filename = currentpath.BaseName().value();
  std::string filecontents;
  if (!file_util::ReadFileToString(currentpath, &filecontents)) {
    LOG(ERROR) << "Unable to read this file:" << currentpath.value();
    return;
  }
  fetch_.reset(URLFetcher::Create(0,
                                  GURL(url),
                                  URLFetcher::POST,
                                  this));
  fetch_->set_upload_data("image/jpeg", filecontents);
  // Set the filename on the server
  std::string slug = "Slug: ";
  slug += filename;
  fetch_->set_extra_request_headers(slug);
  fetch_->set_request_context(profile_->GetRequestContext());
  fetch_->Start();
#endif
}

// This is just a prototype for allowing generic uploads to various sites
// TODO(dhg): Remove this and implement general upload.
void FilebrowseHandler::UploadToPicasaweb(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string search_string = WideToUTF8(ExtractStringValue(args));
  current_file_uploaded_ = search_string;
  //  ReadInFile();
  FilePath current_path(search_string);
  TaskProxy* task = new TaskProxy(AsWeakPtr(), current_path);
  task->AddRef();
  current_task_ = task;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          task, &TaskProxy::ReadInFileProxy));
#endif
}

void FilebrowseHandler::GetChildrenForPath(FilePath& path, bool is_refresh) {
  filelist_value_.reset(new ListValue());
  currentpath_ = path;

  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }

  is_refresh_ = is_refresh;

#if defined(OS_CHROMEOS)
  // Don't allow listing files in inaccessible dirs.
  if (net::URLRequestFileJob::AccessDisabled(path))
    return;
#endif  // OS_CHROMEOS

  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }
  if (currentpath_ == default_download_path) {
    lister_ = new net::DirectoryLister(currentpath_,
                                       false,
                                       net::DirectoryLister::DATE,
                                       this);
  } else {
    lister_ = new net::DirectoryLister(currentpath_, this);
  }
  lister_->Start();
}

void FilebrowseHandler::HandleGetChildren(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = WideToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);
  filelist_value_.reset(new ListValue());

  GetChildrenForPath(currentpath, false);
#endif
}

void FilebrowseHandler::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
#if defined(OS_WIN)
  if (data.info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
    return;
  }
#elif defined(OS_POSIX)
  if (data.info.filename[0] == '.') {
    return;
  }
#endif

  DictionaryValue* file_value = new DictionaryValue();

#if defined(OS_WIN)
  int64 size = (static_cast<int64>(data.info.nFileSizeHigh) << 32) |
      data.info.nFileSizeLow;
  file_value->SetString(kPropertyTitle, data.info.cFileName);
  file_value->SetString(kPropertyPath,
                        currentpath_.Append(data.info.cFileName).value());
  file_value->SetBoolean(kPropertyDirectory,
      (data.info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false);
#elif defined(OS_POSIX)
  file_value->SetString(kPropertyTitle, data.info.filename);
  file_value->SetString(kPropertyPath,
                        currentpath_.Append(data.info.filename).value());
  file_value->SetBoolean(kPropertyDirectory, S_ISDIR(data.info.stat.st_mode));
#endif
  filelist_value_->Append(file_value);
}

void FilebrowseHandler::OnListDone(int error) {
  DictionaryValue info_value;
  if (is_refresh_) {
    info_value.SetString("functionCall", "refresh");
  } else {
    info_value.SetString("functionCall", "getChildren");
  }
  info_value.SetString(kPropertyPath, currentpath_.value());
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, *(filelist_value_.get()));
}

void FilebrowseHandler::HandleGetMetadata(const ListValue* args) {
}

void FilebrowseHandler::HandleGetDownloads(const ListValue* args) {
  UpdateDownloadList();
}

void FilebrowseHandler::ModelChanged() {
  if (!currentpath_.empty())
    GetChildrenForPath(currentpath_, true);
  else
    UpdateDownloadList();
}

void FilebrowseHandler::UpdateDownloadList() {
  ClearDownloadItems();

  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(FilePath(), &downloads);

  std::vector<DownloadItem*> new_downloads;
  // Scan for any in progress downloads and add ourself to them as an observer.
  for (DownloadList::iterator it = downloads.begin();
       it != downloads.end(); ++it) {
    DownloadItem* download = *it;
    // We want to know what happens as the download progresses and be notified
    // when the user validates the dangerous download.
    if (download->state() == DownloadItem::IN_PROGRESS ||
        download->safety_state() == DownloadItem::DANGEROUS) {
      download->AddObserver(this);
      active_download_items_.push_back(download);
    }
    DownloadList::iterator item = find(download_items_.begin(),
                                       download_items_.end(),
                                       download);
    if (item == download_items_.end() && got_first_download_list_) {
      SendNewDownload(download);
    }
    new_downloads.push_back(download);
  }
  download_items_.swap(new_downloads);
  got_first_download_list_ = true;
  SendCurrentDownloads();
}

void FilebrowseHandler::SendNewDownload(DownloadItem* download) {
  ListValue results_value;
  results_value.Append(download_util::CreateDownloadItemValue(download, -1));
  dom_ui_->CallJavascriptFunction(L"newDownload", results_value);
}

void FilebrowseHandler::DeleteFile(const FilePath& path) {
  if (!file_util::Delete(path, true)) {
    LOG(ERROR) << "unable to delete directory";
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(current_task_, &TaskProxy::FireDeleteCompleteProxy));
}

void FilebrowseHandler::CopyFile(const FilePath& src, const FilePath& dest) {
  if (file_util::DirectoryExists(src)) {
    if (!file_util::CopyDirectory(src, dest, true)) {
      LOG(ERROR) << "unable to copy directory:" << src.value();
    }
  } else {
    if (!file_util::CopyFile(src, dest)) {
      LOG(ERROR) << "unable to copy file" << src.value();
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(current_task_, &TaskProxy::FireCopyCompleteProxy));
}

void FilebrowseHandler::HandleDeleteFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = WideToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);

  // Don't allow file deletion in inaccessible dirs.
  if (net::URLRequestFileJob::AccessDisabled(currentpath))
    return;

  for (unsigned int x = 0; x < active_download_items_.size(); x++) {
    FilePath item = active_download_items_[x]->full_path();
    if (item == currentpath) {
      active_download_items_[x]->Cancel(true);
      active_download_items_[x]->Remove(true);
      FilePath dir_path = item.DirName();
      GetChildrenForPath(dir_path, true);
      return;
    }
  }
  TaskProxy* task = new TaskProxy(AsWeakPtr(), currentpath);
  task->AddRef();
  current_task_ = task;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          task, &TaskProxy::DeleteFileProxy));
#endif
}

void FilebrowseHandler::HandleCopyFile(const ListValue* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string src;
    std::string dest;

    // Get path string.
    if (list_value->GetString(0, &src) &&
        list_value->GetString(1, &dest)) {
      FilePath SrcPath = FilePath(src);
      FilePath DestPath = FilePath(dest);

      // Don't allow file copy to inaccessible dirs.
      if (net::URLRequestFileJob::AccessDisabled(DestPath))
        return;

      TaskProxy* task = new TaskProxy(AsWeakPtr(), SrcPath, DestPath);
      task->AddRef();
      current_task_ = task;
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              task, &TaskProxy::CopyFileProxy));
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void FilebrowseHandler::HandleValidateSavePath(const ListValue* args) {
  std::string string_path;
  if (!args || !args->GetString(0, &string_path)) {
    FireOnValidatedSavePathOnUIThread(false, FilePath());  // Invalid save path.
    return;
  }

  FilePath save_path(string_path);

#if defined(OS_CHROMEOS)
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), save_path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::ValidateSavePathOnFileThread));
#else
  // No save path checking for non-ChromeOS platforms.
  FireOnValidatedSavePathOnUIThread(true, save_path);
#endif
}

void FilebrowseHandler::ValidateSavePathOnFileThread(
    const FilePath& save_path) {
#if defined(OS_CHROMEOS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }

  // Get containing folder of save_path.
  FilePath save_dir = save_path.DirName();

  // Valid save path must be inside default download dir.
  bool valid = default_download_path == save_dir ||
               file_util::ContainsPath(default_download_path, save_dir);

  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), save_path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &TaskProxy::FireOnValidatedSavePathOnUIThread,
                        valid, save_path));
#endif
}

void FilebrowseHandler::FireOnValidatedSavePathOnUIThread(bool valid,
    const FilePath& save_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FundamentalValue valid_value(valid);
  StringValue path_value(save_path.value());
  dom_ui_->CallJavascriptFunction(L"onValidatedSavePath",
      valid_value, path_value);
}

void FilebrowseHandler::OnDownloadUpdated(DownloadItem* download) {
  DownloadList::iterator it = find(active_download_items_.begin(),
                                   active_download_items_.end(),
                                   download);
  if (it == active_download_items_.end())
    return;
  const int id = static_cast<int>(it - active_download_items_.begin());

  scoped_ptr<DictionaryValue> download_item(
      download_util::CreateDownloadItemValue(download, id));
  dom_ui_->CallJavascriptFunction(L"downloadUpdated", *download_item.get());
}

void FilebrowseHandler::ClearDownloadItems() {
  for (DownloadList::iterator it = active_download_items_.begin();
      it != active_download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  active_download_items_.clear();
}

void FilebrowseHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (DownloadList::iterator it = active_download_items_.begin();
      it != active_download_items_.end(); ++it) {
    int index = static_cast<int>(it - active_download_items_.begin());
    results_value.Append(download_util::CreateDownloadItemValue(*it, index));
  }

  dom_ui_->CallJavascriptFunction(L"downloadsList", results_value);
}

void FilebrowseHandler::OnDownloadFileCompleted(DownloadItem* download) {
  GetChildrenForPath(currentpath_, true);
}

////////////////////////////////////////////////////////////////////////////////
//
// FileBrowseUI
//
////////////////////////////////////////////////////////////////////////////////

FileBrowseUI::FileBrowseUI(TabContents* contents) : HtmlDialogUI(contents) {
  FilebrowseHandler* handler = new FilebrowseHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init();
  FileBrowseUIHTMLSource* html_source = new FileBrowseUIHTMLSource();

  // Set up the chrome://filebrowse/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

// static
Browser* FileBrowseUI::OpenPopup(Profile* profile,
                                 const std::string& hashArgument,
                                 int width,
                                 int height) {
  // Get existing pop up for given hashArgument.
  Browser* browser = GetPopupForPath(hashArgument, profile);

  // Create new browser if no matching pop up found.
  if (browser == NULL) {
    browser = Browser::CreateForType(Browser::TYPE_APP_PANEL, profile);
    std::string url;
    if (hashArgument.empty()) {
      url = chrome::kChromeUIFileBrowseURL;
    } else {
      url = kFilebrowseURLHash;
      url.append(hashArgument);
    }

    browser::NavigateParams params(browser, GURL(url), PageTransition::LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&params);
    // TODO(beng): The following two calls should be automatic by Navigate().
    params.browser->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                  kPopupTop,
                                                  width,
                                                  height));

    params.browser->window()->Show();
  } else {
    browser->window()->Show();
  }

  return browser;
}

Browser* FileBrowseUI::GetPopupForPath(const std::string& path,
                                       Profile* profile) {
  std::string current_path = path;
  if (current_path.empty()) {
    bool is_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableAdvancedFileSystem);
    if (!is_enabled) {
      FilePath default_download_path;
      if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                            &default_download_path)) {
        NOTREACHED();
      }
      current_path = default_download_path.value();
    }
  }

  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if (((*it)->type() == Browser::TYPE_APP_PANEL)) {
      TabContents* tab_contents = (*it)->GetSelectedTabContents();
      DCHECK(tab_contents);
      if (!tab_contents)
          continue;
      const GURL& url = tab_contents->GetURL();

      if (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIFileBrowseHost &&
          url.ref() == current_path &&
          (*it)->profile() == profile) {
        return (*it);
      }
    }
  }

  return NULL;
}

const int FileBrowseUI::kPopupWidth = 250;
const int FileBrowseUI::kPopupHeight = 300;
const int FileBrowseUI::kSmallPopupWidth = 250;
const int FileBrowseUI::kSmallPopupHeight = 50;
