// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bug_report_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bug_report_data.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#endif

using content::BrowserThread;

namespace {

const char kScreenshotBaseUrl[] = "chrome://screenshots/";
const char kCurrentScreenshotUrl[] = "chrome://screenshots/current";
#if defined(OS_CHROMEOS)
const char kSavedScreenshotsUrl[] = "chrome://screenshots/saved/";
const char kScreenshotPattern[] = "screenshot-*.png";

const size_t kMaxSavedScreenshots = 2;
#endif

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

  file_util::FileEnumerator screenshots(fileshelf_path, false,
                                        file_util::FileEnumerator::FILES,
                                        std::string(kScreenshotPattern));
  FilePath screenshot = screenshots.Next();
  while (!screenshot.empty()) {
    saved_screenshots->push_back(std::string(kSavedScreenshotsUrl) +
                                 screenshot.BaseName().value());
    if (saved_screenshots->size() >= kMaxSavedScreenshots)
      break;

    screenshot = screenshots.Next();
  }
  done->Signal();
}

// This fuction posts a task to the file thread to create/list all the current
// and saved screenshots.
void GetScreenshotUrls(std::vector<std::string>* saved_screenshots) {
  base::WaitableEvent done(true, false);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&GetSavedScreenshots,
                                     saved_screenshots, &done));
  done.Wait();
}

std::string GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->logged_in_user().display_email();
}
#endif

// Returns the index of the feedback tab if already open, -1 otherwise
int GetIndexOfFeedbackTab(Browser* browser) {
  GURL bug_report_url(chrome::kChromeUIBugReportURL);
  for (int i = 0; i < browser->tab_count(); ++i) {
    TabContents* tab = browser->GetTabContentsAt(i);
    if (tab && tab->GetURL().GetWithEmptyPath() == bug_report_url)
      return i;
  }

  return -1;
}

}  // namespace


namespace browser {

void ShowHtmlBugReportView(Browser* browser,
                           const std::string& description_template,
                           size_t issue_type) {
  // First check if we're already open (we cannot depend on ShowSingletonTab
  // for this functionality since we need to make *sure* we never get
  // instantiated again while we are open - with singleton tabs, that can
  // happen)
  int feedback_tab_index = GetIndexOfFeedbackTab(browser);
  if (feedback_tab_index >= 0) {
    // Do not refresh screenshot, do not create a new tab
    browser->ActivateTabAt(feedback_tab_index, true);
    return;
  }

  std::vector<unsigned char>* last_screenshot_png =
      BugReportUtil::GetScreenshotPng();
  last_screenshot_png->clear();

  gfx::NativeWindow native_window = browser->window()->GetNativeHandle();
  gfx::Rect snapshot_bounds = gfx::Rect(browser->window()->GetBounds().size());
  bool success = browser::GrabWindowSnapshot(native_window,
                                             last_screenshot_png,
                                             snapshot_bounds);
  BugReportUtil::SetScreenshotSize(success ? snapshot_bounds : gfx::Rect());

  std::string bug_report_url = std::string(chrome::kChromeUIBugReportURL) +
      "#" + base::IntToString(browser->active_index()) +
      "?description=" + net::EscapeUrlEncodedData(description_template, false) +
      "&issueType=" + base::IntToString(issue_type);
  browser->ShowSingletonTab(GURL(bug_report_url));
}

}  // namespace browser

// The handler for Javascript messages related to the "bug report" dialog
class BugReportHandler : public WebUIMessageHandler,
                         public base::SupportsWeakPtr<BugReportHandler> {
 public:
  explicit BugReportHandler(TabContents* tab);
  virtual ~BugReportHandler();

  // Init work after Attach.  Returns true on success.
  bool Init();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  void HandleGetDialogDefaults(const ListValue* args);
  void HandleRefreshCurrentScreenshot(const ListValue* args);
#if defined(OS_CHROMEOS)
  void HandleRefreshSavedScreenshots(const ListValue* args);
#endif
  void HandleSendReport(const ListValue* args);
  void HandleCancel(const ListValue* args);
  void HandleOpenSystemTab(const ListValue* args);

  void SetupScreenshotsSource();
  void ClobberScreenshotsSource();

  void CancelFeedbackCollection();
  void CloseFeedbackTab();

  TabContents* tab_;
  ScreenshotSource* screenshot_source_;

  BugReportData* bug_report_data_;
  std::string target_tab_url_;
#if defined(OS_CHROMEOS)
  // Variables to track SyslogsProvider::RequestSyslogs callback.
  chromeos::system::SyslogsProvider::Handle syslogs_handle_;
  CancelableRequestConsumer syslogs_consumer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BugReportHandler);
};

