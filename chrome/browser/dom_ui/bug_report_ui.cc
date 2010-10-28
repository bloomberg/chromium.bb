// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/bug_report_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/bug_report_data.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/dom_ui/dom_ui_screenshot_source.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "gfx/rect.h"
#include "views/window/window.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if defined(USE_X11)
#include "app/x11_util.h"
#elif defined(OS_MACOSX)
#include "base/mac_util.h"
#elif defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/waitable_event.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/syslogs_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

static const char kScreenshotBaseUrl[] = "chrome://screenshots/";
static const char kCurrentScreenshotUrl[] = "chrome://screenshots/current";
#if defined(OS_CHROMEOS)
static const char kSavedScreenshotsUrl[] = "chrome://screenshots/saved/";

static const char kScreenshotPattern[] = "*.png";
static const char kScreenshotsRelativePath[] = "/Screenshots";
#endif

namespace {
#if defined(OS_CHROMEOS)

void GetSavedScreenshots(std::vector<std::string>* saved_screenshots,
                         base::WaitableEvent* done) {
  saved_screenshots->clear();

  FilePath fileshelf_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &fileshelf_path)) {
    done->Signal();
    return;
  }

  // TODO(rkc): Change this to use FilePath.Append() once the cros
  // issue with it is fixed
  FilePath screenshots_path(fileshelf_path.value() +
                            std::string(kScreenshotsRelativePath));
  file_util::FileEnumerator screenshots(screenshots_path, false,
                                        file_util::FileEnumerator::FILES,
                                        std::string(kScreenshotPattern));
  FilePath screenshot = screenshots.Next();
  while (!screenshot.empty()) {
    saved_screenshots->push_back(std::string(kSavedScreenshotsUrl) +
                                 screenshot.BaseName().value());
    screenshot = screenshots.Next();
  }
  done->Signal();
}

// This fuction posts a task to the file thread to create/list all the current
// and saved screenshots.
void GetScreenshotUrls(std::vector<std::string>* saved_screenshots) {
  base::WaitableEvent done(true, false);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          NewRunnableFunction(&GetSavedScreenshots,
                                              saved_screenshots, &done));
  done.Wait();
}

std::string GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->logged_in_user().email();
}

#endif
}  // namespace


namespace browser {

// TODO(rkc): Eventually find a better way to do this
std::vector<unsigned char>* last_screenshot_png = 0;
gfx::Rect screen_size;

void RefreshLastScreenshot(views::Window* parent) {
  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins).
  if (last_screenshot_png)
    last_screenshot_png->clear();
  else
    last_screenshot_png = new std::vector<unsigned char>;

#if defined(USE_X11)
  screen_size = parent->GetBounds();
  x11_util::GrabWindowSnapshot(parent->GetNativeWindow(), last_screenshot_png);
#elif defined(OS_MACOSX)
  int width = 0, height = 0;
  mac_util::GrabWindowSnapshot(parent->GetNativeWindow(), last_screenshot_png,
                               &width, &height);
#elif defined(OS_WIN)
  screen_size = parent->GetBounds();
  win_util::GrabWindowSnapshot(parent->GetNativeWindow(), last_screenshot_png);
#endif
}

// Global "display this dialog" function declared in browser_dialogs.h.
void ShowHtmlBugReportView(views::Window* parent, Browser* browser) {
  std::string bug_report_url = std::string(chrome::kChromeUIBugReportURL) +
      "#" + base::IntToString(browser->selected_index());

  RefreshLastScreenshot(parent);
  browser->ShowSingletonTab(GURL(bug_report_url));
}

}  // namespace browser


class BugReportUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit BugReportUIHTMLSource(base::StringPiece html);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  base::StringPiece bug_report_html_;
  ~BugReportUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(BugReportUIHTMLSource);
};

