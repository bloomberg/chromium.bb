// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile(),
                                        browser->host_desktop_type()));

  // Find the new browser.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (*it != browser)
      return *it;
  }

  return nullptr;
}

class MockTriggeredProfileResetter : public TriggeredProfileResetter {
 public:
  MockTriggeredProfileResetter() : TriggeredProfileResetter(nullptr) {}

  void Activate() override {}
  bool HasResetTrigger() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTriggeredProfileResetter);
};

scoped_ptr<KeyedService> BuildMockTriggeredProfileResetter(
    content::BrowserContext* context) {
  return make_scoped_ptr(new MockTriggeredProfileResetter);
}

}  // namespace

class StartupBrowserCreatorTriggeredResetTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorTriggeredResetTest() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&StartupBrowserCreatorTriggeredResetTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)))
            .Pass();
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    TriggeredProfileResetterFactory::GetInstance()->SetTestingFactory(
        context, &BuildMockTriggeredProfileResetter);
  }

  scoped_ptr<base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorTriggeredResetTest);
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTriggeredResetTest,
                       TestTriggeredReset) {
  // Use a couple same-site HTTP URLs.
  ASSERT_TRUE(test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(test_server()->GetURL("files/title1.html"));
  urls.push_back(test_server()->GetURL("files/title2.html"));

  Profile* profile = browser()->profile();
  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile, pref);

  // Keep the browser process running while browsers are closed.
  g_browser_process->AddRefModule();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_NOT_FIRST_RUN);
  ASSERT_TRUE(
      launch.Launch(profile, std::vector<GURL>(), false, host_desktop_type));

  // This should have created a new browser window.  |browser()| is still
  // around at this point, even though we've closed its window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  std::vector<GURL> expected_urls(urls);
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    expected_urls.insert(expected_urls.begin(), internals::GetWelcomePageURL());
  expected_urls.insert(expected_urls.begin(),
                       internals::GetTriggeredResetSettingsURL());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(expected_urls.size()), tab_strip->count());
  for (size_t i = 0; i < expected_urls.size(); i++)
    EXPECT_EQ(expected_urls[i], tab_strip->GetWebContentsAt(i)->GetURL());

  g_browser_process->ReleaseModule();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTriggeredResetTest,
                       TestTriggeredResetDoesNotShowWithFirstRunURLs) {
  // The presence of First Run tabs (in production code, these commonly come
  // from master_preferences) should suppress the reset UI. Check that this is
  // the case.
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that only the first-run tabs are shown.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());

  GURL expected_first_tab_url =
      signin::ShouldShowPromoAtStartup(browser()->profile(), true)
          ? signin::GetPromoURL(signin_metrics::SOURCE_START_PAGE, false)
          : GURL(chrome::kChromeUINewTabURL);
  EXPECT_EQ(expected_first_tab_url, tab_strip->GetWebContentsAt(0)->GetURL());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}
