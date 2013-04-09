// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_util.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/metrics_log_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "third_party/icu/public/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

namespace chrome {
const char kAppLauncherCategoryTag[] = "AppLauncher";
}  // namespace chrome

const int kFeedbackVersion = 1;

const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";

// URL to post bug reports to.
const char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

const char kProtBufMimeType[] = "application/x-protobuf";
const char kPngMimeType[] = "image/png";

// Tags we use in product specific data
const char kChromeVersionTag[] = "CHROME VERSION";
const char kOsVersionTag[] = "OS VERSION";

const int kHttpPostSuccessNoContent = 204;
const int kHttpPostFailNoConnection = -1;
const int kHttpPostFailClientError = 400;
const int kHttpPostFailServerError = 500;

const int64 kInitialRetryDelay = 900000;  // 15 minutes
const int64 kRetryDelayIncreaseFactor = 2;
const int64 kRetryDelayLimit = 14400000;  // 4 hours

#if defined(OS_CHROMEOS)
const size_t kFeedbackMaxLength = 4 * 1024;
const size_t kFeedbackMaxLineCount = 40;

const char kArbitraryMimeType[] = "application/octet-stream";
const char kLogsAttachmentName[] = "system_logs.zip";

const char kTimestampTag[] = "TIMESTAMP";

const int kChromeOSProductId = 208;
#endif

const int kChromeBrowserProductId = 237;

// Simple net::URLFetcherDelegate to clean up URLFetcher on completion.
class FeedbackUtil::PostCleanup : public net::URLFetcherDelegate {
 public:
  PostCleanup(Profile* profile,
              std::string* post_body,
              int64 previous_delay) : profile_(profile),
                                      post_body_(post_body),
                                      previous_delay_(previous_delay) { }
  // Overridden from net::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

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
void FeedbackUtil::PostCleanup::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::stringstream error_stream;
  int response_code = source->GetResponseCode();
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
    FeedbackUtil::DispatchFeedback(profile_, post_body_, previous_delay_);

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

  LOG(WARNING) << "FEEDBACK: Submission to feedback server (" <<
               source->GetURL() << ") status: " << error_stream.str();

  // Delete the URLFetcher.
  delete source;
  // And then delete ourselves.
  delete this;
}

// static
void FeedbackUtil::SetOSVersion(std::string* os_version) {
#if defined(OS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  *os_version = base::StringPrintf("%d.%d.%d",
                                   version_number.major,
                                   version_number.minor,
                                   version_number.build);
  int service_pack = os_info->service_pack().major;
  if (service_pack > 0)
    os_version->append(base::StringPrintf("Service Pack %d", service_pack));
#elif defined(OS_MACOSX)
  *os_version = base::SysInfo::OperatingSystemVersion();
#else
  *os_version = "unknown";
#endif
}

// static
void FeedbackUtil::DispatchFeedback(Profile* profile,
                                    std::string* post_body,
                                    int64 delay) {
  DCHECK(post_body);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FeedbackUtil::SendFeedback, profile, post_body, delay),
      base::TimeDelta::FromMilliseconds(delay));
}

// static
void FeedbackUtil::SendFeedback(Profile* profile,
                                std::string* post_body,
                                int64 previous_delay) {
  DCHECK(post_body);

  GURL post_url;
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kFeedbackServer))
    post_url = GURL(CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kFeedbackServer));
  else
    post_url = GURL(kFeedbackPostUrl);

  net::URLFetcher* fetcher = net::URLFetcher::Create(
      post_url, net::URLFetcher::POST,
      new FeedbackUtil::PostCleanup(profile, post_body, previous_delay));
  fetcher->SetRequestContext(profile->GetRequestContext());
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->SetUploadData(std::string(kProtBufMimeType), *post_body);
  fetcher->Start();
}