// The handler for Javascript messages related to the "bug report" dialog
class BugReportHandler : public DOMMessageHandler,
                         public base::SupportsWeakPtr<BugReportHandler> {
 public:
  explicit BugReportHandler(TabContents* tab);
  virtual ~BugReportHandler();

  // Init work after Attach.
  base::StringPiece Init();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

 private:
  void HandleGetDialogDefaults(const ListValue* args);
  void HandleRefreshScreenshots(const ListValue* args);
  void HandleSendReport(const ListValue* args);
  void HandleCancel(const ListValue* args);
  void HandleOpenSystemTab(const ListValue* args);

  void SetupScreenshotsSource();
  void ClobberScreenshotsSource();

  void CancelFeedbackCollection();
  void CloseFeedbackTab();

  TabContents* tab_;
  DOMUIScreenshotSource* screenshot_source_;

  BugReportData* bug_report_;
  string16 target_tab_title_;
  std::string target_tab_url_;
#if defined(OS_CHROMEOS)
  // Variables to track SyslogsLibrary::RequestSyslogs callback.
  chromeos::SyslogsLibrary::Handle syslogs_handle_;
  CancelableRequestConsumer syslogs_consumer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BugReportHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// BugReportHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

BugReportUIHTMLSource::BugReportUIHTMLSource(base::StringPiece html)
    : DataSource(chrome::kChromeUIBugReportHost, MessageLoop::current()) {
  bug_report_html_ = html;
}

void BugReportUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_off_the_record,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(std::string("title"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_TITLE));
  localized_strings.SetString(std::string("issue-with"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_ISSUE_WITH));
  localized_strings.SetString(std::string("page-url"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_REPORT_URL_LABEL));
  localized_strings.SetString(std::string("description"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_DESCRIPTION_LABEL));
  localized_strings.SetString(std::string("screenshot"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_SCREENSHOT_LABEL));
#if defined(OS_CHROMEOS)
  localized_strings.SetString(std::string("user-email"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_USER_EMAIL_LABEL));
  localized_strings.SetString(std::string("currentscreenshots"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_CURRENT_SCREENSHOTS));
  localized_strings.SetString(std::string("savedscreenshots"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_SAVED_SCREENSHOTS));
  localized_strings.SetString(std::string("sysinfo"),
      l10n_util::GetStringUTF8(
          IDS_BUGREPORT_INCLUDE_SYSTEM_INFORMATION_CHKBOX));
#else
  localized_strings.SetString(std::string("currentscreenshots"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_INCLUDE_NEW_SCREEN_IMAGE));
#endif
  localized_strings.SetString(std::string("noscreenshot"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_INCLUDE_NO_SCREENSHOT));

  localized_strings.SetString(std::string("send-report"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_SEND_REPORT));
  localized_strings.SetString(std::string("cancel"),
      l10n_util::GetStringUTF8(IDS_CANCEL));

  // Option strings for the "issue with" drop-down.
  localized_strings.SetString(std::string("issue-choose"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_CHOOSE_ISSUE));

  localized_strings.SetString(std::string("no-issue-selected"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_NO_ISSUE_SELECTED));


  // TODO(rkc): Find some way to ensure this order of dropdowns is in sync
  // with the order in the userfeedback ChromeData proto buffer
#if defined(OS_CHROMEOS)
  // Dropdown for ChromeOS:
  //
  // Connectivity
  // Sync
  // Crash
  // Page Formatting
  // Extensions or Apps
  // Standby or Resume
  // Phishing Page
  // General Feedback/Other

  localized_strings.SetString(std::string("issue-connectivity"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_CONNECTIVITY));
  localized_strings.SetString(std::string("issue-sync"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_SYNC));
  localized_strings.SetString(std::string("issue-crashes"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_CRASHES));
  localized_strings.SetString(std::string("issue-page-formatting"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PAGE_FORMATTING));
  localized_strings.SetString(std::string("issue-extensions"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_EXTENSIONS));
  localized_strings.SetString(std::string("issue-standby"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_STANDBY_RESUME));
  localized_strings.SetString(std::string("issue-phishing"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PHISHING_PAGE));
  localized_strings.SetString(std::string("issue-other"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_GENERAL));
#else
  // Dropdown for Chrome:
  //
  // Page formatting or layout
  // Pages not loading
  // Plug-ins (e.g. Adobe Flash Player, Quicktime, etc)
  // Tabs or windows
  // Synced preferences
  // Crashes
  // Extensions or apps
  // Phishing
  // Other

  localized_strings.SetString(std::string("issue-page-formatting"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PAGE_FORMATTING));
  localized_strings.SetString(std::string("issue-page-load"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PAGE_LOAD));
  localized_strings.SetString(std::string("issue-plugins"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PLUGINS));
  localized_strings.SetString(std::string("issue-tabs"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_TABS));
  localized_strings.SetString(std::string("issue-sync"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_SYNC));
  localized_strings.SetString(std::string("issue-crashes"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_CRASHES));
  localized_strings.SetString(std::string("issue-extensions"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_EXTENSIONS));
  localized_strings.SetString(std::string("issue-phishing"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_PHISHING_PAGE));
  localized_strings.SetString(std::string("issue-other"),
      l10n_util::GetStringUTF8(IDS_BUGREPORT_OTHER));
#endif

  SetFontAndTextDirection(&localized_strings);

  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      bug_report_html_, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}


