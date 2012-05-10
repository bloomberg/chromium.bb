// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kScreenshotBaseUrl[] = "chrome://screenshots/";
const char kCurrentScreenshotUrl[] = "chrome://screenshots/current";

const char kCategoryTagParameter[] = "categoryTag=";
const char kDescriptionParameter[] = "description=";

#if defined(OS_CHROMEOS)
const char kSavedScreenshotsUrl[] = "chrome://screenshots/saved/";
const char kScreenshotPattern[] = "Screenshot_*.png";

const char kTimestampParameter[] = "timestamp=";

const size_t kMaxSavedScreenshots = 2;
#endif

#if defined(OS_CHROMEOS)

// Compare two screenshot filepaths, which include the screenshot timestamp
// in the format of screenshot-yyyymmdd-hhmmss.png. Return true if |filepath1|
// is more recent |filepath2|.
bool ScreenshotTimestampComp(const std::string& filepath1,
                             const std::string& filepath2) {
  return filepath1 > filepath2;
}

void GetSavedScreenshots(std::vector<std::string>* saved_screenshots) {
  saved_screenshots->clear();

  FilePath fileshelf_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &fileshelf_path))
    return;

  FeedbackUI::GetMostRecentScreenshots(fileshelf_path, saved_screenshots,
                                       kMaxSavedScreenshots);
}

std::string GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->GetLoggedInUser().display_email();
}

#endif  // OS_CHROMEOS

// Returns the index of the feedback tab if already open, -1 otherwise
int GetIndexOfFeedbackTab(Browser* browser) {
  GURL feedback_url(chrome::kChromeUIFeedbackURL);
  for (int i = 0; i < browser->tab_count(); ++i) {
    WebContents* tab = browser->GetTabContentsWrapperAt(i)->web_contents();
    if (tab && tab->GetURL().GetWithEmptyPath() == feedback_url)
      return i;
  }

  return -1;
}

}  // namespace


namespace browser {

void ShowWebFeedbackView(Browser* browser,
                         const std::string& description_template,
                         const std::string& category_tag) {
#if defined(OS_CHROMEOS)
  // Grab the timestamp before we do anything else - this is crucial to help
  // diagnose some hardware issues.
  base::Time now = base::Time::Now();
  std::string timestamp = base::DoubleToString(now.ToDoubleT());
#endif

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
      FeedbackUtil::GetScreenshotPng();
  last_screenshot_png->clear();

  gfx::NativeWindow native_window = browser->window()->GetNativeHandle();
  gfx::Rect snapshot_bounds = gfx::Rect(browser->window()->GetBounds().size());
  bool success = browser::GrabWindowSnapshot(native_window,
                                             last_screenshot_png,
                                             snapshot_bounds);
  FeedbackUtil::SetScreenshotSize(success ? snapshot_bounds : gfx::Rect());

  std::string feedback_url = std::string(chrome::kChromeUIFeedbackURL) +
      "#" + base::IntToString(browser->active_index()) + "?" +
      kDescriptionParameter +
      net::EscapeUrlEncodedData(description_template, false) + "&" +
      kCategoryTagParameter + net::EscapeUrlEncodedData(category_tag, false);

#if defined(OS_CHROMEOS)
  feedback_url = feedback_url + "&" + kTimestampParameter +
                 net::EscapeUrlEncodedData(timestamp, false);
#endif
  browser->ShowSingletonTab(GURL(feedback_url));
}

}  // namespace browser

// The handler for Javascript messages related to the "bug report" dialog
class FeedbackHandler : public WebUIMessageHandler,
                         public base::SupportsWeakPtr<FeedbackHandler> {
 public:
  explicit FeedbackHandler(content::WebContents* tab);
  virtual ~FeedbackHandler();

  // Init work after Attach.  Returns true on success.
  bool Init();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void HandleGetDialogDefaults(const ListValue* args);
  void HandleRefreshCurrentScreenshot(const ListValue* args);
#if defined(OS_CHROMEOS)
  void HandleRefreshSavedScreenshots(const ListValue* args);
  void RefreshSavedScreenshotsCallback(
      std::vector<std::string>* saved_screenshots);
#endif
  void HandleSendReport(const ListValue* args);
  void HandleCancel(const ListValue* args);
  void HandleOpenSystemTab(const ListValue* args);

  void SetupScreenshotsSource();
  void ClobberScreenshotsSource();

  void CancelFeedbackCollection();
  void CloseFeedbackTab();

  WebContents* tab_;
  ScreenshotSource* screenshot_source_;

  FeedbackData* feedback_data_;
  std::string target_tab_url_;
#if defined(OS_CHROMEOS)
  // Variables to track SyslogsProvider::RequestSyslogs callback.
  chromeos::system::SyslogsProvider::Handle syslogs_handle_;
  CancelableRequestConsumer syslogs_consumer_;

  // Timestamp of when the feedback request was initiated.
  std::string timestamp_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FeedbackHandler);
};

