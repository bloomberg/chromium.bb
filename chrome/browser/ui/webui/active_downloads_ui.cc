// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/active_downloads_ui.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/mediaplayer_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
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
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

const int kPopupWidth = 250;
const int kPopupHeight = 300;
const int kSmallPopupWidth = 250;
const int kSmallPopupHeight = 50;

static const char kPropertyPath[] = "path";
static const char kPropertyTitle[] = "title";
static const char kPropertyDirectory[] = "isDirectory";
static const char kActiveDownloadsURLHash[] = "chrome://active-downloads#";
static const int kPopupLeft = 0;
static const int kPopupTop = 0;

class ActiveDownloadsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  ActiveDownloadsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~ActiveDownloadsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(ActiveDownloadsUIHTMLSource);
};

class TaskProxy;

// The handler for Javascript messages related to the "active_downloads" view.
class ActiveDownloadsHandler
    : public base::SupportsWeakPtr<ActiveDownloadsHandler>,
      public net::DirectoryLister::DirectoryListerDelegate,
      public WebUIMessageHandler,
      public DownloadManager::Observer,
      public DownloadItem::Observer {
 public:
  ActiveDownloadsHandler();
  virtual ~ActiveDownloadsHandler();

  // Initialization after Attach.
  void Init();

  // DirectoryLister::DirectoryListerDelegate methods:
  virtual void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int error);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* item);
  virtual void OnDownloadOpened(DownloadItem* item) { }

  // DownloadManager::Observer interface.
  virtual void ModelChanged();

  // WebUI Callbacks.
  void HandleGetRoots(const ListValue* args);
  void HandleGetChildren(const ListValue* args);
  void HandleRefreshDirectory(const ListValue* args);
  void HandleGetDownloads(const ListValue* args);
  void HandlePauseToggleDownload(const ListValue* args);
  void HandleCancelDownload(const ListValue* args);
  void HandleAllowDownload(const ListValue* args);
  void HandleIsAdvancedEnabled(const ListValue* args);
  void HandleDeleteFile(const ListValue* args);
  void HandleCopyFile(const ListValue* value);
  void HandleValidateSavePath(const ListValue* args);
  void OpenNewPopupWindow(const ListValue* args);
  void OpenNewFullWindow(const ListValue* args);
  void PlayMediaFile(const ListValue* args);
  void EnqueueMediaFile(const ListValue* args);

  // TaskProxy callbacks.
  void DeleteFile(const FilePath& path, TaskProxy* task);
  void CopyFile(const FilePath& src, const FilePath& dest, TaskProxy* task);
  void FireDeleteComplete(const FilePath& path);
  void FireCopyComplete(const FilePath& src, const FilePath& dest);
  void ValidateSavePathOnFileThread(const FilePath& save_path, TaskProxy* task);
  void FireOnValidatedSavePathOnUIThread(bool valid, const FilePath& save_path);

  void AddToDownloads(DownloadItem* item);

 private:
  void GetChildrenForPath(const FilePath& path, bool is_refresh);

  void OpenNewWindow(const ListValue* args, bool popup);

  void UpdateDownloadList();
  void ClearDownloadItems();
  void SendDownloads();
  void SendNewDownload(DownloadItem* item);

  bool ValidateSaveDir(const FilePath& save_dir, bool exists) const;
  bool AccessDisabled(const FilePath& path) const;

  scoped_ptr<ListValue> filelist_value_;
  FilePath currentpath_;
  Profile* profile_;
  TabContents* tab_contents_;
  std::string current_file_contents_;
  TaskProxy* current_task_;
  scoped_refptr<net::DirectoryLister> lister_;
  bool is_refresh_;

  DownloadManager* download_manager_;
  typedef std::vector<DownloadItem*> DownloadList;
  DownloadList active_download_items_;
  DownloadList download_items_;
  DownloadList downloads_;
  DISALLOW_COPY_AND_ASSIGN(ActiveDownloadsHandler);
};

