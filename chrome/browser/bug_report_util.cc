// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_util.h"

#include <sstream>
#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/file_util.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/url_request/url_request_status.h"
#include "unicode/locid.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/notifications/system_notification.h"
#endif

namespace {

const int kBugReportVersion = 1;

const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";

// URL to post bug reports to.
static char const kBugReportPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

static char const kProtBufMimeType[] = "application/x-protobuf";
static char const kPngMimeType[] = "image/png";

// Tags we use in product specific data
static char const kPageTitleTag[] = "PAGE TITLE";
static char const kProblemTypeIdTag[] = "PROBLEM TYPE ID";
static char const kProblemTypeTag[] = "PROBLEM TYPE";
static char const kChromeVersionTag[] = "CHROME VERSION";
static char const kOsVersionTag[] = "OS VERSION";

static char const kNotificationId[] = "feedback.chromeos";

static int const kHttpPostSuccessNoContent = 204;
static int const kHttpPostFailNoConnection = -1;
static int const kHttpPostFailClientError = 400;
static int const kHttpPostFailServerError = 500;

#if defined(OS_CHROMEOS)
static char const kBZip2MimeType[] = "application/x-bzip2";
static char const kLogsAttachmentName[] = "system_logs.bz2";
// Maximum number of lines in system info log chunk to be still included
// in product specific data.
const size_t kMaxLineCount       = 10;
// Maximum number of bytes in system info log chunk to be still included
// in product specific data.
const size_t kMaxSystemLogLength = 1024;
#endif

}  // namespace


#if defined(OS_CHROMEOS)
class FeedbackNotification {
 public:
  // Note: notification will show only on one profile at a time.
  void Show(Profile* profile, const string16& message, bool urgent) {
    if (notification_.get()) {
        notification_->Hide();
    }
    notification_.reset(
        new chromeos::SystemNotification(profile, kNotificationId,
                               IDR_STATUSBAR_FEEDBACK,
                               l10n_util::GetStringUTF16(
                                   IDS_BUGREPORT_NOTIFICATION_TITLE)));
    notification_->Show(message, urgent, false);
  }

 private:
  FeedbackNotification() {}
  friend struct DefaultSingletonTraits<FeedbackNotification>;

  scoped_ptr<chromeos::SystemNotification> notification_;
  DISALLOW_COPY_AND_ASSIGN(FeedbackNotification);
};
#endif

// Simple URLFetcher::Delegate to clean up URLFetcher on completion.
class BugReportUtil::PostCleanup : public URLFetcher::Delegate {
 public:
#if defined(OS_CHROMEOS)
  explicit PostCleanup(Profile* profile);
#else
  PostCleanup();
#endif
  // Overridden from URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 protected:
  virtual ~PostCleanup() {}

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PostCleanup);
};

#if defined(OS_CHROMEOS)
  BugReportUtil::PostCleanup::PostCleanup(Profile* profile)
      : profile_(profile) {
#else
  BugReportUtil::PostCleanup::PostCleanup() : profile_(NULL) {
#endif
}

void BugReportUtil::PostCleanup::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {

  std::stringstream error_stream;
  if (response_code == kHttpPostSuccessNoContent) {
    error_stream << "Success";
  } else if (response_code == kHttpPostFailNoConnection) {
    error_stream << "No connection to server.";
  } else if ((response_code > kHttpPostFailClientError) &&
      (response_code < kHttpPostFailServerError)) {
    error_stream << "Client error: HTTP response code " << response_code;
  } else if (response_code > kHttpPostFailServerError) {
    error_stream << "Server error: HTTP response code " << response_code;
  } else {
    error_stream << "Unknown error: HTTP response code " << response_code;
  }

  LOG(WARNING) << "Submission to feedback server (" << url
               << ") status: " << error_stream.str();

#if defined(OS_CHROMEOS)
  // Show the notification to the user; this notification will stay active till
  // either the user closes it, or we display another notification.
  if (response_code == kHttpPostSuccessNoContent) {
    Singleton<FeedbackNotification>()->Show(profile_, l10n_util::GetStringUTF16(
        IDS_BUGREPORT_FEEDBACK_STATUS_SUCCESS), false);
  } else {
    Singleton<FeedbackNotification>()->Show(profile_,
        l10n_util::GetStringFUTF16(IDS_BUGREPORT_FEEDBACK_STATUS_FAIL,
                                   ASCIIToUTF16(error_stream.str())),
        true);
  }
#endif

  // Delete the URLFetcher.
  delete source;
  // And then delete ourselves.
  delete this;
}

// static
void BugReportUtil::SetOSVersion(std::string *os_version) {
#if defined(OS_WIN)
  OSVERSIONINFO osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (GetVersionEx(&osvi)) {
    *os_version = StringPrintf("%d.%d.%d %S",
      osvi.dwMajorVersion,
      osvi.dwMinorVersion,
      osvi.dwBuildNumber,
      osvi.szCSDVersion);
  } else {
    *os_version = "unknown";
  }
#elif defined(OS_MACOSX)
  int32 major;
  int32 minor;
  int32 bugFix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugFix);
  *os_version = StringPrintf("%d.%d.%d", major, minor, bugFix);
#else
  *os_version = "unknown";
#endif
}

// static
std::string BugReportUtil::feedback_server_("");

// static
void BugReportUtil::SetFeedbackServer(const std::string& server) {
  feedback_server_ = server;
}


