// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback_ui.h"

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogUI;

namespace {

const char kCategoryTagParameter[] = "categoryTag=";
const char kDescriptionParameter[] = "description=";
const char kSessionIDParameter[] = "session_id=";
const char kTabIndexParameter[] = "tab_index=";
const char kCustomPageUrlParameter[] = "customPageUrl=";

#if defined(OS_CHROMEOS)

const char kTimestampParameter[] = "timestamp=";

const size_t kMaxSavedScreenshots = 2;
size_t kMaxNumScanFiles = 1000;

// Compare two screenshot filepaths, which include the screenshot timestamp
// in the format of screenshot-yyyymmdd-hhmmss.png. Return true if |filepath1|
// is more recent |filepath2|.
bool ScreenshotTimestampComp(const std::string& filepath1,
                             const std::string& filepath2) {
  return filepath1 > filepath2;
}

std::string GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->GetLoggedInUser()->display_email();
}

bool ScreenshotDriveTimestampComp(const drive::DriveEntryProto& entry1,
                                  const drive::DriveEntryProto& entry2) {
  return entry1.file_info().last_modified() >
      entry2.file_info().last_modified();
}

void ReadDirectoryCallback(size_t max_saved,
                           std::vector<std::string>* saved_screenshots,
                           base::Closure callback,
                           drive::DriveFileError error,
                           bool hide_hosted_documents,
                           scoped_ptr<drive::DriveEntryProtoVector> entries) {
  if (error != drive::DRIVE_FILE_OK) {
    callback.Run();
    return;
  }

  size_t max_scan = std::min(kMaxNumScanFiles, entries->size());
  std::vector<drive::DriveEntryProto> screenshot_entries;
  for (size_t i = 0; i < max_scan; ++i) {
    const drive::DriveEntryProto& entry = (*entries)[i];
    if (StartsWithASCII(entry.base_name(),
                        ScreenshotSource::kScreenshotPrefix, true) &&
        EndsWith(entry.base_name(),
                 ScreenshotSource::kScreenshotSuffix, true)) {
      screenshot_entries.push_back(entry);
    }
  }

  size_t sort_size = std::min(max_saved, screenshot_entries.size());
  std::partial_sort(screenshot_entries.begin(),
                    screenshot_entries.begin() + sort_size,
                    screenshot_entries.end(),
                    ScreenshotDriveTimestampComp);
  for (size_t i = 0; i < sort_size; ++i) {
    const drive::DriveEntryProto& entry = screenshot_entries[i];
    saved_screenshots->push_back(
        std::string(ScreenshotSource::kScreenshotUrlRoot) +
        std::string(ScreenshotSource::kScreenshotSaved) +
        entry.resource_id());
  }
  callback.Run();
}

#else

std::string GetUserEmail() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return std::string();

  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
  if (!signin)
    return std::string();

  return signin->GetAuthenticatedUsername();
}

#endif  // OS_CHROMEOS

// Returns the index of the feedback tab if already open, -1 otherwise
int GetIndexOfFeedbackTab(Browser* browser) {
  GURL feedback_url(chrome::kChromeUIFeedbackURL);
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(i);
    if (tab && tab->GetURL().GetWithEmptyPath() == feedback_url)
      return i;
  }

  return -1;
}

}  // namespace