class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  TaskProxy(const base::WeakPtr<ActiveDownloadsHandler>& handler,
            const FilePath& path, const FilePath& dest)
      : handler_(handler),
        src_(path),
        dest_(dest) {}
  TaskProxy(const base::WeakPtr<ActiveDownloadsHandler>& handler,
            const FilePath& path)
      : handler_(handler),
        src_(path) {}

  // TaskProxy is created on the UI thread, so in some cases,
  // we need to post back to the UI thread for destruction.
  void DeleteOnUIThread() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TaskProxy::DoNothing));
  }

  void DoNothing() {}

  void DeleteFileProxy() {
    if (handler_)
      handler_->DeleteFile(src_, this);
  }

  void CopyFileProxy() {
    if (handler_)
      handler_->CopyFile(src_, dest_, this);
  }

  void FireDeleteCompleteProxy() {
    if (handler_)
      handler_->FireDeleteComplete(src_);
  }
  void FireCopyCompleteProxy() {
    if (handler_)
      handler_->FireCopyComplete(src_, dest_);
  }

  void ValidateSavePathOnFileThread() {
    if (handler_)
      handler_->ValidateSavePathOnFileThread(src_, this);
  }

  void FireOnValidatedSavePathOnUIThread(bool valid) {
    if (handler_)
      handler_->FireOnValidatedSavePathOnUIThread(valid, src_);
  }

 private:
  base::WeakPtr<ActiveDownloadsHandler> handler_;
  FilePath src_;
  FilePath dest_;
  friend class base::RefCountedThreadSafe<TaskProxy>;
  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
};


////////////////////////////////////////////////////////////////////////////////
//
// ActiveDownloadsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

ActiveDownloadsUIHTMLSource::ActiveDownloadsUIHTMLSource()
    : DataSource(chrome::kChromeUIActiveDownloadsHost, MessageLoop::current()) {
}

void ActiveDownloadsUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_incognito,
                                              int request_id) {
  DictionaryValue localized_strings;
  // TODO(dhg): Add strings to localized strings, also add more strings
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
  localized_strings.SetString("delete",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_DELETE));
  localized_strings.SetString("enqueue",
      l10n_util::GetStringUTF16(IDS_FILEBROWSER_ENQUEUE));
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

  static const base::StringPiece active_downloads_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ACTIVE_DOWNLOADS_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      active_downloads_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// ActiveDownloadsHandler
//
////////////////////////////////////////////////////////////////////////////////
ActiveDownloadsHandler::ActiveDownloadsHandler()
    : profile_(NULL),
      tab_contents_(NULL),
      is_refresh_(false),
      download_manager_(NULL) {
  lister_ = NULL;
}

ActiveDownloadsHandler::~ActiveDownloadsHandler() {
  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
  }

  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
}

WebUIMessageHandler* ActiveDownloadsHandler::Attach(WebUI* web_ui) {
  // Create our favicon data source.
  profile_ = web_ui->GetProfile();
  profile_->GetChromeURLDataManager()->AddDataSource(
      new FaviconSource(profile_));
  tab_contents_ = web_ui->tab_contents();
  return WebUIMessageHandler::Attach(web_ui);
}

void ActiveDownloadsHandler::Init() {
  download_manager_ = profile_->GetDownloadManager();
  download_manager_->AddObserver(this);
}

void ActiveDownloadsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getRoots",
      NewCallback(this, &ActiveDownloadsHandler::HandleGetRoots));
  web_ui_->RegisterMessageCallback("getChildren",
      NewCallback(this, &ActiveDownloadsHandler::HandleGetChildren));
  web_ui_->RegisterMessageCallback("refreshDirectory",
      NewCallback(this, &ActiveDownloadsHandler::HandleRefreshDirectory));
  web_ui_->RegisterMessageCallback("getDownloads",
      NewCallback(this, &ActiveDownloadsHandler::HandleGetDownloads));
  web_ui_->RegisterMessageCallback("pauseToggleDownload",
      NewCallback(this, &ActiveDownloadsHandler::HandlePauseToggleDownload));
  web_ui_->RegisterMessageCallback("cancelDownload",
      NewCallback(this, &ActiveDownloadsHandler::HandleCancelDownload));
  web_ui_->RegisterMessageCallback("allowDownload",
      NewCallback(this, &ActiveDownloadsHandler::HandleAllowDownload));
  web_ui_->RegisterMessageCallback("isAdvancedEnabled",
      NewCallback(this, &ActiveDownloadsHandler::HandleIsAdvancedEnabled));
  web_ui_->RegisterMessageCallback("deleteFile",
      NewCallback(this, &ActiveDownloadsHandler::HandleDeleteFile));
  web_ui_->RegisterMessageCallback("copyFile",
      NewCallback(this, &ActiveDownloadsHandler::HandleCopyFile));
  web_ui_->RegisterMessageCallback("validateSavePath",
      NewCallback(this, &ActiveDownloadsHandler::HandleValidateSavePath));
  web_ui_->RegisterMessageCallback("openNewPopupWindow",
      NewCallback(this, &ActiveDownloadsHandler::OpenNewPopupWindow));
  web_ui_->RegisterMessageCallback("openNewFullWindow",
      NewCallback(this, &ActiveDownloadsHandler::OpenNewFullWindow));
  web_ui_->RegisterMessageCallback("playMediaFile",
      NewCallback(this, &ActiveDownloadsHandler::PlayMediaFile));
  web_ui_->RegisterMessageCallback("enqueueMediaFile",
      NewCallback(this, &ActiveDownloadsHandler::EnqueueMediaFile));
}

