// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/filebrowse_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;
static const std::wstring kPropertyPath = L"path";
static const std::wstring kPropertyTitle = L"title";
static const std::wstring kPropertyDirectory = L"isDirectory";
static const std::string kPicasawebUserPrefix =
    "http://picasaweb.google.com/data/feed/api/user/";
static const std::string kPicasawebDefault = "/albumid/default";
static const std::string kPicasawebDropBox = "/home";
static const std::string kPicasawebBaseUrl = "http://picasaweb.google.com/";
static const std::string kMediaPath = "/media";
static const char* kFilebrowseURLHash = "chrome://filebrowse#";
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
  virtual void OnListFile(const file_util::FileEnumerator::FindInfo& data);
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
  virtual void OnDownloadFileCompleted(DownloadItem* download) { }
  virtual void OnDownloadOpened(DownloadItem* download) { }

  // DownloadManager::Observer interface
  virtual void ModelChanged();
  virtual void SetDownloads(std::vector<DownloadItem*>& downloads);

  // Callback for the "getRoots" message.
  void HandleGetRoots(const Value* value);

  void GetChildrenForPath(FilePath& path, bool is_refresh);

  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

  // Callback for the "getChildren" message.
  void HandleGetChildren(const Value* value);
 // Callback for the "refreshDirectory" message.
  void HandleRefreshDirectory(const Value* value);
  void HandleIsAdvancedEnabled(const Value* value);

  // Callback for the "getMetadata" message.
  void HandleGetMetadata(const Value* value);

 // Callback for the "openNewWindow" message.
  void OpenNewFullWindow(const Value* value);
  void OpenNewPopupWindow(const Value* value);

  // Callback for the "uploadToPicasaweb" message.
  void UploadToPicasaweb(const Value* value);

  // Callback for the "getDownloads" message.
  void HandleGetDownloads(const Value* value);

  void HandleCreateNewFolder(const Value* value);

  void PlayMediaFile(const Value* value);
  void EnqueueMediaFile(const Value* value);

  void HandleDeleteFile(const Value* value);
  void DeleteFile(const FilePath& path);
  void FireDeleteComplete(const FilePath& path);

  void HandlePauseToggleDownload(const Value* value);

  void HandleCancelDownload(const Value* value);
  void HandleAllowDownload(const Value* value);

  void ReadInFile();
  void FireUploadComplete();

  void SendPicasawebRequest();
 private:

  void OpenNewWindow(const Value* value, bool popup);

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
  explicit TaskProxy(const base::WeakPtr<FilebrowseHandler>& handler,
                     FilePath& path)
      : handler_(handler),
        path_(path) {}
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
      handler_->DeleteFile(path_);
    }
  }

  void FireDeleteCompleteProxy() {
    if (handler_) {
      handler_->FireDeleteComplete(path_);
    }
  }
 private:
  base::WeakPtr<FilebrowseHandler> handler_;
  FilePath path_;
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
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_FILEBROWSER_TITLE));
  localized_strings.SetString(L"pause",
      l10n_util::GetString(IDS_FILEBROWSER_PAUSE));
  localized_strings.SetString(L"resume",
      l10n_util::GetString(IDS_FILEBROWSER_RESUME));
  localized_strings.SetString(L"scanning",
      l10n_util::GetString(IDS_FILEBROWSER_SCANNING));
  localized_strings.SetString(L"confirmdelete",
      l10n_util::GetString(IDS_FILEBROWSER_CONFIRM_DELETE));
  localized_strings.SetString(L"confirmyes",
      l10n_util::GetString(IDS_FILEBROWSER_CONFIRM_YES));
  localized_strings.SetString(L"confirmcancel",
      l10n_util::GetString(IDS_FILEBROWSER_CONFIRM_CANCEL));
  localized_strings.SetString(L"allowdownload",
      l10n_util::GetString(IDS_FILEBROWSER_CONFIRM_DOWNLOAD));
  localized_strings.SetString(L"filenameprompt",
      l10n_util::GetString(IDS_FILEBROWSER_PROMPT_FILENAME));
  localized_strings.SetString(L"save",
      l10n_util::GetString(IDS_FILEBROWSER_SAVE));
  localized_strings.SetString(L"newfolder",
      l10n_util::GetString(IDS_FILEBROWSER_NEW_FOLDER));
  localized_strings.SetString(L"open",
      l10n_util::GetString(IDS_FILEBROWSER_OPEN));
  localized_strings.SetString(L"picasaweb",
      l10n_util::GetString(IDS_FILEBROWSER_UPLOAD_PICASAWEB));
  localized_strings.SetString(L"flickr",
      l10n_util::GetString(IDS_FILEBROWSER_UPLOAD_FLICKR));
  localized_strings.SetString(L"email",
      l10n_util::GetString(IDS_FILEBROWSER_UPLOAD_EMAIL));
  localized_strings.SetString(L"delete",
      l10n_util::GetString(IDS_FILEBROWSER_DELETE));
  localized_strings.SetString(L"enqueue",
      l10n_util::GetString(IDS_FILEBROWSER_ENQUEUE));
  localized_strings.SetString(L"mediapath", kMediaPath);
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }
  localized_strings.SetString(L"downloadpath", default_download_path.value());
  localized_strings.SetString(L"error_unknown_file_type",
      l10n_util::GetString(IDS_FILEBROWSER_ERROR_UNKNOWN_FILE_TYPE));
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
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            task, &TaskProxy::DeleteFetcher, fetch));
  }
}