namespace chrome {

void ShowFeedbackPage(Browser* browser,
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
  // happen).
  int feedback_tab_index = GetIndexOfFeedbackTab(browser);
  if (feedback_tab_index >= 0) {
    // Do not refresh screenshot, do not create a new tab.
    browser->tab_strip_model()->ActivateTabAt(feedback_tab_index, true);
    return;
  }

  if (category_tag != kAppLauncherCategoryTag) {
    std::vector<unsigned char>* last_screenshot_png =
        FeedbackUtil::GetScreenshotPng();
    last_screenshot_png->clear();

    gfx::NativeWindow native_window;
    gfx::Rect snapshot_bounds;

#if defined(OS_CHROMEOS)
    // For ChromeOS, don't use the browser window but the root window
    // instead to grab the screenshot. We want everything on the screen, not
    // just the current browser.
    native_window = ash::Shell::GetPrimaryRootWindow();
    snapshot_bounds = gfx::Rect(native_window->bounds());
#else
    native_window = browser->window()->GetNativeWindow();
    snapshot_bounds = gfx::Rect(browser->window()->GetBounds().size());
#endif
    bool success = chrome::GrabWindowSnapshotForUser(native_window,
                                                     last_screenshot_png,
                                                     snapshot_bounds);
    FeedbackUtil::SetScreenshotSize(success ? snapshot_bounds : gfx::Rect());
  }
  std::string feedback_url = std::string(chrome::kChromeUIFeedbackURL) + "?" +
      kSessionIDParameter + base::IntToString(browser->session_id().id()) +
      "&" + kTabIndexParameter +
      base::IntToString(browser->tab_strip_model()->active_index()) +
      "&" + kDescriptionParameter +
      net::EscapeUrlEncodedData(description_template, false) + "&" +
      kCategoryTagParameter + net::EscapeUrlEncodedData(category_tag, false);

#if defined(OS_CHROMEOS)
  feedback_url = feedback_url + "&" + kTimestampParameter +
                 net::EscapeUrlEncodedData(timestamp, false);
#endif
  chrome::ShowSingletonTab(browser, GURL(feedback_url));
}

}  // namespace chrome

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
  void GetMostRecentScreenshotsDrive(
      const base::FilePath& filepath,
      std::vector<std::string>* saved_screenshots,
      size_t max_saved, base::Closure callback);
  void StartSyslogsCollection();
#endif
  void HandleSendReport(const ListValue* args);
  void HandleCancel(const ListValue* args);
  void HandleOpenSystemTab(const ListValue* args);

  void SetupScreenshotsSource();
  void ClobberScreenshotsSource();

  void CloseFeedbackTab();

  WebContents* tab_;
  ScreenshotSource* screenshot_source_;

  scoped_refptr<FeedbackData> feedback_data_;
  std::string target_tab_url_;
  std::string category_tag_;
#if defined(OS_CHROMEOS)
  // Timestamp of when the feedback request was initiated.
  std::string timestamp_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FeedbackHandler);
};

content::WebUIDataSource* CreateFeedbackUIHTMLSource(bool successful_init) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFeedbackHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("title", IDS_FEEDBACK_TITLE);
  source->AddLocalizedString("page-title", IDS_FEEDBACK_REPORT_PAGE_TITLE);
  source->AddLocalizedString("page-url", IDS_FEEDBACK_REPORT_URL_LABEL);
  source->AddLocalizedString("description", IDS_FEEDBACK_DESCRIPTION_LABEL);
  source->AddLocalizedString("current-screenshot",
                             IDS_FEEDBACK_SCREENSHOT_LABEL);
  source->AddLocalizedString("saved-screenshot",
                             IDS_FEEDBACK_SAVED_SCREENSHOT_LABEL);
  source->AddLocalizedString("user-email", IDS_FEEDBACK_USER_EMAIL_LABEL);