ChromeWebUIDataSource* CreateFeedbackUIHTMLSource(bool successful_init) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIFeedbackHost);

  source->AddLocalizedString("title", IDS_FEEDBACK_TITLE);
  source->AddLocalizedString("page-title", IDS_FEEDBACK_REPORT_PAGE_TITLE);
  source->AddLocalizedString("page-url", IDS_FEEDBACK_REPORT_URL_LABEL);
  source->AddLocalizedString("description", IDS_FEEDBACK_DESCRIPTION_LABEL);
  source->AddLocalizedString("current-screenshot",
                             IDS_FEEDBACK_SCREENSHOT_LABEL);
  source->AddLocalizedString("saved-screenshot",
                             IDS_FEEDBACK_SAVED_SCREENSHOT_LABEL);
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("user-email", IDS_FEEDBACK_USER_EMAIL_LABEL);
  source->AddLocalizedString("sysinfo",
                             IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
  source->AddLocalizedString("currentscreenshots",
                             IDS_FEEDBACK_CURRENT_SCREENSHOTS);
  source->AddLocalizedString("savedscreenshots",
                             IDS_FEEDBACK_SAVED_SCREENSHOTS);
  source->AddLocalizedString("choose-different-screenshot",
                             IDS_FEEDBACK_CHOOSE_DIFFERENT_SCREENSHOT);
  source->AddLocalizedString("choose-original-screenshot",
                             IDS_FEEDBACK_CHOOSE_ORIGINAL_SCREENSHOT);
#else
  source->AddLocalizedString("currentscreenshots",
                             IDS_FEEDBACK_INCLUDE_NEW_SCREEN_IMAGE);
#endif
  source->AddLocalizedString("noscreenshot",
                             IDS_FEEDBACK_INCLUDE_NO_SCREENSHOT);

  source->AddLocalizedString("send-report", IDS_FEEDBACK_SEND_REPORT);
  source->AddLocalizedString("cancel", IDS_CANCEL);

  source->AddLocalizedString("no-description", IDS_FEEDBACK_NO_DESCRIPTION);
  source->AddLocalizedString("no-saved-screenshots",
                             IDS_FEEDBACK_NO_SAVED_SCREENSHOTS_HELP);
  source->AddLocalizedString("privacy-note", IDS_FEEDBACK_PRIVACY_NOTE);

  source->set_json_path("strings.js");
  source->add_resource_path("feedback.js", IDR_FEEDBACK_JS);
  source->set_default_resource(
      successful_init ? IDR_FEEDBACK_HTML : IDR_FEEDBACK_HTML_INVALID);

  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// FeedbackHandler
//
////////////////////////////////////////////////////////////////////////////////
FeedbackHandler::FeedbackHandler(WebContents* tab)
    : tab_(tab),
      screenshot_source_(NULL),
      feedback_data_(NULL)
#if defined(OS_CHROMEOS)
    , syslogs_handle_(0)
#endif
{
}

FeedbackHandler::~FeedbackHandler() {
  // Just in case we didn't send off feedback_data_ to SendReport
  if (feedback_data_) {
    // If we're deleting the report object, cancel feedback collection first
    CancelFeedbackCollection();
    delete feedback_data_;
  }
  // Make sure we don't leave any screenshot data around.
  FeedbackUtil::ClearScreenshotPng();
}

void FeedbackHandler::ClobberScreenshotsSource() {
  // Re-create our screenshots data source (this clobbers the last source)
  // setting the screenshot to NULL, effectively disabling the source
  // TODO(rkc): Once there is a method to 'remove' a source, change this code
  Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
  ChromeURLDataManager::AddDataSource(profile, new ScreenshotSource(NULL));

  FeedbackUtil::ClearScreenshotPng();
}

void FeedbackHandler::SetupScreenshotsSource() {
  // If we don't already have a screenshot source object created, create one.
  if (!screenshot_source_) {
    screenshot_source_ =
        new ScreenshotSource(FeedbackUtil::GetScreenshotPng());
  }
  // Add the source to the data manager.
  Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
  ChromeURLDataManager::AddDataSource(profile, screenshot_source_);
}

bool FeedbackHandler::Init() {
  std::string page_url;
  if (tab_->GetController().GetActiveEntry()) {
     page_url = tab_->GetController().GetActiveEntry()->GetURL().spec();
  }

  url_parse::Parsed parts;
  ParseStandardURL(page_url.c_str(), page_url.length(), &parts);

  size_t params_start = page_url.find("?");
  std::string index_str = page_url.substr(parts.ref.begin,
                                          params_start - parts.ref.begin);
  std::string query = page_url.substr(params_start + 1);

  int index = 0;
  if (!base::StringToInt(index_str, &index))
    return false;

#if defined(OS_CHROMEOS)
  std::vector<std::string> params;
  if (Tokenize(query, std::string("&"), &params)) {
    for (std::vector<std::string>::iterator it = params.begin();
         it != params.end(); ++it) {
      if (StartsWithASCII(*it, std::string(kTimestampParameter), true)) {
        timestamp_ = *it;
        ReplaceFirstSubstringAfterOffset(&timestamp_,
                                         0,
                                         kTimestampParameter,
                                         "");
        break;
      }
    }
  }
#endif

  Browser* browser = BrowserList::GetLastActive();
  // Sanity checks.
  if (((index == 0) && (index_str != "0")) || !browser ||
      index >= browser->tab_count()) {
    return false;
  }

  WebContents* target_tab =
      browser->GetTabContentsWrapperAt(index)->web_contents();
  if (target_tab) {
    target_tab_url_ = target_tab->GetURL().spec();
  }

  // Setup the screenshot source after we've verified input is legit.
  SetupScreenshotsSource();

  return true;
}

void FeedbackHandler::RegisterMessages() {
  SetupScreenshotsSource();

  web_ui()->RegisterMessageCallback("getDialogDefaults",
      base::Bind(&FeedbackHandler::HandleGetDialogDefaults,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("refreshCurrentScreenshot",
      base::Bind(&FeedbackHandler::HandleRefreshCurrentScreenshot,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("refreshSavedScreenshots",
      base::Bind(&FeedbackHandler::HandleRefreshSavedScreenshots,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback("sendReport",
      base::Bind(&FeedbackHandler::HandleSendReport,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("cancel",
      base::Bind(&FeedbackHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openSystemTab",
      base::Bind(&FeedbackHandler::HandleOpenSystemTab,
                 base::Unretained(this)));
}

void FeedbackHandler::HandleGetDialogDefaults(const ListValue*) {
  // Will delete itself when feedback_data_->SendReport() is called.
  feedback_data_ = new FeedbackData();

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
        base::Bind(&FeedbackData::SyslogsComplete,
                   base::Unretained(feedback_data_)));
  }
  // 2: user e-mail
  dialog_defaults.Append(new StringValue(GetUserEmail()));
#endif

  web_ui()->CallJavascriptFunction("setupDialogDefaults", dialog_defaults);
}

void FeedbackHandler::HandleRefreshCurrentScreenshot(const ListValue*) {
  std::string current_screenshot(kCurrentScreenshotUrl);
  StringValue screenshot(current_screenshot);
  web_ui()->CallJavascriptFunction("setupCurrentScreenshot", screenshot);
}


#if defined(OS_CHROMEOS)
void FeedbackHandler::HandleRefreshSavedScreenshots(const ListValue*) {
  std::vector<std::string>* saved_screenshots = new std::vector<std::string>;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetSavedScreenshots, base::Unretained(saved_screenshots)),
      base::Bind(&FeedbackHandler::RefreshSavedScreenshotsCallback,
                 base::Unretained(this), base::Owned(saved_screenshots)));
}

void FeedbackHandler::RefreshSavedScreenshotsCallback(
    std::vector<std::string>* saved_screenshots) {
  ListValue screenshots_list;
  for (size_t i = 0; i < saved_screenshots->size(); ++i)
    screenshots_list.Append(new StringValue((*saved_screenshots)[i]));
  web_ui()->CallJavascriptFunction("setupSavedScreenshots", screenshots_list);
}

#endif


void FeedbackHandler::HandleSendReport(const ListValue* list_value) {
  if (!feedback_data_) {
    LOG(ERROR) << "Bug report hasn't been intialized yet.";
    return;
  }
  // TODO(rkc): Find a better way to do this check.
#if defined(OS_CHROMEOS)
  if (list_value->GetSize() != 6) {
#else
  if (list_value->GetSize() != 4) {
#endif
    LOG(ERROR) << "Feedback data corrupt! Feedback not sent.";
    return;
  }

  ListValue::const_iterator i = list_value->begin();
  std::string page_url;
  (*i++)->GetAsString(&page_url);
  std::string category_tag;
  (*i++)->GetAsString(&category_tag);
  std::string description;
  (*i++)->GetAsString(&description);
  std::string screenshot_path;
  (*i++)->GetAsString(&screenshot_path);
  screenshot_path.erase(0, strlen(kScreenshotBaseUrl));

  // Get the image to send in the report.
  ScreenshotDataPtr image_ptr;
  if (!screenshot_path.empty())
    image_ptr = screenshot_source_->GetCachedScreenshot(screenshot_path);

#if defined(OS_CHROMEOS)
  std::string user_email;
  (*i++)->GetAsString(&user_email);
  std::string sys_info_checkbox;
  (*i++)->GetAsString(&sys_info_checkbox);
  bool send_sys_info = (sys_info_checkbox == "true");

  // If we aren't sending the sys_info, cancel the gathering of the syslogs.
  if (!send_sys_info)
    CancelFeedbackCollection();
#endif

  // Update the data in feedback_data_ so it can be sent
  feedback_data_->UpdateData(Profile::FromWebUI(web_ui())
                               , target_tab_url_
                               , std::string()
                               , page_url
                               , description
                               , image_ptr
#if defined(OS_CHROMEOS)
                               , user_email
                               , send_sys_info
                               , false  // sent_report
                               , timestamp_
#endif
                               );

#if defined(OS_CHROMEOS)
  // If we don't require sys_info, or we have it, or we never requested it
  // (because libcros failed to load), then send the report now.
  // Otherwise, the report will get sent when we receive sys_info.
  if (!send_sys_info || feedback_data_->sys_info() != NULL ||
      syslogs_handle_ == 0) {
    feedback_data_->SendReport();
  }
#else
  feedback_data_->SendReport();
#endif
  // Lose the pointer to the FeedbackData object; the object will delete itself
  // from SendReport, whether we called it, or will be called by the log
  // completion routine.
  feedback_data_ = NULL;

  // Whether we sent the report, or if it will be sent by the Syslogs complete
  // function, close our feedback tab anyway, we have no more use for it.
  CloseFeedbackTab();
}

void FeedbackHandler::HandleCancel(const ListValue*) {
  CloseFeedbackTab();
}

void FeedbackHandler::HandleOpenSystemTab(const ListValue* args) {
#if defined(OS_CHROMEOS)
  Browser* last_active = BrowserList::GetLastActive();
  last_active->OpenURL(
      content::OpenURLParams(GURL(chrome::kChromeUISystemInfoURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
  last_active->window()->Activate();
#endif
}

void FeedbackHandler::CancelFeedbackCollection() {
#if defined(OS_CHROMEOS)
  if (syslogs_handle_ != 0) {
    chromeos::system::SyslogsProvider* provider =
        chromeos::system::SyslogsProvider::GetInstance();
    if (provider && syslogs_consumer_.HasPendingRequests())
      provider->CancelRequest(syslogs_handle_);
  }
#endif
}

void FeedbackHandler::CloseFeedbackTab() {
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
// FeedbackUI
//
////////////////////////////////////////////////////////////////////////////////
FeedbackUI::FeedbackUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  FeedbackHandler* handler = new FeedbackHandler(web_ui->GetWebContents());
  web_ui->AddMessageHandler(handler);

  // The handler's init will determine whether we show the error html page.
  ChromeWebUIDataSource* html_source =
      CreateFeedbackUIHTMLSource(handler->Init());

  // Set up the chrome://feedback/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, html_source);
}

#if defined(OS_CHROMEOS)
// static
void FeedbackUI::GetMostRecentScreenshots(
    const FilePath& filepath,
    std::vector<std::string>* saved_screenshots,
    size_t max_saved) {
  file_util::FileEnumerator screenshots(filepath, false,
                                        file_util::FileEnumerator::FILES,
                                        std::string(kScreenshotPattern));
  FilePath screenshot = screenshots.Next();

  std::vector<std::string> screenshot_filepaths;
  while (!screenshot.empty()) {
    screenshot_filepaths.push_back(screenshot.BaseName().value());
    screenshot = screenshots.Next();
  }

  size_t sort_size = std::min(max_saved, screenshot_filepaths.size());
  std::partial_sort(screenshot_filepaths.begin(),
                    screenshot_filepaths.begin() + sort_size,
                    screenshot_filepaths.end(),
                    ScreenshotTimestampComp);
  for (size_t i = 0; i < sort_size; ++i)
    saved_screenshots->push_back(std::string(kSavedScreenshotsUrl) +
                                   screenshot_filepaths[i]);
}
#endif