void ActiveDownloadsHandler::FireDeleteComplete(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We notify the UI by telling it to refresh its contents.
  FilePath dir_path = path.DirName();
  GetChildrenForPath(dir_path, true);
};

void ActiveDownloadsHandler::FireCopyComplete(const FilePath& src,
                                         const FilePath& dest) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath dir_path = dest.DirName();
  GetChildrenForPath(dir_path, true);
};

void ActiveDownloadsHandler::HandleGetRoots(const ListValue* args) {
  ListValue results_value;
  DictionaryValue info_value;
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
  web_ui_->CallJavascriptFunction("browseFileResult",
                                  info_value, results_value);
}

void ActiveDownloadsHandler::PlayMediaFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  FilePath file_path(UTF16ToUTF8(ExtractStringValue(args)));

  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
  mediaplayer->ForcePlayMediaFile(profile_, file_path, browser);
#endif
}

void ActiveDownloadsHandler::EnqueueMediaFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  FilePath file_path(UTF16ToUTF8(ExtractStringValue(args)));

  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
  mediaplayer->EnqueueMediaFile(profile_, file_path, browser);
#endif
}

void ActiveDownloadsHandler::HandleIsAdvancedEnabled(const ListValue* args) {
#if defined(OS_CHROMEOS)
  bool is_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAdvancedFileSystem);
  DictionaryValue info_value;
  info_value.SetBoolean("enabled", is_enabled);
  info_value.SetBoolean("mpEnabled", true);
  web_ui_->CallJavascriptFunction("enabledResult", info_value);

#endif
}

void ActiveDownloadsHandler::HandleRefreshDirectory(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = UTF16ToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);
  GetChildrenForPath(currentpath, true);
#endif
}

void ActiveDownloadsHandler::HandlePauseToggleDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= static_cast<int>(active_download_items_.size())) {
    return;
  }
  DownloadItem* item = active_download_items_[id];
  item->TogglePause();
#endif
}

void ActiveDownloadsHandler::HandleAllowDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= static_cast<int>(active_download_items_.size())) {
    return;
  }

  DownloadItem* item = active_download_items_[id];
  download_manager_->DangerousDownloadValidated(item);
#endif
}