#if defined(OS_CHROMEOS)
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
  source->AddLocalizedString("attach-file-label",
                             IDS_FEEDBACK_ATTACH_FILE_LABEL);
  source->AddLocalizedString("attach-file-note",
                             IDS_FEEDBACK_ATTACH_FILE_NOTE);
  source->AddLocalizedString("attach-file-to-big",
                             IDS_FEEDBACK_ATTACH_FILE_TO_BIG);
  source->AddLocalizedString("reading-file", IDS_FEEDBACK_READING_FILE);
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
  source->AddLocalizedString("launcher-title", IDS_FEEDBACK_LAUNCHER_TITLE);
  source->AddLocalizedString("launcher-description",
                             IDS_FEEDBACK_LAUNCHER_DESCRIPTION_LABEL);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("feedback.js", IDR_FEEDBACK_JS);
  source->SetDefaultResource(
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
      feedback_data_(NULL) {
  DCHECK(tab);
}

FeedbackHandler::~FeedbackHandler() {
  // Make sure we don't leave any screenshot data around.
  FeedbackUtil::ClearScreenshotPng();
}

void FeedbackHandler::ClobberScreenshotsSource() {
  // Re-create our screenshots data source (this clobbers the last source)
  // setting the screenshot to NULL, effectively disabling the source
  // TODO(rkc): Once there is a method to 'remove' a source, change this code
  Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
  content::URLDataSource::Add(profile, new ScreenshotSource(NULL, profile));

  FeedbackUtil::ClearScreenshotPng();
}

void FeedbackHandler::SetupScreenshotsSource() {
  Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
  screenshot_source_ =
      new ScreenshotSource(FeedbackUtil::GetScreenshotPng(), profile);
  // Add the source to the data manager.
  content::URLDataSource::Add(profile, screenshot_source_);
}

bool FeedbackHandler::Init() {
  std::string page_url;
  if (tab_->GetController().GetActiveEntry()) {
     page_url = tab_->GetController().GetActiveEntry()->GetURL().spec();
  }

  url_parse::Parsed parts;
  ParseStandardURL(page_url.c_str(), page_url.length(), &parts);

  size_t params_start = page_url.find("?");
  std::string query = page_url.substr(params_start + 1);

  int session_id = -1;
  int index = -1;

  std::vector<std::string> params;
  std::string custom_page_url;
  if (Tokenize(query, std::string("&"), &params)) {
    for (std::vector<std::string>::iterator it = params.begin();
         it != params.end(); ++it) {
      std::string query_str = *it;
      if (StartsWithASCII(query_str, std::string(kSessionIDParameter), true)) {
        ReplaceFirstSubstringAfterOffset(
            &query_str, 0, kSessionIDParameter, "");
        if (!base::StringToInt(query_str, &session_id))
          return false;
      } else if (StartsWithASCII(*it, std::string(kTabIndexParameter), true)) {
        ReplaceFirstSubstringAfterOffset(
            &query_str, 0, kTabIndexParameter, "");
        if (!base::StringToInt(query_str, &index))
          return false;
      } else if (StartsWithASCII(*it, std::string(kCustomPageUrlParameter),
                                 true)) {
        ReplaceFirstSubstringAfterOffset(
            &query_str, 0, kCustomPageUrlParameter, "");
        custom_page_url = query_str;
      } else if (StartsWithASCII(*it, std::string(kCategoryTagParameter),
                                 true)) {
        ReplaceFirstSubstringAfterOffset(
            &query_str, 0, kCategoryTagParameter, "");
        category_tag_ = query_str;
#if defined(OS_CHROMEOS)
      } else if (StartsWithASCII(*it, std::string(kTimestampParameter), true)) {
        ReplaceFirstSubstringAfterOffset(
            &query_str, 0, kTimestampParameter, "");
        timestamp_ = query_str;
#endif
      }
    }
  }

  // If we don't have a page url specified, get it from the tab index.
  if (custom_page_url.empty()) {
    if (session_id == -1)
      return false;

    Browser* browser = chrome::FindBrowserWithID(session_id);
    // Sanity checks.
    if (!browser || index >= browser->tab_strip_model()->count())
      return false;

    if (index >= 0) {
      WebContents* target_tab =
          browser->tab_strip_model()->GetWebContentsAt(index);
      if (target_tab)
        target_tab_url_ = target_tab->GetURL().spec();
    }

    // Note: We don't need to setup a screenshot source if we're using a
    // custom page URL since we were invoked from JS/an extension, in which
    // case we don't actually have a screenshot anyway.
    SetupScreenshotsSource();
  } else {
    target_tab_url_ = custom_page_url;
  }

  return true;
}

void FeedbackHandler::RegisterMessages() {
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
  feedback_data_ = new FeedbackData();

  // Send back values which the dialog js needs initially.
  DictionaryValue dialog_defaults;

  if (category_tag_ == chrome::kAppLauncherCategoryTag)
    dialog_defaults.SetBoolean("launcherFeedback", true);

  // Current url.
  dialog_defaults.SetString("currentUrl", target_tab_url_);

  // Are screenshots disabled?
  dialog_defaults.SetBoolean(
      "disableScreenshots",
      g_browser_process->local_state()->GetBoolean(prefs::kDisableScreenshots));

  // User e-mail
  std::string user_email = GetUserEmail();
  dialog_defaults.SetString("userEmail", user_email);

  // Set email checkbox to checked by default for cros, unchecked for Chrome.
  dialog_defaults.SetBoolean(
      "emailCheckboxDefault",
#if defined(OS_CHROMEOS)
      true);
#else
      false);
#endif


#if defined(OS_CHROMEOS)
  feedback_data_->StartSyslogsCollection();

  // On ChromeOS if the user's email is blank, it means we don't
  // have a logged in user, hence don't use saved screenshots.
  dialog_defaults.SetBoolean("useSaved", !user_email.empty());
#endif

  web_ui()->CallJavascriptFunction("setupDialogDefaults", dialog_defaults);
}

