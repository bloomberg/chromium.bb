// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_util.h"

#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request_status.h"
#include "unicode/locid.h"

#include <string>

namespace {

const int kBugReportVersion = 1;

const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";

// URL to post bug reports to.
const char* const kBugReportPostUrl =
    "https://www.google.com/tools/feedback/chrome/__submit";

const char* const kProtBufMimeType = "application/x-protobuf";
const char* const kPngMimeType = "image/png";

// Tags we use in product specific data
const char* const kPageTitleTag = "PAGE TITLE";
const char* const kProblemTypeIdTag = "PROBLEM TYPE ID";
const char* const kProblemTypeTag = "PROBLEM TYPE";
const char* const kChromeVersionTag = "CHROME VERSION";
const char* const kOsVersionTag = "OS VERSION";


}  // namespace

// Simple URLFetcher::Delegate to clean up URLFetcher on completion.
class BugReportUtil::PostCleanup : public URLFetcher::Delegate {
 public:
  PostCleanup();

  // Overridden from URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  DISALLOW_COPY_AND_ASSIGN(PostCleanup);
};

BugReportUtil::PostCleanup::PostCleanup() {
}

void BugReportUtil::PostCleanup::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // if not 204, something went wrong
  if (response_code != 204)
    LOG(WARNING) << "Submission to feedback server failed. Response code: " <<
                 response_code << std::endl;
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
void BugReportUtil::AddFeedbackData(
    userfeedback::ExternalExtensionSubmit* feedback_data,
    const std::string& key, const std::string& value) {
  // Create log_value object and add it to the web_data object
  userfeedback::ProductSpecificData log_value;
  log_value.set_key(key);
  log_value.set_value(value);
  userfeedback::WebData* web_data = feedback_data->mutable_web_data();
  *(web_data->add_product_specific_data()) = log_value;
}

// static
void BugReportUtil::SendReport(Profile* profile,
    const std::string& page_title_text,
    int problem_type,
    const std::string& page_url_text,
    const std::string& user_email_text,
    const std::string& description,
    const char* png_data,
    int png_data_length,
    int png_width,
#if defined(OS_CHROMEOS)
    int png_height,
    const std::string& problem_type_text,
    const chromeos::LogDictionaryType* const sys_info) {
#else
    int png_height) {
#endif
  GURL post_url(kBugReportPostUrl);

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

  AddFeedbackData(&feedback_data, std::string(kProblemTypeIdTag),
                  StringPrintf("%d\r\n", problem_type));

#if defined(OS_CHROMEOS)
  AddFeedbackData(&feedback_data, std::string(kProblemTypeTag),
                  problem_type_text);
#endif

  // Add the user e-mail to the feedback object
  common_data->set_user_email(user_email_text);

  // Add the description to the feedback object
  common_data->set_description(description);

  // Add the language
  std::string chrome_locale = g_browser_process->GetApplicationLocale();
  common_data->set_source_descripton_language(chrome_locale);

  // Set the url
  web_data->set_url(page_url_text);

  // Add the Chrome version
  std::string chrome_version;
  scoped_ptr<FileVersionInfo> version_info(chrome::GetChromeVersionInfo());
  if (version_info.get()) {
    chrome_version = WideToUTF8(version_info->product_name()) + " - " +
        WideToUTF8(version_info->file_version()) +
        " (" + WideToUTF8(version_info->last_change()) + ")";
  }

  if (!chrome_version.empty())
    AddFeedbackData(&feedback_data, std::string(kChromeVersionTag),
                    chrome_version);

  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2").
  std::string os_version = "";
  SetOSVersion(&os_version);
  AddFeedbackData(&feedback_data, std::string(kOsVersionTag), os_version);

#if defined(OS_CHROMEOS)
  if (sys_info) {
    for (chromeos::LogDictionaryType::const_iterator i = sys_info->begin();
         i != sys_info->end(); ++i)
      AddFeedbackData(&feedback_data, i->first, i->second);
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

  // We have the body of our POST, so send it off to the server.
  URLFetcher* fetcher = new URLFetcher(post_url, URLFetcher::POST,
                                       new BugReportUtil::PostCleanup);
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
