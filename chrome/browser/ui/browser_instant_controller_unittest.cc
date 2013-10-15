// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace chrome {

namespace {

class BrowserInstantControllerTest : public InstantUnitTestBase {
 protected:
  friend class FakeWebContentsObserver;
};

const struct TabReloadTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  bool should_reload;
  bool end_in_instant_process;
} kTabReloadTestCases[] = {
    {"Local Embedded NTP", chrome::kChromeSearchLocalNtpUrl,
     true, true, true},
    {"Remote Embedded NTP", "https://www.google.com/instant?strk",
     true, true, false},
    {"Remote Embedded SERP", "https://www.google.com/url?strk&bar=search+terms",
     true, true, false},
    {"Other NTP", "https://bar.com/instant?strk",
     false, false, false}
};

class FakeWebContentsObserver : public content::WebContentsObserver {
 public:
  FakeWebContentsObserver(BrowserInstantControllerTest* base_test,
                          content::WebContents* contents)
      : WebContentsObserver(contents),
        contents_(contents),
        base_test_(base_test),
        url_(contents->GetURL()),
        num_reloads_(0) {}

  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE {
    // The tab reload event doesn't work with BrowserWithTestWindowTest.
    // So we capture the NavigateToPendingEntry, and use the
    // BrowserWithTestWindowTest::NavigateAndCommit to simulate the complete
    // reload. Note that this will again trigger NavigateToPendingEntry, so we
    // remove this as observer.
    content::NavigationController* controller =
        &web_contents()->GetController();
    Observe(NULL);

    if (url_ == url)
      num_reloads_++;

    base_test_->NavigateAndCommit(controller, url);
  }

  int num_reloads() const {
    return num_reloads_;
  }

  content::WebContents* contents() {
    return contents_;
  }

 protected:
  friend class BrowserInstantControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest,
                           DefaultSearchProviderChanged);
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest,
                           GoogleBaseURLUpdated);

 private:
  content::WebContents* contents_;
  BrowserInstantControllerTest* base_test_;
  const GURL& url_;
  int num_reloads_;
};

TEST_F(BrowserInstantControllerTest, DefaultSearchProviderChanged) {
  chrome::EnableQueryExtractionForTesting();
  size_t num_tests = arraysize(kTabReloadTestCases);
  ScopedVector<FakeWebContentsObserver> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test = kTabReloadTestCases[i];
    AddTab(browser(), GURL(test.start_url));
    content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(test.start_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(new FakeWebContentsObserver(this, contents));
  }

  SetDefaultSearchProvider("https://bar.com/");

  for (size_t i = 0; i < num_tests; ++i) {
    FakeWebContentsObserver* observer = observers[i];
    const TabReloadTestCase& test = kTabReloadTestCases[i];
    content::WebContents* contents = observer->contents();

    // Validate final instant state.
    EXPECT_EQ(test.end_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Ensure only the expected tabs(contents) reloaded.
    EXPECT_EQ(test.should_reload ? 1 : 0, observer->num_reloads())
      << test.description;
  }
}

TEST_F(BrowserInstantControllerTest, GoogleBaseURLUpdated) {
  chrome::EnableQueryExtractionForTesting();
  const size_t num_tests = arraysize(kTabReloadTestCases);
  ScopedVector<FakeWebContentsObserver> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test = kTabReloadTestCases[i];
    AddTab(browser(), GURL(test.start_url));
    content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(test.start_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(new FakeWebContentsObserver(this, contents));
  }

  NotifyGoogleBaseURLUpdate("https://www.google.es/");

  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test = kTabReloadTestCases[i];
    FakeWebContentsObserver* observer = observers[i];
    content::WebContents* contents = observer->contents();

    // Validate final instant state.
    EXPECT_EQ(test.end_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Ensure only the expected tabs(contents) reloaded.
    EXPECT_EQ(test.should_reload ? 1 : 0, observer->num_reloads())
      << test.description;
  }
}

TEST_F(BrowserInstantControllerTest, BrowserWindowLifecycle) {
  scoped_ptr<BrowserWindow> window(CreateBrowserWindow());
  Browser::CreateParams params(profile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
  params.window = window.get();
  scoped_ptr<Browser> browser(new Browser(params));
  InstantServiceObserver* bic;
  bic = browser->instant_controller();
  EXPECT_TRUE(IsInstantServiceObserver(bic))
    << "New BrowserInstantController should register as InstantServiceObserver";

  browser.reset(NULL);
  window.reset(NULL);
  EXPECT_FALSE(IsInstantServiceObserver(bic))
    << "New BrowserInstantController should register as InstantServiceObserver";
}

}  // namespace

}  // namespace chrome
