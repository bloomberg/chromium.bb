// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeOmniboxNavigationObserverTest : public testing::Test {
 protected:
  ChromeOmniboxNavigationObserverTest() {}
  ~ChromeOmniboxNavigationObserverTest() override {}

  content::NavigationController* navigation_controller() {
    return &(web_contents_->GetController());
  }

  TestingProfile* profile() { return &profile_; }
  TemplateURLService* model() {
    return TemplateURLServiceFactory::GetForProfile(&profile_);
  }

  // Functions that return the name of certain search keywords that are part
  // of the TemplateURLService attached to this profile.
  static base::string16 auto_generated_search_keyword() {
    return base::ASCIIToUTF16("auto_generated_search_keyword");
  }
  static base::string16 non_auto_generated_search_keyword() {
    return base::ASCIIToUTF16("non_auto_generated_search_keyword");
  }
  static base::string16 default_search_keyword() {
    return base::ASCIIToUTF16("default_search_keyword");
  }
  static base::string16 prepopulated_search_keyword() {
    return base::ASCIIToUTF16("prepopulated_search_keyword");
  }
  static base::string16 policy_search_keyword() {
    return base::ASCIIToUTF16("policy_search_keyword");
  }

 private:
  // testing::Test:
  void SetUp() override;

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxNavigationObserverTest);
};

void ChromeOmniboxNavigationObserverTest::SetUp() {
  web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
      profile(), content::SiteInstance::Create(profile())));

  InfoBarService::CreateForWebContents(web_contents_.get());

  // Set up a series of search engines for later testing.
  TemplateURLServiceFactoryTestUtil factory_util(profile());
  factory_util.VerifyLoad();

  TemplateURLData auto_gen_turl;
  auto_gen_turl.SetKeyword(auto_generated_search_keyword());
  auto_gen_turl.safe_for_autoreplace = true;
  factory_util.model()->Add(base::MakeUnique<TemplateURL>(auto_gen_turl));

  TemplateURLData non_auto_gen_turl;
  non_auto_gen_turl.SetKeyword(non_auto_generated_search_keyword());
  factory_util.model()->Add(base::MakeUnique<TemplateURL>(non_auto_gen_turl));

  TemplateURLData default_turl;
  default_turl.SetKeyword(default_search_keyword());
  factory_util.model()->SetUserSelectedDefaultSearchProvider(
      factory_util.model()->Add(base::MakeUnique<TemplateURL>(default_turl)));

  TemplateURLData prepopulated_turl;
  prepopulated_turl.SetKeyword(prepopulated_search_keyword());
  prepopulated_turl.prepopulate_id = 1;
  factory_util.model()->Add(base::MakeUnique<TemplateURL>(prepopulated_turl));

  TemplateURLData policy_turl;
  policy_turl.SetKeyword(policy_search_keyword());
  policy_turl.created_by_policy = true;
  factory_util.model()->Add(base::MakeUnique<TemplateURL>(policy_turl));
}

TEST_F(ChromeOmniboxNavigationObserverTest, LoadStateAfterPendingNavigation) {
  std::unique_ptr<ChromeOmniboxNavigationObserver> observer =
      base::MakeUnique<ChromeOmniboxNavigationObserver>(
          profile(), base::ASCIIToUTF16("test text"), AutocompleteMatch(),
          AutocompleteMatch());
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_NOT_SEEN,
            observer->load_state());

  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          GURL(), content::Referrer(), ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
          false, std::string(), profile()));

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::Source<content::NavigationController>(navigation_controller()),
      content::Details<content::NavigationEntry>(entry.get()));

  // A pending navigation notification should synchronously update the load
  // state to pending.
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_PENDING,
            observer->load_state());
}

TEST_F(ChromeOmniboxNavigationObserverTest, DeleteBrokenCustomSearchEngines) {
  struct TestData {
    base::string16 keyword;
    int status_code;
    bool expect_exists;
  };
  std::vector<TestData> cases = {
      {auto_generated_search_keyword(), 200, true},
      {auto_generated_search_keyword(), 404, false},
      {non_auto_generated_search_keyword(), 404, true},
      {default_search_keyword(), 404, true},
      {prepopulated_search_keyword(), 404, true},
      {policy_search_keyword(), 404, true}};

  base::string16 query = base::ASCIIToUTF16(" text");
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE("case #" + base::IntToString(i));
    // The keyword should always exist at the beginning.
    EXPECT_TRUE(model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);

    AutocompleteMatch match;
    match.keyword = cases[i].keyword;
    // |observer| gets deleted by observer->NavigationEntryCommitted().
    ChromeOmniboxNavigationObserver* observer =
        new ChromeOmniboxNavigationObserver(profile(), cases[i].keyword + query,
                                            match, AutocompleteMatch());
    auto navigation_entry(content::NavigationController::CreateNavigationEntry(
        GURL(), content::Referrer(), ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
        false, std::string(), profile()));
    content::LoadCommittedDetails details;
    details.http_status_code = cases[i].status_code;
    details.entry = navigation_entry.get();
    observer->NavigationEntryCommitted(details);
    EXPECT_EQ(cases[i].expect_exists,
              model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);
  }

  // Also run a URL navigation that results in a 404 through the system to make
  // sure nothing crashes for regular URL navigations.
  // |observer| gets deleted by observer->NavigationEntryCommitted().
  ChromeOmniboxNavigationObserver* observer =
      new ChromeOmniboxNavigationObserver(
          profile(), base::ASCIIToUTF16("url navigation"), AutocompleteMatch(),
          AutocompleteMatch());
  auto navigation_entry(content::NavigationController::CreateNavigationEntry(
      GURL(), content::Referrer(), ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, false,
      std::string(), profile()));
  content::LoadCommittedDetails details;
  details.http_status_code = 404;
  details.entry = navigation_entry.get();
  observer->NavigationEntryCommitted(details);
}
