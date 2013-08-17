// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_util.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/metrics_log_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

const base::FilePath::CharType kLogsFilename[] =
    FILE_PATH_LITERAL("system_logs.txt");

void DispatchFeedback(Profile* profile, std::string* post_body, int64 delay);

// Check the key/value pair to see if it is one of the screensize, keys. If so,
// populate the screensize structure with the key.
bool IsScreensizeInfo(const std::string key,
                      const std::string value,
                      gfx::Rect* screen_size) {
  if (key == FeedbackData::kScreensizeHeightKey) {
    int height = 0;
    base::StringToInt(value, &height);
    screen_size->SetRect(0, 0, screen_size->width(), height);
    return true;
  } else if (key == FeedbackData::kScreensizeWidthKey) {
    int width = 0;
    base::StringToInt(value, &width);
    screen_size->SetRect(0, 0, width, screen_size->height());
    return true;
  }
  return false;
}

GURL GetTargetTabUrl(int session_id, int index) {
  Browser* browser = chrome::FindBrowserWithID(session_id);
  // Sanity checks.
  if (!browser || index >= browser->tab_strip_model()->count())
    return GURL();

  if (index >= 0) {
    content::WebContents* target_tab =
        browser->tab_strip_model()->GetWebContentsAt(index);
    if (target_tab)
      return target_tab->GetURL();
  }

  return GURL();
}

gfx::Rect GetScreenSize(Browser* browser) {
#if defined(OS_CHROMEOS)
  // For ChromeOS, don't use the browser window but the root window
  // instead to grab the screenshot. We want everything on the screen, not
  // just the current browser.
  gfx::NativeWindow native_window = ash::Shell::GetPrimaryRootWindow();
  return gfx::Rect(native_window->bounds());
#else
  return gfx::Rect(browser->window()->GetBounds().size());
#endif
}

// URL to post bug reports to.
const char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

const char kProtBufMimeType[] = "application/x-protobuf";
const char kPngMimeType[] = "image/png";

const int kHttpPostSuccessNoContent = 204;
const int kHttpPostFailNoConnection = -1;
const int kHttpPostFailClientError = 400;
const int kHttpPostFailServerError = 500;

const int64 kInitialRetryDelay = 900000;  // 15 minutes
const int64 kRetryDelayIncreaseFactor = 2;
const int64 kRetryDelayLimit = 14400000;  // 4 hours

const size_t kFeedbackMaxLength = 4 * 1024;
const size_t kFeedbackMaxLineCount = 40;

const char kArbitraryMimeType[] = "application/octet-stream";
const char kLogsAttachmentName[] = "system_logs.zip";

const int kChromeOSProductId = 208;
const int kChromeBrowserProductId = 237;

// Simple net::URLFetcherDelegate to clean up URLFetcher on completion.
class PostCleanup : public net::URLFetcherDelegate {
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
void PostCleanup::OnURLFetchComplete(
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
    DispatchFeedback(profile_, post_body_, previous_delay_);

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

void SendFeedback(Profile* profile,
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
      new PostCleanup(profile, post_body, previous_delay));
  fetcher->SetRequestContext(profile->GetRequestContext());
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);

  net::HttpRequestHeaders headers;
  chrome_variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      fetcher->GetOriginalURL(), profile->IsOffTheRecord(), false, &headers);
  fetcher->SetExtraRequestHeaders(headers.ToString());

  fetcher->SetUploadData(std::string(kProtBufMimeType), *post_body);
  fetcher->Start();
}

void DispatchFeedback(Profile* profile, std::string* post_body, int64 delay) {
  DCHECK(post_body);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SendFeedback, profile, post_body, delay),
      base::TimeDelta::FromMilliseconds(delay));
}


void AddFeedbackData(userfeedback::ExtensionSubmit* feedback_data,
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

bool ValidFeedbackSize(const std::string& content) {
  if (content.length() > kFeedbackMaxLength)
    return false;
  const size_t line_count = std::count(content.begin(), content.end(), '\n');
  if (line_count > kFeedbackMaxLineCount)
    return false;
  return true;
}

}  // namespace

namespace chrome {

const char kAppLauncherCategoryTag[] = "AppLauncher";

void ShowFeedbackPage(Browser* browser,
                    const std::string& description_template,
                    const std::string& category_tag) {
  DCHECK(browser);

  // Get the current browser's screensize and send it with the feedback request
  // event - this browser may have changed or even been closed by the time that
  // feedback is sent.
  gfx::Rect screen_size = GetScreenSize(browser);
  GURL page_url = GetTargetTabUrl(
      browser->session_id().id(), browser->tab_strip_model()->active_index());

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->GetForProfile(
          browser->profile());

  api->RequestFeedback(description_template,
                       category_tag,
                       page_url,
                       screen_size);
}

}  // namespace chrome

