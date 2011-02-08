// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/slideshow_ui.h"

#include "base/callback.h"
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
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/web_ui_favicon_source.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/base/directory_lister.h"
#include "ui/base/resource/resource_bundle.h"

static const char kPropertyPath[] = "path";
static const char kPropertyTitle[] = "title";
static const char kPropertyOffset[] = "currentOffset";
static const char kPropertyDirectory[] = "isDirectory";

class SlideshowUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  SlideshowUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~SlideshowUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(SlideshowUIHTMLSource);
};

// The handler for Javascript messages related to the "slideshow" view.
class SlideshowHandler : public net::DirectoryLister::DirectoryListerDelegate,
                         public WebUIMessageHandler,
                         public base::SupportsWeakPtr<SlideshowHandler> {
 public:
  SlideshowHandler();
  virtual ~SlideshowHandler();

  // Init work after Attach.
  void Init();

  // DirectoryLister::DirectoryListerDelegate methods:
  virtual void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int error);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  void GetChildrenForPath(const FilePath& path, bool is_refresh);

  // Callback for the "getChildren" message.
  void HandleGetChildren(const ListValue* args);

  void HandleRefreshDirectory(const ListValue* args);

 private:
  bool PathIsImageFile(const char* filename);

  scoped_ptr<ListValue> filelist_value_;
  FilePath currentpath_;
  FilePath originalpath_;
  Profile* profile_;
  int counter_;
  int currentOffset_;
  scoped_refptr<net::DirectoryLister> lister_;
  bool is_refresh_;

  DISALLOW_COPY_AND_ASSIGN(SlideshowHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// SlideshowHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

SlideshowUIHTMLSource::SlideshowUIHTMLSource()
    : DataSource(chrome::kChromeUISlideshowHost, MessageLoop::current()) {
}

void SlideshowUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_off_the_record,
                                              int request_id) {
  DictionaryValue localized_strings;
  // TODO(dhg): Add stirings to localized strings, also add more strings
  // that are currently hardcoded.
  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece slideshow_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SLIDESHOW_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      slideshow_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// SlideshowHandler
//
////////////////////////////////////////////////////////////////////////////////
SlideshowHandler::SlideshowHandler()
    : profile_(NULL),
      is_refresh_(false) {
  lister_ = NULL;
}

SlideshowHandler::~SlideshowHandler() {
  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
  }
}

WebUIMessageHandler* SlideshowHandler::Attach(DOMUI* dom_ui) {
  // Create our favicon data source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new WebUIFavIconSource(dom_ui->GetProfile()))));
  profile_ = dom_ui->GetProfile();
  return WebUIMessageHandler::Attach(dom_ui);
}

void SlideshowHandler::Init() {
}

void SlideshowHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getChildren",
      NewCallback(this, &SlideshowHandler::HandleGetChildren));
  dom_ui_->RegisterMessageCallback("refreshDirectory",
      NewCallback(this, &SlideshowHandler::HandleRefreshDirectory));
}

void SlideshowHandler::HandleRefreshDirectory(const ListValue* args) {
#if defined(OS_CHROMEOS)
  std::string path = WideToUTF8(ExtractStringValue(args));
  GetChildrenForPath(FilePath(path), true);
#endif
}

void SlideshowHandler::GetChildrenForPath(const FilePath& path,
                                          bool is_refresh) {
  filelist_value_.reset(new ListValue());
  currentpath_ = path;

  if (lister_.get()) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }

  is_refresh_ = is_refresh;
  if (file_util::EnsureEndsWithSeparator(&currentpath_) &&
      currentpath_.IsAbsolute()) {
    lister_ = new net::DirectoryLister(currentpath_, this);
  } else {
    originalpath_ = currentpath_;
    currentpath_ = currentpath_.DirName();
    lister_ = new net::DirectoryLister(currentpath_, this);
  }
  counter_ = 0;
  currentOffset_ = -1;
  lister_->Start();
}

void SlideshowHandler::HandleGetChildren(const ListValue* args) {
#if defined(OS_CHROMEOS)
  filelist_value_.reset(new ListValue());
  std::string path = WideToUTF8(ExtractStringValue(args));
  GetChildrenForPath(FilePath(path), false);
#endif
}

bool SlideshowHandler::PathIsImageFile(const char* filename) {
#if defined(OS_CHROMEOS)
  FilePath file = FilePath(filename);
  std::string ext = file.Extension();
  ext = StringToLowerASCII(ext);
  if (ext == ".jpg" ||
      ext == ".jpeg" ||
      ext == ".png" ||
      ext == ".gif") {
    return true;
  } else {
    return false;
  }
#else
  return false;
#endif
}

void SlideshowHandler::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
#if defined(OS_CHROMEOS)
  if (data.info.filename[0] == '.') {
    return;
  }
  if (!PathIsImageFile(data.info.filename.c_str())) {
    return;
  }

  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetString(kPropertyTitle, data.info.filename);
  file_value->SetString(kPropertyPath,
                        currentpath_.Append(data.info.filename).value());
  file_value->SetBoolean(kPropertyDirectory, S_ISDIR(data.info.stat.st_mode));
  filelist_value_->Append(file_value);
  std::string val;
  file_value->GetString(kPropertyTitle, &val);
  if (val == originalpath_.BaseName().value()) {
    currentOffset_ = counter_;
  }
  counter_++;
#endif
}

void SlideshowHandler::OnListDone(int error) {
  DictionaryValue info_value;
  counter_ = 0;
  if (!(file_util::EnsureEndsWithSeparator(&originalpath_) &&
        originalpath_.IsAbsolute()) &&
      currentOffset_ != -1) {
    info_value.SetInteger(kPropertyOffset, currentOffset_);
  }
  if (is_refresh_) {
    info_value.SetString("functionCall", "refresh");
  } else {
    info_value.SetString("functionCall", "getChildren");
  }
  info_value.SetString(kPropertyPath, currentpath_.value());
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, *(filelist_value_.get()));
}

////////////////////////////////////////////////////////////////////////////////
//
// SlideshowUI
//
////////////////////////////////////////////////////////////////////////////////

SlideshowUI::SlideshowUI(TabContents* contents) : DOMUI(contents) {
  SlideshowHandler* handler = new SlideshowHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init();
  SlideshowUIHTMLSource* html_source = new SlideshowUIHTMLSource();

  // Set up the chrome://slideshow/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