DOMMessageHandler* FilebrowseHandler::Attach(DOMUI* dom_ui) {
  // Create our favicon data source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new DOMUIFavIconSource(dom_ui->GetProfile()))));
  profile_ = dom_ui->GetProfile();
  tab_contents_ = dom_ui->tab_contents();
  return DOMMessageHandler::Attach(dom_ui);
}

void FilebrowseHandler::Init() {
  download_manager_ = profile_->GetOriginalProfile()->GetDownloadManager();
  download_manager_->AddObserver(this);
  TaskProxy* task = new TaskProxy(AsWeakPtr(), currentpath_);
  task->AddRef();
  current_task_ = task;
  static bool sent_request = false;
  if (!sent_request) {
    // If we have not sent a request before, we should do one in order to
    // ensure that we have the correct cookies. This is for uploads.
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
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
  dom_ui_->RegisterMessageCallback("cancelDownload",
      NewCallback(this, &FilebrowseHandler::HandleCancelDownload));
  dom_ui_->RegisterMessageCallback("allowDownload",
      NewCallback(this, &FilebrowseHandler::HandleAllowDownload));
  dom_ui_->RegisterMessageCallback("refreshDirectory",
      NewCallback(this, &FilebrowseHandler::HandleRefreshDirectory));
  dom_ui_->RegisterMessageCallback("isAdvancedEnabled",
      NewCallback(this, &FilebrowseHandler::HandleIsAdvancedEnabled));
}


void FilebrowseHandler::FireDeleteComplete(const FilePath& path) {
  // We notify the UI by telling it to refresh its contents.
  FilePath dir_path = path.DirName();
  GetChildrenForPath(dir_path, true);
};

void FilebrowseHandler::FireUploadComplete() {
#if defined(OS_CHROMEOS)
  DictionaryValue info_value;
  info_value.SetString(L"path", current_file_uploaded_);

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
  std::string picture_url;
  picture_url = kPicasawebBaseUrl;
  picture_url += username;
  picture_url += kPicasawebDropBox;
  info_value.SetString(L"url", picture_url);
  info_value.SetInteger(L"status_code", upload_response_code_);
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
                                           const URLRequestStatus& status,
                                           int response_code,
                                           const ResponseCookies& cookies,
                                           const std::string& data) {
  upload_response_code_ = response_code;
  LOG(INFO) << "Response code:" << response_code;
  LOG(INFO) << "request url" << url;
  if (StartsWithASCII(url.spec(), kPicasawebUserPrefix, true)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(current_task_, &TaskProxy::FireUploadCompleteProxy));
  }
  fetch_.reset();
}

void FilebrowseHandler::HandleGetRoots(const Value* value) {
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
      FilePath currentpath;
      currentpath = FilePath(disks[i].mount_path);
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

  info_value.SetString(L"functionCall", "getRoots");
  info_value.SetString(kPropertyPath, "");
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, results_value);
}