////////////////////////////////////////////////////////////////////////////////
//
// BugReportData
//
////////////////////////////////////////////////////////////////////////////////
void BugReportData::SendReport() {
#if defined(OS_CHROMEOS)
  // In case we already got the syslogs and sent the report, leave
  if (sent_report_) return;
  // Set send_report_ so that no one else processes SendReport
  sent_report_ = true;
#endif

  int image_data_size = image_.size();
  char* image_data = image_data_size ?
      reinterpret_cast<char*>(&(image_.front())) : NULL;
  BugReportUtil::SendReport(profile_
                            , UTF16ToUTF8(target_tab_title_)
                            , problem_type_
                            , page_url_
                            , description_
                            , image_data
                            , image_data_size
                            , browser::screen_size.width()
                            , browser::screen_size.height()
#if defined(OS_CHROMEOS)
                            , user_email_
                            , zip_content_ ? zip_content_->c_str() : NULL
                            , zip_content_ ? zip_content_->length() : 0
                            , send_sys_info_ ? sys_info_ : NULL
#endif
                            );

#if defined(OS_CHROMEOS)
  if (sys_info_) {
    delete sys_info_;
    sys_info_ = NULL;
  }
  if (zip_content_) {
    delete zip_content_;
    zip_content_ = NULL;
  }
#endif

  // Once the report has been sent, this object has no purpose in life, delete
  // ourselves.
  delete this;
}


////////////////////////////////////////////////////////////////////////////////
//
// BugReportHandler
//
////////////////////////////////////////////////////////////////////////////////
BugReportHandler::BugReportHandler(TabContents* tab)
    : tab_(tab)
    , screenshot_source_(NULL)
#if defined(OS_CHROMEOS)
    , syslogs_handle_(0)
#endif
{
}

BugReportHandler::~BugReportHandler() {
  // Just in case we didn't send off bug_report_ to SendReport
  if (bug_report_)
    delete bug_report_;
}

void BugReportHandler::ClobberScreenshotsSource() {
  // Re-create our screenshots data source (this clobbers the last source)
  // setting the screenshot to NULL, effectively disabling the source
  // TODO(rkc): Once there is a method to 'remove' a source, change this code
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new DOMUIScreenshotSource(NULL))));

  // clobber last screenshot
  if (browser::last_screenshot_png)
    browser::last_screenshot_png->clear();
}

void BugReportHandler::SetupScreenshotsSource() {
  // If we don't already have a screenshot source object created, create one.
  if (!screenshot_source_)
    screenshot_source_ = new DOMUIScreenshotSource(
        browser::last_screenshot_png);

  // Add the source to the data manager.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(screenshot_source_)));
}

DOMMessageHandler* BugReportHandler::Attach(DOMUI* dom_ui) {
  SetupScreenshotsSource();
  return DOMMessageHandler::Attach(dom_ui);
}