void ActiveDownloadsHandler::HandleCancelDownload(const ListValue* args) {
#if defined(OS_CHROMEOS)
  int id;
  ExtractIntegerValue(args, &id);
  if ((id - 1) >= static_cast<int>(active_download_items_.size())) {
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

void ActiveDownloadsHandler::OpenNewFullWindow(const ListValue* args) {
  OpenNewWindow(args, false);
}

void ActiveDownloadsHandler::OpenNewPopupWindow(const ListValue* args) {
  OpenNewWindow(args, true);
}

void ActiveDownloadsHandler::OpenNewWindow(const ListValue* args, bool popup) {
  std::string url = UTF16ToUTF8(ExtractStringValue(args));
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


void ActiveDownloadsHandler::GetChildrenForPath(const FilePath& path,
                                           bool is_refresh) {
  if (path.empty())
    return;

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
  if (AccessDisabled(path))
    return;
#endif

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
  // lister_->Start();
  OnListDone(0);
}

void ActiveDownloadsHandler::HandleGetChildren(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = UTF16ToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);
  filelist_value_.reset(new ListValue());

  GetChildrenForPath(currentpath, false);
#endif
}

void ActiveDownloadsHandler::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
#if defined(OS_WIN)
  if (data.info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
    return;
  }
#elif defined(OS_POSIX)
  if (data.info.filename[0] == '.') {
    return;
  }

  // Suppress .crdownload files.
  static const char crdownload[] = (".crdownload");
  static const size_t crdownload_size = arraysize(crdownload);
  const std::string& filename = data.info.filename;
  if ((filename.size() > crdownload_size) &&
      (filename.rfind(crdownload) == (filename.size() - crdownload_size)))
    return;
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

void ActiveDownloadsHandler::OnListDone(int error) {
  DictionaryValue info_value;
  info_value.SetString("functionCall", is_refresh_ ? "refresh" : "getChildren");
  info_value.SetString(kPropertyPath, currentpath_.value());
  web_ui_->CallJavascriptFunction("browseFileResult",
                                  info_value, *(filelist_value_.get()));
}

void ActiveDownloadsHandler::ModelChanged() {
  LOG(INFO) << "ModelChanged";
  UpdateDownloadList();
}

void ActiveDownloadsHandler::HandleGetDownloads(const ListValue* args) {
  LOG(INFO) << "HandleGetDownloads";
  UpdateDownloadList();
}

void ActiveDownloadsHandler::UpdateDownloadList() {
  LOG(INFO) << "UpdateDownloadList";
  ClearDownloadItems();
  DCHECK(active_download_items_.empty());

  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(FilePath(), &downloads);

  download_items_.clear();
  // Scan for any in progress downloads and add ourself to them as an observer.
  for (DownloadList::iterator it = downloads.begin();
       it != downloads.end();
       ++it) {
    DownloadItem* item = *it;
    // We want to know what happens as the download progresses and be notified
    // when the user validates the dangerous download.
    if ((item->state() == DownloadItem::IN_PROGRESS ||
         item->safety_state() == DownloadItem::DANGEROUS)) {
      LOG(INFO) << "UpdateDownloadList " << item << ", "
        << item->full_path().value();
      item->AddObserver(this);
      active_download_items_.push_back(item);
      AddToDownloads(item);
    }
    download_items_.push_back(item);
  }
  SendDownloads();
}

void ActiveDownloadsHandler::SendNewDownload(DownloadItem* item) {
  LOG(INFO) << "SendNewDownload " << item->full_path().value();
  ListValue results_value;
  results_value.Append(download_util::CreateDownloadItemValue(item, -1));
  web_ui_->CallJavascriptFunction("newDownload", results_value);
}

void ActiveDownloadsHandler::SendDownloads() {
  ListValue results;

  DownloadList::iterator it = downloads_.begin();
  for (int i = 0; it != downloads_.end(); ++i, ++it) {
    LOG(INFO) << "SendDownloads " << (*it)->full_path().value();
    results.Append(download_util::CreateDownloadItemValue(*it, i));
  }

  web_ui_->CallJavascriptFunction("downloadsList", results);
}

void ActiveDownloadsHandler::AddToDownloads(DownloadItem* item) {
  LOG(INFO) << "AddToDownloads " << item->full_path().value();
  if (item && (std::find(downloads_.begin(), downloads_.end(), item) ==
      downloads_.end())) {
    downloads_.push_back(item);
  }
}

void ActiveDownloadsHandler::ClearDownloadItems() {
  for (DownloadList::iterator it = active_download_items_.begin();
      it != active_download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  active_download_items_.clear();
}

void ActiveDownloadsHandler::OnDownloadUpdated(DownloadItem* item) {
  DownloadList::iterator it = find(active_download_items_.begin(),
                                   active_download_items_.end(),
                                   item);
  if (it == active_download_items_.end())
    return;

  const size_t id = it - active_download_items_.begin();
  web_ui_->CallJavascriptFunction("downloadUpdated",
      *download_util::CreateDownloadItemValue(item, id));
}

void ActiveDownloadsHandler::DeleteFile(const FilePath& path, TaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(path, true)) {
    LOG(ERROR) << "unable to delete directory";
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task, &TaskProxy::FireDeleteCompleteProxy));
}

void ActiveDownloadsHandler::CopyFile(const FilePath& src,
                                 const FilePath& dest,
                                 TaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
      NewRunnableMethod(task, &TaskProxy::FireCopyCompleteProxy));
}