namespace feedback_util {

void SendReport(scoped_refptr<FeedbackData> data) {
  if (!data.get()) {
    LOG(ERROR) << "SendReport called with NULL data!";
    NOTREACHED();
    return;
  }

  userfeedback::ExtensionSubmit feedback_data;
  // Unused field, needs to be 0 though.
  feedback_data.set_type_id(0);

  userfeedback::CommonData* common_data = feedback_data.mutable_common_data();
  // We're not using gaia ids, we're using the e-mail field instead.
  common_data->set_gaia_id(0);
  common_data->set_user_email(data->user_email());
  common_data->set_description(data->description());

  std::string chrome_locale = g_browser_process->GetApplicationLocale();
  common_data->set_source_description_language(chrome_locale);

  userfeedback::WebData* web_data = feedback_data.mutable_web_data();
  web_data->set_url(data->page_url());
  web_data->mutable_navigator()->set_user_agent(content::GetUserAgent(GURL()));

  gfx::Rect screen_size;
  if (data->sys_info()) {
    for (FeedbackData::SystemLogsMap::const_iterator i =
        data->sys_info()->begin(); i != data->sys_info()->end(); ++i) {
      if (!IsScreensizeInfo(i->first, i->second, &screen_size)) {
        if (ValidFeedbackSize(i->second))
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

  if (!data->attached_filename().empty() &&
      data->attached_filedata() &&
      !data->attached_filedata()->empty()) {
    userfeedback::ProductSpecificBinaryData attached_file;
    attached_file.set_mime_type(kArbitraryMimeType);
    // We need to use the UTF8Unsafe methods here to accomodate Windows, which
    // uses wide strings to store filepaths.
    std::string name = base::FilePath::FromUTF8Unsafe(
        data->attached_filename()).BaseName().AsUTF8Unsafe();
    attached_file.set_name(name);
    attached_file.set_data(*data->attached_filedata());
    *(feedback_data.add_product_specific_binary_data()) = attached_file;
  }

  // NOTE: Screenshot needs to be processed after system info since we'll get
  // the screenshot dimensions from system info.
  if (data->image() && data->image()->size()) {
    userfeedback::PostedScreenshot screenshot;
    screenshot.set_mime_type(kPngMimeType);

    // Set the dimensions of the screenshot
    userfeedback::Dimensions dimensions;
    dimensions.set_width(static_cast<float>(screen_size.width()));
    dimensions.set_height(static_cast<float>(screen_size.height()));

    *(screenshot.mutable_dimensions()) = dimensions;
    screenshot.set_binary_content(*data->image());

    *(feedback_data.mutable_screenshot()) = screenshot;
  }

  if (data->category_tag().size())
    feedback_data.set_bucket(data->category_tag());

  // Set whether we're reporting from ChromeOS or Chrome on another platform.
  userfeedback::ChromeData chrome_data;
#if defined(OS_CHROMEOS)
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_OS);
  userfeedback::ChromeOsData chrome_os_data;
  chrome_os_data.set_category(
      userfeedback::ChromeOsData_ChromeOsCategory_OTHER);
  *(chrome_data.mutable_chrome_os_data()) = chrome_os_data;
  feedback_data.set_product_id(kChromeOSProductId);
#else
  chrome_data.set_chrome_platform(
      userfeedback::ChromeData_ChromePlatform_CHROME_BROWSER);
  userfeedback::ChromeBrowserData chrome_browser_data;
  chrome_browser_data.set_category(
      userfeedback::ChromeBrowserData_ChromeBrowserCategory_OTHER);
  *(chrome_data.mutable_chrome_browser_data()) = chrome_browser_data;
  feedback_data.set_product_id(kChromeBrowserProductId);
#endif

  *(feedback_data.mutable_chrome_data()) = chrome_data;

  // This pointer will eventually get deleted by the PostCleanup class, after
  // we've either managed to successfully upload the report or died trying.
  std::string* post_body = new std::string;
  feedback_data.SerializeToString(post_body);

  DispatchFeedback(data->profile(), post_body, 0);
}

bool ZipString(const std::string& logs, std::string* compressed_logs) {
  base::FilePath temp_path;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &temp_path))
    return false;
  if (file_util::WriteFile(temp_path.Append(kLogsFilename),
                           logs.c_str(), logs.size()) == -1)
    return false;
  if (!file_util::CreateTemporaryFile(&zip_file))
    return false;

  if (!zip::Zip(temp_path, zip_file, false))
    return false;

  if (!file_util::ReadFileToString(zip_file, compressed_logs))
    return false;

  return true;
}

}  // namespace feedback_util