base::StringPiece BugReportHandler::Init() {
  std::string page_url;
  if (tab_->controller().GetActiveEntry()) {
     page_url = tab_->controller().GetActiveEntry()->url().spec();
  }

  std::string params = page_url.substr(strlen(chrome::kChromeUIBugReportURL));
  // Erase the # - the first character.
  if (params.length())
    params.erase(params.begin(), params.begin() + 1);

  int index = 0;
  if (!base::StringToInt(params, &index)) {
    return base::StringPiece(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_BUGREPORT_HTML_INVALID));
  }

  Browser* browser = BrowserList::GetLastActive();
  // Sanity checks.
  if (((index == 0) && (params != "0")) || !browser ||
      index >= browser->tab_count()) {
    return base::StringPiece(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_BUGREPORT_HTML_INVALID));
  }

  TabContents* target_tab = browser->GetTabContentsAt(index);
  if (target_tab) {
    target_tab_title_ = target_tab->GetTitle();
    target_tab_url_ = target_tab->controller().GetActiveEntry()->url().spec();
  }

  // Setup the screenshot source after we've verified input is legit.
  SetupScreenshotsSource();

  return base::StringPiece(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BUGREPORT_HTML));
}

void BugReportHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getDialogDefaults",
      NewCallback(this, &BugReportHandler::HandleGetDialogDefaults));
  dom_ui_->RegisterMessageCallback("refreshScreenshots",
      NewCallback(this, &BugReportHandler::HandleRefreshScreenshots));
  dom_ui_->RegisterMessageCallback("sendReport",
      NewCallback(this, &BugReportHandler::HandleSendReport));
  dom_ui_->RegisterMessageCallback("cancel",
      NewCallback(this, &BugReportHandler::HandleCancel));
  dom_ui_->RegisterMessageCallback("openSystemTab",
      NewCallback(this, &BugReportHandler::HandleOpenSystemTab));
}

void BugReportHandler::HandleGetDialogDefaults(const ListValue*) {
  bug_report_ = new BugReportData();

  // send back values which the dialog js needs initially
  ListValue dialog_defaults;

  // 0: current url
  if (target_tab_url_.length())
    dialog_defaults.Append(new StringValue(target_tab_url_));
  else
    dialog_defaults.Append(new StringValue(""));

#if defined(OS_CHROMEOS)
  // 1: about:system
  dialog_defaults.Append(new StringValue(chrome::kChromeUISystemInfoURL));
  // Trigger the request for system information here.
  chromeos::SyslogsLibrary* syslogs_lib =
      chromeos::CrosLibrary::Get()->GetSyslogsLibrary();
  if (syslogs_lib) {
    syslogs_handle_ = syslogs_lib->RequestSyslogs(
        true, true, &syslogs_consumer_,
        NewCallback(bug_report_, &BugReportData::SyslogsComplete));
  }
  // 2: user e-mail
  dialog_defaults.Append(new StringValue(GetUserEmail()));
#endif

  dom_ui_->CallJavascriptFunction(L"setupDialogDefaults", dialog_defaults);
}

void BugReportHandler::HandleRefreshScreenshots(const ListValue*) {
  ListValue screenshots;
  screenshots.Append(new StringValue(std::string(kCurrentScreenshotUrl)));


#if defined(OS_CHROMEOS)
  std::vector<std::string> saved_screenshots;
  GetScreenshotUrls(&saved_screenshots);

  ListValue* saved_screenshot_list = new ListValue();
  for (size_t i = 0; i < saved_screenshots.size(); ++i)
    saved_screenshot_list->Append(new StringValue(saved_screenshots[i]));
  screenshots.Append(saved_screenshot_list);
#endif
  dom_ui_->CallJavascriptFunction(L"setupScreenshots", screenshots);
}