// static
void FeedbackUtil::AddFeedbackData(
    userfeedback::ExtensionSubmit* feedback_data,
    const std::string& key, const std::string& value) {
  // Don't bother with empty keys or values
  if (key == "" || value == "") return;
  // Create log_value object and add it to the web_data object
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

#if defined(OS_CHROMEOS)
bool FeedbackUtil::ValidFeedbackSize(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = std::count(content.begin(), content.end(), '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}
#endif

// static
void FeedbackUtil::SendReport(scoped_refptr<FeedbackData> data) {
  if (!data) {
    LOG(ERROR) << "FeedbackUtil::SendReport called with NULL data!";
    NOTREACHED();
    return;
  }

  // Create google feedback protocol buffer objects
  userfeedback::ExtensionSubmit feedback_data;
  // type id set to 0, unused field but needs to be initialized to 0
  feedback_data.set_type_id(0);

  userfeedback::CommonData* common_data = feedback_data.mutable_common_data();
  userfeedback::WebData* web_data = feedback_data.mutable_web_data();

  // Set our user agent.
  userfeedback::Navigator* navigator = web_data->mutable_navigator();
  navigator->set_user_agent(content::GetUserAgent(GURL()));

  // Set GAIA id to 0. We're not using gaia id's for recording
  // use feedback - we're using the e-mail field, allows users to
  // submit feedback from incognito mode and specify any mail id
  // they wish
  common_data->set_gaia_id(0);

  // Add the user e-mail to the feedback object
  common_data->set_user_email(data->user_email());

  // Add the description to the feedback object
  common_data->set_description(data->description());

  // Add the language
  std::string chrome_locale = g_browser_process->GetApplicationLocale();
  common_data->set_source_description_language(chrome_locale);

  // Set the url
  web_data->set_url(data->page_url());

  // Add the Chrome version
  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    std::string chrome_version = version_info.Name() + " - " +
        version_info.Version() +
        " (" + version_info.LastChange() + ")";
    AddFeedbackData(&feedback_data, std::string(kChromeVersionTag),
                    chrome_version);
  }

  // We don't need the OS version for ChromeOS since we get it in
  // CHROMEOS_RELEASE_VERSION from /etc/lsb-release
#if !defined(OS_CHROMEOS)
  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2").
  std::string os_version = "";
  SetOSVersion(&os_version);
  AddFeedbackData(&feedback_data, std::string(kOsVersionTag), os_version);
#endif

  // Include the page image if we have one.
  if (data->image().get() && data->image()->size()) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);
    // Set the dimensions of the screenshot
    userfeedback::Dimensions dimensions;
    gfx::Rect& screen_size = GetScreenshotSize();
    dimensions.set_width(static_cast<float>(screen_size.width()));
    dimensions.set_height(static_cast<float>(screen_size.height()));
    *(screenshot.mutable_dimensions()) = dimensions;

    int image_data_size = data->image()->size();
    char* image_data = reinterpret_cast<char*>(&(data->image()->front()));
    screenshot.set_binary_content(std::string(image_data, image_data_size));

    // Set the screenshot object in feedback
    *(feedback_data.mutable_screenshot()) = screenshot;
  }

#if defined(OS_CHROMEOS)
  if (data->sys_info()) {
    // Add the product specific data
    for (chromeos::SystemLogsResponse::const_iterator i =
        data->sys_info()->begin(); i != data->sys_info()->end(); ++i) {
      if (ValidFeedbackSize(i->second)) {
        AddFeedbackData(&feedback_data, i->first, i->second);
      }
    }

    if (data->compressed_logs() && data->compressed_logs()->size()) {
      userfeedback::ProductSpecificBinaryData attachment;
      attachment.set_mime_type(kArbitraryMimeType);
      attachment.set_name(kLogsAttachmentName);
      attachment.set_data(*(data->compressed_logs()));
      *(feedback_data.add_product_specific_binary_data()) = attachment;
    }
  }

  if (data->timestamp() != "")
    AddFeedbackData(&feedback_data, std::string(kTimestampTag),
                    data->timestamp());

  if (data->attached_filename() != "" &&
      data->attached_filedata() &&
      data->attached_filedata()->size()) {
    userfeedback::ProductSpecificBinaryData attached_file;
    attached_file.set_mime_type(kArbitraryMimeType);
    attached_file.set_name(data->attached_filename());
    attached_file.set_data(*data->attached_filedata());
    *(feedback_data.add_product_specific_binary_data()) = attached_file;
  }
#endif

  // Set our category tag if we have one
  if (data->category_tag().size())
    feedback_data.set_bucket(data->category_tag());

  // Set our Chrome specific data
  userfeedback::ChromeData chrome_data;
  chrome_data.set_chrome_platform(
#if defined(OS_CHROMEOS)
      userfeedback::ChromeData_ChromePlatform_CHROME_OS);
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      userfeedback::ChromeOsData_ChromeOsCategory_OTHER);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
  feedback_data.set_product_id(kChromeOSProductId);
#else
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER);
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_OTHER);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
  feedback_data.set_product_id(kChromeBrowserProductId);
#endif

  *(feedback_data.mutable_chrome_data()) = chrome_data;

  // Serialize our report to a string pointer we can pass around
  std::string* post_body = new std::string;
  feedback_data.SerializeToString(post_body);

  // We have the body of our POST, so send it off to the server with 0 delay
  DispatchFeedback(data->profile(), post_body, 0);
}

#if defined(FULL_SAFE_BROWSING)
// static
void FeedbackUtil::ReportPhishing(WebContents* current_tab,
                                  const std::string& phishing_url) {
  current_tab->GetController().LoadURL(
      safe_browsing_util::GeneratePhishingReportUrl(
          kReportPhishingUrl, phishing_url,
          false /* not client-side detection */),
      content::Referrer(),
      content::PAGE_TRANSITION_LINK,
      std::string());
}
#endif

static std::vector<unsigned char>* screenshot_png = NULL;
static gfx::Rect* screenshot_size = NULL;

// static
std::vector<unsigned char>* FeedbackUtil::GetScreenshotPng() {
  if (screenshot_png == NULL)
    screenshot_png = new std::vector<unsigned char>;
  return screenshot_png;
}

// static
void FeedbackUtil::ClearScreenshotPng() {
  if (screenshot_png)
    screenshot_png->clear();
}

// static
gfx::Rect& FeedbackUtil::GetScreenshotSize() {
  if (screenshot_size == NULL)
    screenshot_size = new gfx::Rect();
  return *screenshot_size;
}

// static
void FeedbackUtil::SetScreenshotSize(const gfx::Rect& rect) {
  gfx::Rect& screen_size = GetScreenshotSize();
  screen_size = rect;
}