void FeedbackHandler::HandleRefreshCurrentScreenshot(const ListValue*) {
  std::string current_screenshot(
          std::string(ScreenshotSource::kScreenshotUrlRoot) +
          std::string(ScreenshotSource::kScreenshotCurrent));
  StringValue screenshot(current_screenshot);
  web_ui()->CallJavascriptFunction("setupCurrentScreenshot", screenshot);
}

#if defined(OS_CHROMEOS)
void FeedbackHandler::HandleRefreshSavedScreenshots(const ListValue*) {
  std::vector<std::string>* saved_screenshots = new std::vector<std::string>;
  base::FilePath filepath = DownloadPrefs::FromBrowserContext(
      tab_->GetBrowserContext())->DownloadPath();
  base::Closure refresh_callback = base::Bind(
      &FeedbackHandler::RefreshSavedScreenshotsCallback,
      AsWeakPtr(), base::Owned(saved_screenshots));
  if (drive::util::IsUnderDriveMountPoint(filepath)) {
    GetMostRecentScreenshotsDrive(
        filepath, saved_screenshots, kMaxSavedScreenshots, refresh_callback);
  } else {
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&FeedbackUI::GetMostRecentScreenshots, filepath,
                   base::Unretained(saved_screenshots), kMaxSavedScreenshots),
        refresh_callback);
  }
}

void FeedbackHandler::RefreshSavedScreenshotsCallback(
    std::vector<std::string>* saved_screenshots) {
  ListValue screenshots_list;
  for (size_t i = 0; i < saved_screenshots->size(); ++i)
    screenshots_list.Append(new StringValue((*saved_screenshots)[i]));
  web_ui()->CallJavascriptFunction("setupSavedScreenshots", screenshots_list);
}

void FeedbackHandler::GetMostRecentScreenshotsDrive(
    const base::FilePath& filepath, std::vector<std::string>* saved_screenshots,
    size_t max_saved, base::Closure callback) {
  drive::DriveFileSystemInterface* file_system =
      drive::DriveSystemServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()))->file_system();
  file_system->ReadDirectoryByPath(
      drive::util::ExtractDrivePath(filepath),
      base::Bind(&ReadDirectoryCallback, max_saved, saved_screenshots,
                 callback));
}
#endif


