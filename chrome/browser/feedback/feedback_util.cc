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
#include "chrome/browser/feedback/feedback_uploader.h"
#include "chrome/browser/feedback/feedback_uploader_factory.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/metrics_log_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
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

namespace {

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


#if defined(OS_CHROMEOS)
const int kChromeOSProductId = 208;
#else
const int kChromeBrowserProductId = 237;
#endif

}  // namespace

namespace chrome {

const char kAppLauncherCategoryTag[] = "AppLauncher";

void ShowFeedbackPage(Browser* browser,
                      const std::string& description_template,
                      const std::string& category_tag) {
  GURL page_url;
  if (browser) {
    page_url = GetTargetTabUrl(browser->session_id().id(),
                               browser->tab_strip_model()->active_index());
  }

  Profile* profile = NULL;
  if (browser) {
    profile = browser->profile();
  } else {
    profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  }
  if (!profile) {
    LOG(ERROR) << "Cannot invoke feedback: No profile found!";
    return;
  }

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);

  api->RequestFeedback(description_template,
                       category_tag,
                       page_url);
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
  data->PrepareReport(&feedback_data);

  std::string chrome_locale = g_browser_process->GetApplicationLocale();

  feedback_data.mutable_common_data()->set_source_description_language(
      chrome_locale);

  feedback_data.mutable_web_data()->mutable_navigator()->set_user_agent(
      GetUserAgent());

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
  std::string post_body;
  feedback_data.SerializeToString(&post_body);

  feedback::FeedbackUploader *uploader =
      feedback::FeedbackUploaderFactory::GetForBrowserContext(data->profile());
  uploader->QueueReport(post_body);
}

bool ZipString(const base::FilePath& filename,
               const std::string& data, std::string* compressed_logs) {
  base::FilePath temp_path;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(), &temp_path))
    return false;
  if (base::WriteFile(temp_path.Append(filename), data.c_str(), data.size()) ==
      -1)
    return false;

  bool succeed = base::CreateTemporaryFile(&zip_file) &&
      zip::Zip(temp_path, zip_file, false) &&
      base::ReadFileToString(zip_file, compressed_logs);

  base::DeleteFile(temp_path, true);
  base::DeleteFile(zip_file, false);

  return succeed;
}  // feedback_util

}  // namespace feedback_util
