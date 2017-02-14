// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

void SetUrlRequestMock(const base::FilePath& path) {
  net::URLRequestMockHTTPJob::AddUrlHandlers(path,
                                             BrowserThread::GetBlockingPool());
}

}  // namespace

class NewTabPageInterceptorTest : public InProcessBrowserTest {
 public:
  NewTabPageInterceptorTest() {}

  void SetUpOnMainThread() override {
    path_ = ui_test_utils::GetTestFilePath(base::FilePath(), base::FilePath());
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&SetUrlRequestMock, path_));
  }

  const GURL& new_tab_url() const { return new_tab_url_; }
  void set_new_tab_url(const GURL& url) { new_tab_url_ = url; }

  void ChangeDefaultSearchProvider(const char* new_tab_path) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    UIThreadSearchTermsData::SetGoogleBaseURL("https://mock.http/");
    std::string base_url("{google:baseURL}");
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("Google"));
    data.SetKeyword(base::UTF8ToUTF16(base_url));
    data.SetURL(base_url + "url?bar={searchTerms}");
    data.new_tab_url = base_url + new_tab_path;
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

 private:
  GURL new_tab_url_;
  base::FilePath path_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageInterceptorTest, NoInterception) {
  set_new_tab_url(
      net::URLRequestMockHTTPJob::GetMockHttpsUrl("instant_extended.html"));
  ChangeDefaultSearchProvider("instant_extended.html");

  ui_test_utils::NavigateToURL(browser(), new_tab_url());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();
  // A correct, 200-OK file works correctly.
  EXPECT_EQ(new_tab_url(), controller->GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(NewTabPageInterceptorTest, 404Interception) {
  set_new_tab_url(net::URLRequestMockHTTPJob::GetMockHttpsUrl("page404.html"));
  ChangeDefaultSearchProvider("page404.html");

  ui_test_utils::NavigateToURL(browser(), new_tab_url());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();
  // 404 makes a redirect to the local NTP.
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            controller->GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(NewTabPageInterceptorTest, 204Interception) {
  set_new_tab_url(net::URLRequestMockHTTPJob::GetMockHttpsUrl("page204.html"));
  ChangeDefaultSearchProvider("page204.html");

  ui_test_utils::NavigateToURL(browser(), new_tab_url());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();
  // 204 makes a redirect to the local NTP.
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            controller->GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(NewTabPageInterceptorTest, FailedRequestInterception) {
  set_new_tab_url(
      net::URLRequestMockHTTPJob::GetMockHttpsUrl("notarealfile.html"));
  ChangeDefaultSearchProvider("notarealfile.html");

  ui_test_utils::NavigateToURL(browser(), new_tab_url());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController* controller = &contents->GetController();
  // Failed navigation makes a redirect to the local NTP.
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            controller->GetLastCommittedEntry()->GetURL());
}