void FilebrowseHandler::HandleCreateNewFolder(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string path;

    // Get path string.
    if (list_value->GetString(0, &path)) {

      FilePath currentpath;
      currentpath = FilePath(path);

      if (!file_util::CreateDirectory(currentpath)) {
        LOG(ERROR) << "unable to create directory";
      }
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void FilebrowseHandler::PlayMediaFile(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string path;

    // Get path string.
    if (list_value->GetString(0, &path)) {
      FilePath currentpath;
      currentpath = FilePath(path);

      MediaPlayer* mediaplayer = MediaPlayer::Get();
      std::string url = currentpath.value();

      GURL gurl(url);
      Browser* browser = Browser::GetBrowserForController(
          &tab_contents_->controller(), NULL);
      mediaplayer->ForcePlayMediaURL(gurl, browser);
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void FilebrowseHandler::EnqueueMediaFile(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string path;

    // Get path string.
    if (list_value->GetString(0, &path)) {
      FilePath currentpath;
      currentpath = FilePath(path);

      MediaPlayer* mediaplayer = MediaPlayer::Get();
      std::string url = currentpath.value();

      GURL gurl(url);
      Browser* browser = Browser::GetBrowserForController(
          &tab_contents_->controller(), NULL);
      mediaplayer->EnqueueMediaURL(gurl, browser);
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void FilebrowseHandler::HandleIsAdvancedEnabled(const Value* value) {
#if defined(OS_CHROMEOS)
  Profile* profile = BrowserList::GetLastActive()->profile();
  PrefService* pref_service = profile->GetPrefs();
  bool is_enabled = pref_service->GetBoolean(
      prefs::kLabsAdvancedFilesystemEnabled);
  bool mp_enabled = pref_service->GetBoolean(prefs::kLabsMediaplayerEnabled);
  DictionaryValue info_value;
  info_value.SetBoolean(L"enabled", is_enabled);
  info_value.SetBoolean(L"mpEnabled", mp_enabled);
  dom_ui_->CallJavascriptFunction(L"enabledResult",
                                  info_value);
#endif
}
void FilebrowseHandler::HandleRefreshDirectory(const Value* value) {
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string path;

    // Get path string.
    if (list_value->GetString(0, &path)) {
      FilePath currentpath;
#if defined(OS_WIN)
      currentpath = FilePath(ASCIIToWide(path));
#else
      currentpath = FilePath(path);
#endif
      GetChildrenForPath(currentpath, true);
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
}

void FilebrowseHandler::HandlePauseToggleDownload(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    int id;
    std::string str_id;

    if (list_value->GetString(0, &str_id)) {
      id = atoi(str_id.c_str());
      DownloadItem* item = active_download_items_[id];
      item->TogglePause();
    } else {
      LOG(ERROR) << "Unable to get id for download to pause";
      return;
    }
  }
#endif
}

void FilebrowseHandler::HandleAllowDownload(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    int id;
    std::string str_id;

    if (list_value->GetString(0, &str_id)) {
      id = atoi(str_id.c_str());
      DownloadItem* item = active_download_items_[id];
      download_manager_->DangerousDownloadValidated(item);
    } else {
      LOG(ERROR) << "Unable to get id for download to pause";
      return;
    }
  }
#endif
}

void FilebrowseHandler::HandleCancelDownload(const Value* value) {
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    int id;
    std::string str_id;

    if (list_value->GetString(0, &str_id)) {
      id = atoi(str_id.c_str());
      DownloadItem* item = active_download_items_[id];
      item->Cancel(true);
      FilePath path = item->full_path();
      FilePath dir_path = path.DirName();
      item->Remove(true);
      GetChildrenForPath(dir_path, true);
    } else {
      LOG(ERROR) << "Unable to get id for download to pause";
      return;
    }
  }
#endif
}

void FilebrowseHandler::OpenNewFullWindow(const Value* value) {
  OpenNewWindow(value, false);
}

void FilebrowseHandler::OpenNewPopupWindow(const Value* value) {
  OpenNewWindow(value, true);
}

void FilebrowseHandler::OpenNewWindow(const Value* value, bool popup) {
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;
    std::string path;

    // Get path string.
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      string_value->GetAsString(&path);
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
    Browser* browser;
    if (popup) {
      browser = Browser::CreateForPopup(profile_);
    } else {
      browser = BrowserList::GetLastActive();
    }
    TabContents* contents = browser->AddTabWithURL(
        GURL(path), GURL(), PageTransition::LINK, -1,
        TabStripModel::ADD_SELECTED, NULL, std::string());
    // AddTabWithURL could have picked another Browser instance to create this
    // new tab at. So we have to reset the ptr of the browser that we want to
    // talk to.
    browser = contents->delegate()->GetBrowser();
    if (popup) {
      // TODO(dhg): Remove these from being hardcoded. Allow javascript
      // to specify.
      browser->window()->SetBounds(gfx::Rect(0, 0, 400, 300));
    }
    browser->window()->Show();
  } else {
    LOG(ERROR) << "Wasn't able to get the List if requested files.";
    return;
  }
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

  FilePath currentpath;
  currentpath = FilePath(current_file_uploaded_);
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
void FilebrowseHandler::UploadToPicasaweb(const Value* value) {
  std::string path;
#if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;

    // Get search string.
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      string_value->GetAsString(&path);
    }

  } else {
    LOG(ERROR) << "Wasn't able to get the List if requested files.";
    return;
  }
  current_file_uploaded_ = path;
  //  ReadInFile();
  FilePath current_path(path);
  TaskProxy* task = new TaskProxy(AsWeakPtr(), current_path);
  task->AddRef();
  current_task_ = task;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          task, &TaskProxy::ReadInFileProxy));
#endif
}

void FilebrowseHandler::GetChildrenForPath(FilePath& path, bool is_refresh) {
  filelist_value_.reset(new ListValue());
  currentpath_ = FilePath(path);

  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }

  is_refresh_ = is_refresh;
  lister_ = new net::DirectoryLister(currentpath_, this);
  lister_->Start();
}

