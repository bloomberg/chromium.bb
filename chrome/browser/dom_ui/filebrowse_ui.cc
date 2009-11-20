// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/filebrowse_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// Maximum number of search results to return in a given search. We should
// eventually remove this.
static const int kMaxSearchResults = 100;
static const std::wstring kPropertyPath = L"path";
static const std::wstring kPropertyTitle = L"title";
static const std::wstring kPropertyDirectory = L"isDirectory";


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

// The handler for Javascript messages related to the "filebrowse" view.
class FilebrowseHandler : public net::DirectoryLister::DirectoryListerDelegate,
                          public DOMMessageHandler {
 public:
  FilebrowseHandler();
  virtual ~FilebrowseHandler();

  // DirectoryLister::DirectoryListerDelegate methods:
  virtual void OnListFile(const file_util::FileEnumerator::FindInfo& data);
  virtual void OnListDone(int error);

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Callback for the "getRoots" message.
  void HandleGetRoots(const Value* value);

  // Callback for the "getChildren" message.
  void HandleGetChildren(const Value* value);

  // Callback for the "getMetadata" message.
  void HandleGetMetadata(const Value* value);

  // Callback for the "openNewWindow" message.
  void OpenNewFullWindow(const Value* value);
  void OpenNewPopupWindow(const Value* value);

 private:

  void OpenNewWindow(const Value* value, bool popup);

  scoped_ptr<ListValue> filelist_value_;
  FilePath currentpath_;
  Profile* profile_;
  scoped_refptr<net::DirectoryLister> lister_;

  DISALLOW_COPY_AND_ASSIGN(FilebrowseHandler);
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
    : profile_(NULL) {
  // TODO(dhg): Check to see if this is really necessary
  lister_ = NULL;
}

FilebrowseHandler::~FilebrowseHandler() {
  // TODO(dhg): Cancel any pending listings that are currently in flight.
  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
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
  return DOMMessageHandler::Attach(dom_ui);
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
}

void FilebrowseHandler::HandleGetRoots(const Value* value) {
  ListValue results_value;
  DictionaryValue info_value;

  DictionaryValue* page_value = new DictionaryValue();
  // TODO(dhg): add other entries, make this more general
  page_value->SetString(kPropertyPath, "/home/chronos");
  page_value->SetString(kPropertyTitle, "home");
  page_value->SetBoolean(kPropertyDirectory, true);

  results_value.Append(page_value);

  info_value.SetString(L"call", "getRoots");

  dom_ui_->CallJavascriptFunction(L"fileBrowseResult",
                                  info_value, results_value);
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
  info_value.SetString(L"call", "getChildren");
  info_value.SetString(kPropertyPath, currentpath_.value());
  dom_ui_->CallJavascriptFunction(L"fileBrowseResult",
                                  info_value, *(filelist_value_.get()));
}

void FilebrowseHandler::HandleGetMetadata(const Value* value) {
}

////////////////////////////////////////////////////////////////////////////////
//
// FileBrowseUIContents
//
////////////////////////////////////////////////////////////////////////////////

FileBrowseUI::FileBrowseUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new FilebrowseHandler())->Attach(this));
  FileBrowseUIHTMLSource* html_source = new FileBrowseUIHTMLSource();

  // Set up the chrome://filebrowse/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
