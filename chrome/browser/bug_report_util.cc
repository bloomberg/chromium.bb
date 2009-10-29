// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_util.h"

#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/locale_settings.h"

namespace {

static const int kBugReportVersion = 1;

static const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";
}

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
#else
  // Right now, we only get the OS Version for Windows.
  // TODO(mirandac): make this cross-platform.
  *os_version = "unknown";
#endif
}

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
void BugReportUtil::CreateMimeBoundary(std::string *out) {
  int r1 = rand();
  int r2 = rand();
  SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

// static
void BugReportUtil::SendReport(Profile* profile,
    std::string page_title_text,
    int problem_type,
    std::string page_url_text,
    std::string description,
    const char* png_data,
    int png_data_length) {
  GURL post_url(WideToUTF8(l10n_util::GetString(IDS_BUGREPORT_POST_URL)));
  std::string mime_boundary;
  CreateMimeBoundary(&mime_boundary);

  // Create a request body and add the mandatory parameters.
  std::string post_body;

  // Add the protocol version:
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"data_version\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", kBugReportVersion));

  // Add the page title.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append(page_title_text + "\r\n");

  // Add the problem type.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"problem\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", problem_type));

  // Add in the URL, if we have one.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"url\"\r\n\r\n");

  // Convert URL to UTF8.
  if (page_url_text.empty())
    post_body.append("n/a\r\n");
  else
    post_body.append(page_url_text + "\r\n");

  // Add Chrome version.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_version\"\r\n\r\n");

  std::string chrome_version;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    chrome_version = WideToUTF8(version_info->product_name()) + " - " +
        WideToUTF8(version_info->file_version()) +
        " (" + WideToUTF8(version_info->last_change()) + ")";
  }

  if (chrome_version.empty())
    post_body.append("n/a\r\n");
  else
    post_body.append(chrome_version + "\r\n");

  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2").
  std::string os_version = "";
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"os_version\"\r\n\r\n");
  SetOSVersion(&os_version);
  post_body.append(os_version + "\r\n");

  // Add locale.
#if defined(OS_MACOSX)
  std::string chrome_locale = g_browser_process->GetApplicationLocale();
#else
  icu::Locale locale = icu::Locale::getDefault();
  const char *lang = locale.getLanguage();
  std::string chrome_locale = (lang)? lang:"en";
#endif

  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_locale\"\r\n\r\n");
  post_body.append(chrome_locale + "\r\n");

  // Add a description if we have one.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"description\"\r\n\r\n");

  if (description.empty())
    post_body.append("n/a\r\n");
  else
    post_body.append(description + "\r\n");

  // Include the page image if we have one.
  if (png_data != NULL && png_data_length > 0) {
    post_body.append("--" + mime_boundary + "\r\n");
    post_body.append("Content-Disposition: form-data; name=\"screenshot\"; "
                      "filename=\"screenshot.png\"\r\n");
    post_body.append("Content-Type: application/octet-stream\r\n");
    post_body.append(StringPrintf("Content-Length: %lu\r\n\r\n",
                     png_data_length));
    post_body.append(png_data, png_data_length);
    post_body.append("\r\n");
  }

  // TODO(awalker): include the page source if we can get it.
  // if (include_page_source_checkbox_->checked()) {
  // }

  // Terminate the body.
  post_body.append("--" + mime_boundary + "--\r\n");

  // We have the body of our POST, so send it off to the server.
  URLFetcher* fetcher = new URLFetcher(post_url, URLFetcher::POST,
                                       new BugReportUtil::PostCleanup);
  fetcher->set_request_context(profile->GetRequestContext());
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  fetcher->set_upload_data(mime_type, post_body);
  fetcher->Start();
}

// static
std::string BugReportUtil::GetMimeType() {
  std::string mime_type("multipart/form-data; boundary=");
  std::string mime_boundary;
  CreateMimeBoundary(&mime_boundary);
  mime_type += mime_boundary;
  return mime_type;
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