void FilebrowseHandler::HandleGetChildren(const Value* value) {
  std::string path;
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;

    // Get search string.
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      string_value->GetAsString(&path);
    }

  } else {
    LOG(ERROR) << "Wasn't able to get the List if requested files.";
    return;
  }
  filelist_value_.reset(new ListValue());
  FilePath currentpath;
#if defined(OS_WIN)
  currentpath = FilePath(ASCIIToWide(path));
#else
  currentpath = FilePath(path);
#endif

  GetChildrenForPath(currentpath, false);
}

void FilebrowseHandler::OnListFile(
    const file_util::FileEnumerator::FindInfo& data) {
#if defined(OS_WIN)
  if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
    return;
  }
#elif defined(OS_POSIX)
  if (data.filename[0] == '.') {
    return;
  }
#endif

  DictionaryValue* file_value = new DictionaryValue();

#if defined(OS_WIN)
  int64 size = (static_cast<int64>(data.nFileSizeHigh) << 32) |
      data.nFileSizeLow;
  file_value->SetString(kPropertyTitle, data.cFileName);
  file_value->SetString(kPropertyPath,
                        currentpath_.Append(data.cFileName).value());
  file_value->SetBoolean(kPropertyDirectory,
      (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false);

#elif defined(OS_POSIX)
  file_value->SetString(kPropertyTitle, data.filename);
  file_value->SetString(kPropertyPath,
                        currentpath_.Append(data.filename).value());
  file_value->SetBoolean(kPropertyDirectory, S_ISDIR(data.stat.st_mode));
#endif
  filelist_value_->Append(file_value);
}

void FilebrowseHandler::OnListDone(int error) {
  DictionaryValue info_value;
  if (is_refresh_) {
    info_value.SetString(L"functionCall", "refresh");
  } else {
    info_value.SetString(L"functionCall", "getChildren");
  }
  info_value.SetString(kPropertyPath, currentpath_.value());
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, *(filelist_value_.get()));
  SendCurrentDownloads();
}

void FilebrowseHandler::HandleGetMetadata(const Value* value) {
}

void FilebrowseHandler::HandleGetDownloads(const Value* value) {
  ModelChanged();
}

void FilebrowseHandler::ModelChanged() {
  ClearDownloadItems();
  download_manager_->GetAllDownloads(this, FilePath());
}

void FilebrowseHandler::SetDownloads(std::vector<DownloadItem*>& downloads) {
  ClearDownloadItems();
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
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(current_task_, &TaskProxy::FireDeleteCompleteProxy));
}

void FilebrowseHandler::HandleDeleteFile(const Value* value) {
  #if defined(OS_CHROMEOS)
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string path;

    // Get path string.
    if (list_value->GetString(0, &path)) {

      FilePath currentpath;
      currentpath = FilePath(path);
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
      ChromeThread::PostTask(
          ChromeThread::FILE, FROM_HERE,
          NewRunnableMethod(
              task, &TaskProxy::DeleteFileProxy));
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void FilebrowseHandler::OnDownloadUpdated(DownloadItem* download) {
  DownloadList::iterator it = find(active_download_items_.begin(),
                                   active_download_items_.end(),
                                   download);
  if (it == active_download_items_.end())
    return;
  const int id = static_cast<int>(it - active_download_items_.begin());

  ListValue results_value;
  results_value.Append(download_util::CreateDownloadItemValue(download, id));
  dom_ui_->CallJavascriptFunction(L"downloadUpdated", results_value);
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
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

// static
Browser* FileBrowseUI::OpenPopup(Profile* profile,
                                 const std::string& hashArgument,
                                 int width,
                                 int height) {
  // Get existing pop up for given hashArgument.
  Browser* browser = GetPopupForPath(hashArgument);

  // Create new browser if no matching pop up found.
  if (browser == NULL) {
    browser = Browser::CreateForPopup(profile);
    std::string url;
    if (hashArgument.empty()) {
      url = chrome::kChromeUIFileBrowseURL;
    } else {
      url = kFilebrowseURLHash;
      url.append(hashArgument);
    }

    browser->AddTabWithURL(
        GURL(url), GURL(), PageTransition::LINK, -1,
        TabStripModel::ADD_SELECTED, NULL, std::string());
    browser->window()->SetBounds(gfx::Rect(kPopupLeft,
                                           kPopupTop,
                                           width,
                                           height));

    browser->window()->Show();
  }

  return browser;
}

Browser* FileBrowseUI::GetPopupForPath(const std::string& path) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->type() == Browser::TYPE_POPUP) {
      TabContents* tab_contents = (*it)->GetSelectedTabContents();
      DCHECK(tab_contents);
      if (!tab_contents)
          continue;
      const GURL& url = tab_contents->GetURL();

      if (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIFileBrowseHost &&
          url.ref() == path) {
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