void FeedbackHandler::HandleSendReport(const ListValue* list_value) {
  if (!feedback_data_) {
    LOG(ERROR) << "Bug report hasn't been intialized yet.";
    return;
  }

  ListValue::const_iterator i = list_value->begin();
  std::string page_url;
  (*i++)->GetAsString(&page_url);
  std::string category_tag;
  (*i++)->GetAsString(&category_tag);
  std::string description;
  (*i++)->GetAsString(&description);
  std::string user_email;
  (*i++)->GetAsString(&user_email);
  std::string screenshot_path;
  (*i++)->GetAsString(&screenshot_path);
  screenshot_path.erase(0, strlen(ScreenshotSource::kScreenshotUrlRoot));

  // Get the image to send in the report.
  ScreenshotDataPtr image_ptr;
  if (!screenshot_path.empty() &&  screenshot_source_)
    image_ptr = screenshot_source_->GetCachedScreenshot(screenshot_path);

#if defined(OS_CHROMEOS)
  std::string sys_info_checkbox;
  (*i++)->GetAsString(&sys_info_checkbox);
  bool send_sys_info = (sys_info_checkbox == "true");

  std::string attached_filename;
  scoped_ptr<std::string> attached_filedata;
  // If we have an attached file, we'll still have more data in the list.
  if (i != list_value->end()) {
    (*i++)->GetAsString(&attached_filename);
    if (base::FilePath::IsSeparator(attached_filename[0])) {
      // We have an attached filepath, not filename, hence we need read this
      // this file in chrome. We won't have any file data, skip over it.
      i++;
    } else {
      std::string encoded_filedata;
      attached_filedata.reset(new std::string);
      (*i++)->GetAsString(&encoded_filedata);
      if (!base::Base64Decode(
          base::StringPiece(encoded_filedata), attached_filedata.get())) {
        LOG(ERROR) << "Unable to attach file: " << attached_filename;
        // Clear the filename so feedback_util doesn't try to attach the file.
        attached_filename = "";
      }
    }
  }
#endif

  // TODO(rkc): We are not setting the category tag here since this
  // functionality is broken on the feedback server side. Fix this once the
  // issue is resolved.
  feedback_data_->set_description(description);
  feedback_data_->set_image(image_ptr);
  feedback_data_->set_page_url(page_url);
  feedback_data_->set_profile(Profile::FromWebUI(web_ui()));
  feedback_data_->set_user_email(user_email);
#if defined(OS_CHROMEOS)
  feedback_data_->set_attached_filedata(attached_filedata.Pass());
  feedback_data_->set_attached_filename(attached_filename);
  feedback_data_->set_send_sys_info(send_sys_info);
  feedback_data_->set_timestamp(timestamp_);
#endif

  // Signal the feedback object that the data from the feedback page has been
  // filled - the object will manage sending of the actual report.
  feedback_data_->FeedbackPageDataComplete();

  CloseFeedbackTab();
}

void FeedbackHandler::HandleCancel(const ListValue*) {
  CloseFeedbackTab();
}

void FeedbackHandler::HandleOpenSystemTab(const ListValue* args) {
#if defined(OS_CHROMEOS)
  web_ui()->GetWebContents()->GetDelegate()->OpenURLFromTab(
      web_ui()->GetWebContents(),
      content::OpenURLParams(GURL(chrome::kChromeUISystemInfoURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
#endif
}

void FeedbackHandler::CloseFeedbackTab() {
  ClobberScreenshotsSource();
  tab_->GetDelegate()->CloseContents(tab_);
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
  content::WebUIDataSource* html_source =
      CreateFeedbackUIHTMLSource(handler->Init());

  // Set up the chrome://feedback/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

#if defined(OS_CHROMEOS)
// static
void FeedbackUI::GetMostRecentScreenshots(
    const base::FilePath& filepath,
    std::vector<std::string>* saved_screenshots,
    size_t max_saved) {
  std::string pattern =
      std::string(ScreenshotSource::kScreenshotPrefix) + "*" +
                  ScreenshotSource::kScreenshotSuffix;
  file_util::FileEnumerator screenshots(filepath, false,
                                        file_util::FileEnumerator::FILES,
                                        pattern);
  base::FilePath screenshot = screenshots.Next();

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
    saved_screenshots->push_back(
        std::string(ScreenshotSource::kScreenshotUrlRoot) +
        std::string(ScreenshotSource::kScreenshotSaved) +
        screenshot_filepaths[i]);
}
#endif