void ActiveDownloadsHandler::HandleDeleteFile(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = UTF16ToUTF8(ExtractStringValue(args));
  FilePath currentpath(path);

  // Don't allow file deletion in inaccessible dirs.
  if (AccessDisabled(currentpath))
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
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), currentpath);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          task.get(), &TaskProxy::DeleteFileProxy));
#endif
}

void ActiveDownloadsHandler::HandleCopyFile(const ListValue* value) {
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
      if (AccessDisabled(DestPath))
        return;

      scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(),
                                                    SrcPath, DestPath);
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              task.get(), &TaskProxy::CopyFileProxy));
    } else {
      LOG(ERROR) << "Unable to get string";
      return;
    }
  }
#endif
}

void ActiveDownloadsHandler::HandleValidateSavePath(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string string_path;
  if (!args || !args->GetString(0, &string_path)) {
    FireOnValidatedSavePathOnUIThread(false, FilePath());  // Invalid save path.
    return;
  }

  FilePath save_path(string_path);

  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), save_path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::ValidateSavePathOnFileThread));
#endif
}

void ActiveDownloadsHandler::ValidateSavePathOnFileThread(
    const FilePath& save_path, TaskProxy* task) {
#if defined(OS_CHROMEOS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const bool valid = ValidateSaveDir(save_path.DirName(), true);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task,
                        &TaskProxy::FireOnValidatedSavePathOnUIThread,
                        valid));
#endif
}

bool ActiveDownloadsHandler::ValidateSaveDir(const FilePath& save_dir,
                                        bool exists) const {
#if defined(OS_CHROMEOS)
  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }

  // Valid save dir must be inside default download dir.
  if (default_download_path == save_dir)
    return true;
  if (exists) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
      return file_util::ContainsPath(default_download_path, save_dir);
  } else {
    return default_download_path.IsParent(save_dir);
  }
#endif
  return false;
}

void ActiveDownloadsHandler::FireOnValidatedSavePathOnUIThread(bool valid,
    const FilePath& save_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FundamentalValue valid_value(valid);
  StringValue path_value(save_path.value());
  web_ui_->CallJavascriptFunction("onValidatedSavePath",
      valid_value, path_value);
}

bool ActiveDownloadsHandler::AccessDisabled(const FilePath& path) const {
  return !ValidateSaveDir(path, false) &&
        net::URLRequestFileJob::AccessDisabled(path);
}

////////////////////////////////////////////////////////////////////////////////
//
// ActiveDownloadsUI
//
////////////////////////////////////////////////////////////////////////////////

DownloadItem* ActiveDownloadsUI::first_download_item_;

ActiveDownloadsUI::ActiveDownloadsUI(TabContents* contents)
    : HtmlDialogUI(contents),
      handler_(new ActiveDownloadsHandler()) {
  AddMessageHandler((handler_)->Attach(this));
  LOG(WARNING) << "ActiveDownloadsUI ctor";
  handler_->AddToDownloads(first_download_item_);
  first_download_item_ = NULL;
  handler_->Init();
  ActiveDownloadsUIHTMLSource* html_source = new ActiveDownloadsUIHTMLSource();

  // Set up the chrome://active-downloads/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
Browser* ActiveDownloadsUI::OpenPopup(Profile* profile,
                                      DownloadItem* item) {
  // Get existing pop up for given hashArgument.
  std::string hashArgument = item->full_path().DirName().value();
  Browser* browser = GetPopupForPath(hashArgument, profile);

  // Create new browser if no matching pop up is found.
  if (browser == NULL) {
    first_download_item_ = item;
    browser = Browser::CreateForType(Browser::TYPE_APP_PANEL, profile);
    std::string url;
    if (hashArgument.empty()) {
      url = chrome::kChromeUIActiveDownloadsURL;
    } else {
      url = kActiveDownloadsURLHash;
      url.append(hashArgument);
    }

    browser::NavigateParams params(browser, GURL(url), PageTransition::LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&params);
    // TODO(beng): The following two calls should be automatic by Navigate().
    params.browser->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                  kPopupTop,
                                                  kPopupWidth,
                                                  kPopupHeight));

    params.browser->window()->Show();
  } else {
    browser->window()->Show();
  }

  return browser;
}

Browser* ActiveDownloadsUI::GetPopupForPath(const std::string& path,
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
          url.host() == chrome::kChromeUIActiveDownloadsHost &&
          url.ref() == current_path &&
          (*it)->profile() == profile) {
        return (*it);
      }
    }
  }

  return NULL;
}