// static
void BugReportUtil::AddFeedbackData(
    userfeedback::ExternalExtensionSubmit* feedback_data,
    const std::string& key, const std::string& value) {
  // We have no reason to log any empty values - gives us no data
  if (value == "") return;
  // Create log_value object and add it to the web_data object
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

#if defined(OS_CHROMEOS)
bool BugReportUtil::ValidFeedbackSize(const std::string& content) {
  if (content.length() > kMaxSystemLogLength)
    return false;
  size_t line_count = 0;
  const char* text = content.c_str();
  for (size_t i = 0; i < content.length(); i++) {
    if (*(text + i) == '\n') {
      line_count++;
      if (line_count > kMaxLineCount)
        return false;
    }
  }
  return true;
}
#endif

// static
void BugReportUtil::SendReport(Profile* profile,
    const std::string& page_title_text,
    int problem_type,
    const std::string& page_url_text,
    const std::string& description,
    const char* png_data,
    int png_data_length,
    int png_width,
#if defined(OS_CHROMEOS)
    int png_height,
    const std::string& user_email_text,
    const char* zipped_logs_data,
    int zipped_logs_length,
    const chromeos::LogDictionaryType* const sys_info) {
#else
    int png_height) {
#endif
  GURL post_url;

  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kFeedbackServer))
    post_url = GURL(CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kFeedbackServer));
  else
    post_url = GURL(kBugReportPostUrl);

  // Create google feedback protocol buffer objects
  userfeedback::ExternalExtensionSubmit feedback_data;
  // type id set to 0, unused field but needs to be initialized to 0
  feedback_data.set_type_id(0);

  userfeedback::CommonData* common_data = feedback_data.mutable_common_data();
  userfeedback::WebData* web_data = feedback_data.mutable_web_data();

  // Set GAIA id to 0. We're not using gaia id's for recording
  // use feedback - we're using the e-mail field, allows users to
  // submit feedback from incognito mode and specify any mail id
  // they wish
  common_data->set_gaia_id(0);

  // Add the page title.
  AddFeedbackData(&feedback_data, std::string(kPageTitleTag),
                  page_title_text);

#if defined(OS_CHROMEOS)
  // Add the user e-mail to the feedback object
  common_data->set_user_email(user_email_text);
#endif

  // Add the description to the feedback object
  common_data->set_description(description);

  // Add the language
  std::string chrome_locale = g_browser_process->GetApplicationLocale();
  common_data->set_source_description_language(chrome_locale);

  // Set the url
  web_data->set_url(page_url_text);

  // Add the Chrome version
  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    std::string chrome_version = version_info.Name() + " - " +
        version_info.Version() +
        " (" + version_info.LastChange() + ")";
    AddFeedbackData(&feedback_data, std::string(kChromeVersionTag),
                    chrome_version);
  }

  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2").
  std::string os_version = "";
  SetOSVersion(&os_version);
  AddFeedbackData(&feedback_data, std::string(kOsVersionTag), os_version);

#if defined(OS_CHROMEOS)
  if (sys_info) {
    for (chromeos::LogDictionaryType::const_iterator i = sys_info->begin();
         i != sys_info->end(); ++i)
      if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCompressSystemFeedback) || ValidFeedbackSize(i->second)) {
        AddFeedbackData(&feedback_data, i->first, i->second);
      }
  }
#endif

  // Include the page image if we have one.
  if (png_data) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);
    // Set the dimensions of the screenshot
    userfeedback::Dimensions dimensions;
    dimensions.set_width(static_cast<float>(png_width));
    dimensions.set_height(static_cast<float>(png_height));
    *(screenshot.mutable_dimensions()) = dimensions;
    screenshot.set_binary_content(std::string(png_data, png_data_length));

    // Set the screenshot object in feedback
    *(feedback_data.mutable_screenshot()) = screenshot;
  }

#if defined(OS_CHROMEOS)
  // Include the page image if we have one.
  if (zipped_logs_data && CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kCompressSystemFeedback)) {
    userfeedback::ProductSpecificBinaryData attachment;
    attachment.set_mime_type(kBZip2MimeType);
    attachment.set_name(kLogsAttachmentName);
    attachment.set_data(std::string(zipped_logs_data, zipped_logs_length));

    *(feedback_data.add_product_specific_binary_data()) = attachment;
  }
#endif

  // Set our Chrome specific data
  userfeedback::ChromeData chrome_data;
#if defined(OS_CHROMEOS)
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_OS);
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      (userfeedback::ChromeOsData_ChromeOsCategory) problem_type);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
#else
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER);
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      (userfeedback::ChromeBrowserData_ChromeBrowserCategory) problem_type);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
#endif

  *(feedback_data.mutable_chrome_data()) = chrome_data;

  // We have the body of our POST, so send it off to the server.
  URLFetcher* fetcher = new URLFetcher(post_url, URLFetcher::POST,
#if defined(OS_CHROMEOS)
                                       new BugReportUtil::PostCleanup(profile));
#else
                                       new BugReportUtil::PostCleanup());
#endif
  fetcher->set_request_context(profile->GetRequestContext());

  std::string post_body;
  feedback_data.SerializeToString(&post_body);
  fetcher->set_upload_data(std::string(kProtBufMimeType), post_body);
  fetcher->Start();
}

// static
void BugReportUtil::ReportPhishing(TabContents* currentTab,
                                   const std::string& phishing_url) {
  currentTab->controller().LoadURL(
      safe_browsing_util::GeneratePhishingReportUrl(
          kReportPhishingUrl, phishing_url),
      GURL(),
      PageTransition::LINK);
}