void BugReportHandler::HandleSendReport(const ListValue* list_value) {
  ListValue::const_iterator i = list_value->begin();
  if (i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #0 - Problem type.
  int problem_type;
  std::string problem_type_str;
  (*i)->GetAsString(&problem_type_str);
  if (!base::StringToInt(problem_type_str, &problem_type)) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }
  if (++i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #1 - Page url.
  std::string page_url;
  (*i)->GetAsString(&page_url);
  if (++i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #2 - Description.
  std::string description;
  (*i)->GetAsString(&description);
  if (++i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #3 -  Screenshot to send.
  std::string screenshot_path;
  (*i)->GetAsString(&screenshot_path);
  screenshot_path.erase(0, strlen(kScreenshotBaseUrl));

  // Get the image to send in the report.
  std::vector<unsigned char> image;
  if (screenshot_path.size() > 0) {
    image = screenshot_source_->GetScreenshot(screenshot_path);
  }

#if defined(OS_CHROMEOS)
  if (++i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #4 - User e-mail
  std::string user_email;
  (*i)->GetAsString(&user_email);
  if (++i == list_value->end()) {
    LOG(ERROR) << "Incorrect data passed to sendReport.";
    return;
  }

  // #5 - System info checkbox.
  std::string sys_info_checkbox;
  (*i)->GetAsString(&sys_info_checkbox);
  bool send_sys_info = (sys_info_checkbox == "true");

  // If we aren't sending the sys_info, cancel the gathering of the syslogs.
  if (!send_sys_info)
    CancelFeedbackCollection();

  // Update the data in bug_report_ so it can be sent
  bug_report_->UpdateData(dom_ui_->GetProfile()
                          , target_tab_url_
                          , target_tab_title_
                          , problem_type
                          , page_url
                          , description
                          , image
#if defined(OS_CHROMEOS)
                          , user_email
                          , send_sys_info
                          , false // sent_report
#endif
                          );

  // If we don't require sys_info, or we have it, or we never requested it
  // (because libcros failed to load), then send the report now.
  // Otherwise, the report will get sent when we receive sys_info.
  if (!send_sys_info || bug_report_->sys_info() != NULL ||
      syslogs_handle_ == 0) {
    bug_report_->SendReport();
  }
#else
  bug_report_->SendReport();
#endif
  // Lose the pointer to the BugReportData object; the object will delete itself
  // from SendReport, whether we called it, or will be called by the log
  // completion routine.
  bug_report_ = NULL;

  // Whether we sent the report, or if it will be sent by the Syslogs complete
  // function, close our feedback tab anyway, we have no more use for it.
  CloseFeedbackTab();
}

void BugReportHandler::HandleCancel(const ListValue*) {
  CancelFeedbackCollection();
  CloseFeedbackTab();
}

void BugReportHandler::HandleOpenSystemTab(const ListValue* args) {
#if defined(OS_CHROMEOS)
  BrowserList::GetLastActive()->OpenSystemTabAndActivate();
#endif
}

void BugReportHandler::CancelFeedbackCollection() {
#if defined(OS_CHROMEOS)
  if (syslogs_handle_ != 0) {
    chromeos::SyslogsLibrary* syslogs_lib =
        chromeos::CrosLibrary::Get()->GetSyslogsLibrary();
    if (syslogs_lib)
      syslogs_lib->CancelRequest(syslogs_handle_);
  }
#endif
}

void BugReportHandler::CloseFeedbackTab() {
  Browser* browser = BrowserList::GetLastActive();
  if (browser) {
    browser->CloseTabContents(tab_);
  } else {
    LOG(FATAL) << "Failed to get last active browser.";
  }
  ClobberScreenshotsSource();
}

////////////////////////////////////////////////////////////////////////////////
//
// BugReportUI
//
////////////////////////////////////////////////////////////////////////////////
BugReportUI::BugReportUI(TabContents* tab) : HtmlDialogUI(tab) {
  BugReportHandler* handler = new BugReportHandler(tab);
  AddMessageHandler((handler)->Attach(this));

  // The handler's init will specify which html
  // resource we'll display to the user
  BugReportUIHTMLSource* html_source =
      new BugReportUIHTMLSource(handler->Init());
  // Set up the chrome://bugreport/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
