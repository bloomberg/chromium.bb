// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_util.h"

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"
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

const int64 kInitialRetryDelay = 900000; // 15 minutes
const int64 kRetryDelayIncreaseFactor = 2;
const int64 kRetryDelayLimit = 14400000; // 4 hours


}  // namespace


// Simple URLFetcher::Delegate to clean up URLFetcher on completion.
class BugReportUtil::PostCleanup : public URLFetcher::Delegate {
 public:
  PostCleanup(Profile* profile, std::string* post_body,
              int64 previous_delay) : profile_(profile),
                                      post_body_(post_body),
                                      previous_delay_(previous_delay) { }
  // Overridden from URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 protected:
  virtual ~PostCleanup() {}

 private:
  Profile* profile_;
  std::string* post_body_;
  int64 previous_delay_;

  DISALLOW_COPY_AND_ASSIGN(PostCleanup);
};

// Don't use the data parameter, instead use the pointer we pass into every
// post cleanup object - that pointer will be deleted and deleted only on a
// successful post to the feedback server.
void BugReportUtil::PostCleanup::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {

  std::stringstream error_stream;
  if (response_code == kHttpPostSuccessNoContent) {
    // We've sent our report, delete the report data
    delete post_body_;

    error_stream << "Success";
  } else {
    // Uh oh, feedback failed, send it off to retry
    if (previous_delay_) {
      if (previous_delay_ < kRetryDelayLimit)
        previous_delay_ *= kRetryDelayIncreaseFactor;
    } else {
      previous_delay_ = kInitialRetryDelay;
    }
    BugReportUtil::DispatchFeedback(profile_, post_body_, previous_delay_);

    // Process the error for debug output
    if (response_code == kHttpPostFailNoConnection) {
      error_stream << "No connection to server.";
    } else if ((response_code > kHttpPostFailClientError) &&
        (response_code < kHttpPostFailServerError)) {
      error_stream << "Client error: HTTP response code " << response_code;
    } else if (response_code > kHttpPostFailServerError) {
      error_stream << "Server error: HTTP response code " << response_code;
    } else {
      error_stream << "Unknown error: HTTP response code " << response_code;
    }
  }

  LOG(WARNING) << "FEEDBACK: Submission to feedback server (" << url
               << ") status: " << error_stream.str();

  // Delete the URLFetcher.
  delete source;
  // And then delete ourselves.
  delete this;
}

// static
void BugReportUtil::SetOSVersion(std::string* os_version) {
#if defined(OS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  *os_version = StringPrintf("%d.%d.%d", version_number.major,
                             version_number.minor, version_number.build);
  int service_pack = os_info->service_pack().major;
  if (service_pack > 0)
    os_version->append(StringPrintf("Service Pack %d", service_pack));
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
void BugReportUtil::DispatchFeedback(Profile* profile,
                                     std::string* post_body,
                                     int64 delay) {
  DCHECK(post_body);

  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      &BugReportUtil::SendFeedback, profile, post_body, delay), delay);
}

// static
void BugReportUtil::SendFeedback(Profile* profile,
                                 std::string* post_body,
                                 int64 previous_delay) {
  DCHECK(post_body);

  GURL post_url;
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kFeedbackServer))
    post_url = GURL(CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kFeedbackServer));
  else
    post_url = GURL(kBugReportPostUrl);

  URLFetcher* fetcher = new URLFetcher(post_url, URLFetcher::POST,
                            new BugReportUtil::PostCleanup(profile,
                                                           post_body,
                                                           previous_delay));
  fetcher->set_request_context(profile->GetRequestContext());

  fetcher->set_upload_data(std::string(kProtBufMimeType), *post_body);
  fetcher->Start();
}


// static
void BugReportUtil::AddFeedbackData(
    userfeedback::ExternalExtensionSubmit* feedback_data,
    const std::string& key, const std::string& value) {
  // Don't bother with empty keys or values
  if (key=="" || value == "") return;
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
  if (sys_info) {
    // Add the product specific data
    for (chromeos::LogDictionaryType::const_iterator i = sys_info->begin();
         i != sys_info->end(); ++i)
      if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCompressSystemFeedback) || ValidFeedbackSize(i->second)) {
        AddFeedbackData(&feedback_data, i->first, i->second);
      }

    // If we have zipped logs, add them here
    if (zipped_logs_data && CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kCompressSystemFeedback)) {
      userfeedback::ProductSpecificBinaryData attachment;
      attachment.set_mime_type(kBZip2MimeType);
      attachment.set_name(kLogsAttachmentName);
      attachment.set_data(std::string(zipped_logs_data, zipped_logs_length));
      *(feedback_data.add_product_specific_binary_data()) = attachment;
    }
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

  // Serialize our report to a string pointer we can pass around
  std::string* post_body = new std::string;
  feedback_data.SerializeToString(post_body);

  // We have the body of our POST, so send it off to the server with 0 delay
  DispatchFeedback(profile, post_body, 0);
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