ChromeWebUIDataSource* CreateBugReportUIHTMLSource(bool successful_init) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIBugReportHost);

  source->AddLocalizedString("title", IDS_BUGREPORT_TITLE);
  source->AddLocalizedString("page-title", IDS_BUGREPORT_REPORT_PAGE_TITLE);
  source->AddLocalizedString("issue-with", IDS_BUGREPORT_ISSUE_WITH);
  source->AddLocalizedString("page-url", IDS_BUGREPORT_REPORT_URL_LABEL);
  source->AddLocalizedString("description", IDS_BUGREPORT_DESCRIPTION_LABEL);
  source->AddLocalizedString("current-screenshot",
                             IDS_BUGREPORT_SCREENSHOT_LABEL);
  source->AddLocalizedString("saved-screenshot",
                             IDS_BUGREPORT_SAVED_SCREENSHOT_LABEL);
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("user-email", IDS_BUGREPORT_USER_EMAIL_LABEL);
  source->AddLocalizedString("sysinfo",
                             IDS_BUGREPORT_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
  source->AddLocalizedString("currentscreenshots",
                             IDS_BUGREPORT_CURRENT_SCREENSHOTS);
  source->AddLocalizedString("savedscreenshots",
                             IDS_BUGREPORT_SAVED_SCREENSHOTS);
  source->AddLocalizedString("choose-different-screenshot",
                             IDS_BUGREPORT_CHOOSE_DIFFERENT_SCREENSHOT);
  source->AddLocalizedString("choose-original-screenshot",
                             IDS_BUGREPORT_CHOOSE_ORIGINAL_SCREENSHOT);
#else
  source->AddLocalizedString("currentscreenshots",
                             IDS_BUGREPORT_INCLUDE_NEW_SCREEN_IMAGE);
#endif
  source->AddLocalizedString("noscreenshot",
                             IDS_BUGREPORT_INCLUDE_NO_SCREENSHOT);

  source->AddLocalizedString("send-report", IDS_BUGREPORT_SEND_REPORT);
  source->AddLocalizedString("cancel", IDS_CANCEL);

  // Option strings for the "issue with" drop-down.
  source->AddLocalizedString("issue-choose", IDS_BUGREPORT_CHOOSE_ISSUE);
  source->AddLocalizedString("no-issue-selected",
                             IDS_BUGREPORT_NO_ISSUE_SELECTED);
  source->AddLocalizedString("no-description", IDS_BUGREPORT_NO_DESCRIPTION);
  source->AddLocalizedString("no-saved-screenshots",
                             IDS_BUGREPORT_NO_SAVED_SCREENSHOTS_HELP);
  source->AddLocalizedString("privacy-note", IDS_BUGREPORT_PRIVACY_NOTE);

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
  // Autofill (hidden by default)

  source->AddLocalizedString("issue-connectivity", IDS_BUGREPORT_CONNECTIVITY);
  source->AddLocalizedString("issue-sync", IDS_BUGREPORT_SYNC);
  source->AddLocalizedString("issue-crashes", IDS_BUGREPORT_CRASHES);
  source->AddLocalizedString("issue-page-formatting",
                             IDS_BUGREPORT_PAGE_FORMATTING);
  source->AddLocalizedString("issue-extensions", IDS_BUGREPORT_EXTENSIONS);
  source->AddLocalizedString("issue-standby", IDS_BUGREPORT_STANDBY_RESUME);
  source->AddLocalizedString("issue-phishing", IDS_BUGREPORT_PHISHING_PAGE);
  source->AddLocalizedString("issue-other", IDS_BUGREPORT_GENERAL);
  source->AddLocalizedString("issue-autofill", IDS_BUGREPORT_AUTOFILL);
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
  // Autofill (hidden by default)

  source->AddLocalizedString("issue-page-formatting",
                             IDS_BUGREPORT_PAGE_FORMATTING);
  source->AddLocalizedString("issue-page-load", IDS_BUGREPORT_PAGE_LOAD);
  source->AddLocalizedString("issue-plugins", IDS_BUGREPORT_PLUGINS);
  source->AddLocalizedString("issue-tabs", IDS_BUGREPORT_TABS);
  source->AddLocalizedString("issue-sync", IDS_BUGREPORT_SYNC);
  source->AddLocalizedString("issue-crashes", IDS_BUGREPORT_CRASHES);
  source->AddLocalizedString("issue-extensions", IDS_BUGREPORT_EXTENSIONS);
  source->AddLocalizedString("issue-phishing", IDS_BUGREPORT_PHISHING_PAGE);
  source->AddLocalizedString("issue-other", IDS_BUGREPORT_OTHER);
  source->AddLocalizedString("issue-autofill", IDS_BUGREPORT_AUTOFILL);
#endif

  source->set_json_path("strings.js");
  source->add_resource_path("bug_report.js", IDR_BUGREPORT_JS);
  source->set_default_resource(
      successful_init ? IDR_BUGREPORT_HTML : IDR_BUGREPORT_HTML_INVALID);

  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// BugReportHandler
//
////////////////////////////////////////////////////////////////////////////////
BugReportHandler::BugReportHandler(TabContents* tab)
    : tab_(tab),
      screenshot_source_(NULL),
      bug_report_data_(NULL)
#if defined(OS_CHROMEOS)
    , syslogs_handle_(0)
#endif
{
}

BugReportHandler::~BugReportHandler() {
  // Just in case we didn't send off bug_report_data_ to SendReport
  if (bug_report_data_) {
    // If we're deleting the report object, cancel feedback collection first
    CancelFeedbackCollection();
    delete bug_report_data_;
  }
  // Make sure we don't leave any screenshot data around.
  BugReportUtil::ClearScreenshotPng();
}

void BugReportHandler::ClobberScreenshotsSource() {
  // Re-create our screenshots data source (this clobbers the last source)
  // setting the screenshot to NULL, effectively disabling the source
  // TODO(rkc): Once there is a method to 'remove' a source, change this code
  Profile* profile = Profile::FromBrowserContext(tab_->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(new ScreenshotSource(NULL));

  BugReportUtil::ClearScreenshotPng();
}

void BugReportHandler::SetupScreenshotsSource() {
  // If we don't already have a screenshot source object created, create one.
  if (!screenshot_source_) {
    screenshot_source_ =
        new ScreenshotSource(BugReportUtil::GetScreenshotPng());
  }
  // Add the source to the data manager.
  Profile* profile = Profile::FromBrowserContext(tab_->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(screenshot_source_);
}

WebUIMessageHandler* BugReportHandler::Attach(WebUI* web_ui) {
  SetupScreenshotsSource();
  return WebUIMessageHandler::Attach(web_ui);
}

bool BugReportHandler::Init() {
  std::string page_url;
  if (tab_->controller().GetActiveEntry()) {
     page_url = tab_->controller().GetActiveEntry()->url().spec();
  }

  std::string params = page_url.substr(strlen(chrome::kChromeUIBugReportURL));
  // Erase the # - the first character.
  if (params.length())
    params.erase(params.begin(), params.begin() + 1);

  size_t additional_params_pos = params.find('?');
  if (additional_params_pos != std::string::npos)
    params.erase(params.begin() + additional_params_pos, params.end());

  int index = 0;
  if (!base::StringToInt(params, &index))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  // Sanity checks.
  if (((index == 0) && (params != "0")) || !browser ||
      index >= browser->tab_count()) {
    return false;
  }

  TabContents* target_tab = browser->GetTabContentsAt(index);
  if (target_tab) {
    target_tab_url_ = target_tab->GetURL().spec();
  }

  // Setup the screenshot source after we've verified input is legit.
  SetupScreenshotsSource();

  return true;
}

void BugReportHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getDialogDefaults",
      base::Bind(&BugReportHandler::HandleGetDialogDefaults,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("refreshCurrentScreenshot",
      base::Bind(&BugReportHandler::HandleRefreshCurrentScreenshot,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui_->RegisterMessageCallback("refreshSavedScreenshots",
      base::Bind(&BugReportHandler::HandleRefreshSavedScreenshots,
                 base::Unretained(this)));
#endif
  web_ui_->RegisterMessageCallback("sendReport",
      base::Bind(&BugReportHandler::HandleSendReport,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("cancel",
      base::Bind(&BugReportHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("openSystemTab",
      base::Bind(&BugReportHandler::HandleOpenSystemTab,
                 base::Unretained(this)));
}

void BugReportHandler::HandleGetDialogDefaults(const ListValue*) {
  // Will delete itself when bug_report_data_->SendReport() is called.
  bug_report_data_ = new BugReportData();

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
  chromeos::system::SyslogsProvider* provider =
      chromeos::system::SyslogsProvider::GetInstance();
  if (provider) {
    syslogs_handle_ = provider->RequestSyslogs(
        true,  // don't compress.
        chromeos::system::SyslogsProvider::SYSLOGS_FEEDBACK,
        &syslogs_consumer_,
        base::Bind(&BugReportData::SyslogsComplete,
                   base::Unretained(bug_report_data_)));
  }
  // 2: user e-mail
  dialog_defaults.Append(new StringValue(GetUserEmail()));
#endif

  web_ui_->CallJavascriptFunction("setupDialogDefaults", dialog_defaults);
}

void BugReportHandler::HandleRefreshCurrentScreenshot(const ListValue*) {
  std::string current_screenshot(kCurrentScreenshotUrl);
  StringValue screenshot(current_screenshot);
  web_ui_->CallJavascriptFunction("setupCurrentScreenshot", screenshot);
}


#if defined(OS_CHROMEOS)
void BugReportHandler::HandleRefreshSavedScreenshots(const ListValue*) {
  std::vector<std::string> saved_screenshots;
  GetScreenshotUrls(&saved_screenshots);

  ListValue screenshots_list;
  for (size_t i = 0; i < saved_screenshots.size(); ++i)
    screenshots_list.Append(new StringValue(saved_screenshots[i]));
  web_ui_->CallJavascriptFunction("setupSavedScreenshots", screenshots_list);
}
#endif


void BugReportHandler::HandleSendReport(const ListValue* list_value) {
  if (!bug_report_data_) {
    LOG(ERROR) << "Bug report hasn't been intialized yet.";
    return;
  }

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
  ScreenshotDataPtr image_ptr;
  if (!screenshot_path.empty())
    image_ptr = screenshot_source_->GetCachedScreenshot(screenshot_path);

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
#endif

  // Update the data in bug_report_data_ so it can be sent
  bug_report_data_->UpdateData(Profile::FromWebUI(web_ui_)
                               , target_tab_url_
                               , problem_type
                               , page_url
                               , description
                               , image_ptr
#if defined(OS_CHROMEOS)
                               , user_email
                               , send_sys_info
                               , false // sent_report
#endif
                               );

#if defined(OS_CHROMEOS)
  // If we don't require sys_info, or we have it, or we never requested it
  // (because libcros failed to load), then send the report now.
  // Otherwise, the report will get sent when we receive sys_info.
  if (!send_sys_info || bug_report_data_->sys_info() != NULL ||
      syslogs_handle_ == 0) {
    bug_report_data_->SendReport();
  }
#else
  bug_report_data_->SendReport();
#endif
  // Lose the pointer to the BugReportData object; the object will delete itself
  // from SendReport, whether we called it, or will be called by the log
  // completion routine.
  bug_report_data_ = NULL;

  // Whether we sent the report, or if it will be sent by the Syslogs complete
  // function, close our feedback tab anyway, we have no more use for it.
  CloseFeedbackTab();
}

void BugReportHandler::HandleCancel(const ListValue*) {
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
    chromeos::system::SyslogsProvider* provider =
        chromeos::system::SyslogsProvider::GetInstance();
    if (provider)
      provider->CancelRequest(syslogs_handle_);
  }
#endif
}

void BugReportHandler::CloseFeedbackTab() {
  ClobberScreenshotsSource();

  Browser* browser = BrowserList::GetLastActive();
  if (browser) {
    browser->CloseTabContents(tab_);
  } else {
    LOG(FATAL) << "Failed to get last active browser.";
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// BugReportUI
//
////////////////////////////////////////////////////////////////////////////////
BugReportUI::BugReportUI(TabContents* tab) : HtmlDialogUI(tab) {
  BugReportHandler* handler = new BugReportHandler(tab);
  AddMessageHandler((handler)->Attach(this));

  // The handler's init will determine whether we show the error html page.
  ChromeWebUIDataSource* html_source =
      CreateBugReportUIHTMLSource(handler->Init());

  // Set up the chrome://bugreport/ source.
  Profile* profile = Profile::FromBrowserContext(tab->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}
