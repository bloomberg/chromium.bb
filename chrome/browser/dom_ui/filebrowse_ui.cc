// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/filebrowse_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
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
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/mount_library.h"
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
static const std::string kPicasawebDropBox = "/DropBox";
static const std::string kPicasawebBaseUrl = "http://picasaweb.google.com/";

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

  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

  // Callback for the "getChildren" message.
  void HandleGetChildren(const Value* value);

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

  void ReadInFile();
  void FireUploadComplete();

 private:

  void OpenNewWindow(const Value* value, bool popup);

  // Clear all download items and their observers.
  void ClearDownloadItems();

  // Send the current list of downloads to the page.
  void SendCurrentDownloads();

  scoped_ptr<ListValue> filelist_value_;
  FilePath currentpath_;
  Profile* profile_;
  std::string current_file_contents_;
  std::string current_file_uploaded_;
  int upload_response_code_;
  TaskProxy* CurrentTask_;
  scoped_refptr<net::DirectoryLister> lister_;

  DownloadManager* download_manager_;
  typedef std::vector<DownloadItem*> DownloadList;
  DownloadList download_items_;

  DISALLOW_COPY_AND_ASSIGN(FilebrowseHandler);
};

class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  explicit TaskProxy(const base::WeakPtr<FilebrowseHandler>& handler)
      : handler_(handler) {}
  void ReadInFileProxy() {
    if (handler_) {
      handler_->ReadInFile();
    }
  }

  void FireUploadCompleteProxy() {
    if (handler_) {
      handler_->FireUploadComplete();
    }
  }
 private:
  base::WeakPtr<FilebrowseHandler> handler_;
  friend class base::RefCountedThreadSafe<TaskProxy>;
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
    bool is_off_the_record, int request_id) {
  DictionaryValue localized_strings;
  // TODO(dhg): Add stirings to localized strings, also add more strings
  // that are currently hardcoded.
  localized_strings.SetString(L"devices", "devices");

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
      download_manager_(NULL) {
  // TODO(dhg): Check to see if this is really necessary
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib = chromeos::MountLibrary::Get();
  lib->AddObserver(this);
#endif
  lister_ = NULL;
}

FilebrowseHandler::~FilebrowseHandler() {
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib = chromeos::MountLibrary::Get();
  lib->RemoveObserver(this);
#endif
  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
  }

  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
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

  return DOMMessageHandler::Attach(dom_ui);
}

void FilebrowseHandler::Init() {
  download_manager_ = profile_->GetOriginalProfile()->GetDownloadManager();
  download_manager_->AddObserver(this);
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
}

void FilebrowseHandler::FireUploadComplete() {
  DictionaryValue info_value;
  info_value.SetString(L"path", current_file_uploaded_);

  std::string username;
  username = getenv("CHROMEOS_USER");

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

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(CurrentTask_, &TaskProxy::FireUploadCompleteProxy));
}

void FilebrowseHandler::HandleGetRoots(const Value* value) {
  ListValue results_value;
  DictionaryValue info_value;
  // TODO(dhg): add other entries, make this more general
#if defined(OS_CHROMEOS)
  chromeos::MountLibrary* lib = chromeos::MountLibrary::Get();
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
      browser = Browser::Create(profile_);
    }
    browser->AddTabWithURL(
        GURL(path), GURL(), PageTransition::LINK,
        true, -1, false, NULL);
    if (popup) {
      // TODO(dhg): Remove these from being hardcoded. Allow javascript
      // to specify.
      browser->window()->SetBounds(gfx::Rect(0, 0, 400, 300));
    } else {
      browser->window()->SetBounds(gfx::Rect(0, 0, 800, 600));
    }
    browser->window()->Show();
  } else {
    LOG(ERROR) << "Wasn't able to get the List if requested files.";
    return;
  }
}

void FilebrowseHandler::ReadInFile() {
#if defined(OS_CHROMEOS)
  // Get the users username
  std::string username;
  username = getenv("CHROMEOS_USER");

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

  URLFetcher* fetcher = URLFetcher::Create(0,
                                           GURL(url),
                                           URLFetcher::POST,
                                           this);
  fetcher->set_upload_data("image/jpeg", filecontents);

  // Set the filename on the server
  std::string slug = "Slug: ";
  slug += filename;
  fetcher->set_extra_request_headers(slug);
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->Start();
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
  TaskProxy* task = new TaskProxy(AsWeakPtr());
  task->AddRef();
  CurrentTask_ = task;
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          task, &TaskProxy::ReadInFileProxy));
#endif
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
#if defined(OS_WIN)
  currentpath_ = FilePath(ASCIIToWide(path));
#else
  currentpath_ = FilePath(path);
#endif

  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }
  lister_ = new net::DirectoryLister(currentpath_, this);
  lister_->Start();
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
  info_value.SetString(L"functionCall", "getChildren");
  info_value.SetString(kPropertyPath, currentpath_.value());
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, *(filelist_value_.get()));
}

void FilebrowseHandler::HandleGetMetadata(const Value* value) {
}

void FilebrowseHandler::HandleGetDownloads(const Value* value) {
  ModelChanged();
}

void FilebrowseHandler::ModelChanged() {
  ClearDownloadItems();
  download_manager_->GetDownloads(this, std::wstring());
}

void FilebrowseHandler::SetDownloads(std::vector<DownloadItem*>& downloads) {
  ClearDownloadItems();

  // Scan for any in progress downloads and add ourself to them as an observer.
  for (DownloadList::iterator it = downloads.begin();
       it != downloads.end(); ++it) {
    DownloadItem* download = *it;
    // We want to know what happens as the download progresses and be notified
    // when the user validates the dangerous download.
    if (download->state() == DownloadItem::IN_PROGRESS ||
        download->safety_state() == DownloadItem::DANGEROUS) {
      download->AddObserver(this);
      download_items_.push_back(download);
    }
  }

  SendCurrentDownloads();
}

void FilebrowseHandler::OnDownloadUpdated(DownloadItem* download) {
  DownloadList::iterator it = find(download_items_.begin(),
                                   download_items_.end(),
                                   download);
  if (it == download_items_.end())
    return;
  const int id = static_cast<int>(it - download_items_.begin());

  ListValue results_value;
  results_value.Append(download_util::CreateDownloadItemValue(download, id));
  dom_ui_->CallJavascriptFunction(L"downloadUpdated", results_value);
}

void FilebrowseHandler::ClearDownloadItems() {
  for (DownloadList::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  download_items_.clear();
}

void FilebrowseHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (DownloadList::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    int index = static_cast<int>(it - download_items_.begin());
    results_value.Append(download_util::CreateDownloadItemValue(*it, index));
  }

  dom_ui_->CallJavascriptFunction(L"downloadsList", results_value);
}

////////////////////////////////////////////////////////////////////////////////
//
// FileBrowseUIContents
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
