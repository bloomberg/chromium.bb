// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/toolbar_model.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/google/core/browser/google_switches.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/toolbar/toolbar_model.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/text_elider.h"

// Test data ------------------------------------------------------------------

namespace {

struct TestItem {
  GURL url;
  // The expected text to display when both forms of URL replacement are
  // inactive.
  base::string16 expected_text_url_replacement_inactive;
  // The expected text to display when query extraction is active.
  base::string16 expected_text_query_extraction;
  // The expected text to display when both query extraction and URL removal are
  // active.
  base::string16 expected_text_both;
  bool would_perform_search_term_replacement;
  bool should_display_url;
} test_items[] = {
  {
    GURL("view-source:http://www.google.com"),
    base::ASCIIToUTF16("view-source:www.google.com"),
    base::ASCIIToUTF16("view-source:www.google.com"),
    base::string16(),
    false,
    true
  },
  {
    GURL("view-source:chrome://newtab/"),
    base::ASCIIToUTF16("view-source:chrome://newtab"),
    base::ASCIIToUTF16("view-source:chrome://newtab"),
    base::string16(),
    false,
    true
  },
  {
    GURL("chrome-extension://monkey/balls.html"),
    base::ASCIIToUTF16("chrome-extension://monkey/balls.html"),
    base::ASCIIToUTF16("chrome-extension://monkey/balls.html"),
    base::string16(),
    false,
    true
  },
  {
    GURL(url::kAboutBlankURL),
    base::ASCIIToUTF16(url::kAboutBlankURL),
    base::ASCIIToUTF16(url::kAboutBlankURL),
    base::string16(),
    false,
    true
  },
  {
    GURL("http://searchurl/?q=tractor+supply"),
    base::ASCIIToUTF16("searchurl/?q=tractor+supply"),
    base::ASCIIToUTF16("searchurl/?q=tractor+supply"),
    base::string16(),
    false,
    true
  },
  {
    GURL("http://google.com/search?q=tractor+supply&espv=1"),
    base::ASCIIToUTF16("google.com/search?q=tractor+supply&espv=1"),
    base::ASCIIToUTF16("google.com/search?q=tractor+supply&espv=1"),
    base::string16(),
    false,
    true
  },
  {
    GURL("https://google.ca/search?q=tractor+supply"),
    base::ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    base::ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    base::string16(),
    false,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply"),
    base::ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    base::ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    base::string16(),
    false,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply&espv=1"),
    base::ASCIIToUTF16("https://google.com/search?q=tractor+supply&espv=1"),
    base::ASCIIToUTF16("tractor supply"),
    base::ASCIIToUTF16("tractor supply"),
    true,
    true
  },
  {
    GURL("https://google.com/search?q=tractorsupply.com&espv=1"),
    base::ASCIIToUTF16("https://google.com/search?q=tractorsupply.com&espv=1"),
    base::ASCIIToUTF16("tractorsupply.com"),
    base::ASCIIToUTF16("tractorsupply.com"),
    true,
    true
  },
  {
    GURL("https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    base::ASCIIToUTF16(
        "https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    base::ASCIIToUTF16("ftp://tractorsupply.ie"),
    base::ASCIIToUTF16("ftp://tractorsupply.ie"),
    true,
    true
  },
};

}  // namespace


// ToolbarModelTest -----------------------------------------------------------

class ToolbarModelTest : public BrowserWithTestWindowTest {
 public:
  ToolbarModelTest();
  ToolbarModelTest(Browser::Type browser_type,
                   chrome::HostDesktopType host_desktop_type,
                   bool hosted_app);
  ~ToolbarModelTest() override;

  // BrowserWithTestWindowTest:
  void SetUp() override;

 protected:
  void NavigateAndCheckText(const GURL& url,
                            const base::string16& expected_text,
                            bool would_perform_search_term_replacement,
                            bool should_display_url);
  void NavigateAndCheckElided(const GURL& https_url);

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModelTest);
};

ToolbarModelTest::ToolbarModelTest() {
}

ToolbarModelTest::ToolbarModelTest(
    Browser::Type browser_type,
    chrome::HostDesktopType host_desktop_type,
    bool hosted_app)
    : BrowserWithTestWindowTest(browser_type,
                                host_desktop_type,
                                hosted_app) {
}

ToolbarModelTest::~ToolbarModelTest() {
}

void ToolbarModelTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &AutocompleteClassifierFactory::BuildInstanceFor);
  UIThreadSearchTermsData::SetGoogleBaseURL("http://google.com/");
}

void ToolbarModelTest::NavigateAndCheckText(
    const GURL& url,
    const base::string16& expected_text,
    bool would_perform_search_term_replacement,
    bool should_display_url) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  EXPECT_EQ(expected_text, toolbar_model->GetText());
  EXPECT_EQ(would_perform_search_term_replacement,
            toolbar_model->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(should_display_url, toolbar_model->ShouldDisplayURL());

  // Check after commit.
  CommitPendingLoad(controller);
  // Fake a secure connection for HTTPS URLs, or the toolbar will refuse to
  // extract search terms.
  if (url.SchemeIsCryptographic()) {
    controller->GetVisibleEntry()->GetSSL().security_style =
        content::SECURITY_STYLE_AUTHENTICATED;
  }
  EXPECT_EQ(expected_text, toolbar_model->GetText());
  EXPECT_EQ(would_perform_search_term_replacement,
            toolbar_model->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(should_display_url, toolbar_model->ShouldDisplayURL());

  // Now pretend the user started modifying the omnibox.
  toolbar_model->set_input_in_progress(true);
  EXPECT_FALSE(toolbar_model->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(would_perform_search_term_replacement,
            toolbar_model->WouldPerformSearchTermReplacement(true));

  // Tell the ToolbarModel that the user has stopped editing.  This prevents
  // this function from having side effects.
  toolbar_model->set_input_in_progress(false);
}

void ToolbarModelTest::NavigateAndCheckElided(const GURL& url) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  const base::string16 toolbar_text_before(toolbar_model->GetText());
  EXPECT_LT(toolbar_text_before.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(toolbar_text_before,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
  // Check after commit.
  CommitPendingLoad(controller);
  const base::string16 toolbar_text_after(toolbar_model->GetText());
  EXPECT_LT(toolbar_text_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(toolbar_text_after,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
}

class PopupToolbarModelTest : public ToolbarModelTest {
 public:
  PopupToolbarModelTest();
  ~PopupToolbarModelTest() override;

  DISALLOW_COPY_AND_ASSIGN(PopupToolbarModelTest);
};

PopupToolbarModelTest::PopupToolbarModelTest()
      : ToolbarModelTest(Browser::TYPE_POPUP,
                         chrome::HOST_DESKTOP_TYPE_NATIVE,
                         false) {
}

PopupToolbarModelTest::~PopupToolbarModelTest() {
}

// Actual tests ---------------------------------------------------------------

// Test that we only replace URLs when query extraction and URL replacement
// are enabled.
TEST_F(ToolbarModelTest, ShouldDisplayURL_QueryExtraction) {
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // Before we enable instant extended, query extraction is disabled.
  EXPECT_FALSE(search::IsQueryExtractionEnabled())
      << "This test expects query extraction to be disabled.";
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }

  search::EnableQueryExtractionForTesting();
  EXPECT_TRUE(search::IsQueryExtractionEnabled());
  EXPECT_TRUE(browser()->toolbar_model()->url_replacement_enabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_query_extraction,
                         test_item.would_perform_search_term_replacement,
                         test_item.should_display_url);
  }

  // Disabling URL replacement should reset to only showing URLs.
  browser()->toolbar_model()->set_url_replacement_enabled(false);
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }
}

 // Verify that search terms are extracted while the page is loading.
TEST_F(ToolbarModelTest, SearchTermsWhileLoading) {
  search::EnableQueryExtractionForTesting();
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // While loading, we should be willing to extract search terms.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  controller->LoadURL(GURL("https://google.com/search?q=tractor+supply&espv=1"),
                      content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  controller->GetVisibleEntry()->GetSSL().security_style =
      content::SECURITY_STYLE_UNKNOWN;
  EXPECT_TRUE(toolbar_model->WouldPerformSearchTermReplacement(false));

  // When done loading, we shouldn't extract search terms if we didn't get an
  // authenticated connection.
  CommitPendingLoad(controller);
  controller->GetVisibleEntry()->GetSSL().security_style =
      content::SECURITY_STYLE_UNKNOWN;
  EXPECT_FALSE(toolbar_model->WouldPerformSearchTermReplacement(false));
}

// When the Google base URL is overridden on the command line, we should extract
// search terms from URLs that start with that base URL even when they're not
// secure.
TEST_F(ToolbarModelTest, GoogleBaseURL) {
  search::EnableQueryExtractionForTesting();
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // If the Google base URL wasn't specified on the command line, then if it's
  // HTTP, we should not extract search terms.
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.foo.com/");
  NavigateAndCheckText(
      GURL("http://www.foo.com/search?q=tractor+supply&espv=1"),
      base::ASCIIToUTF16("www.foo.com/search?q=tractor+supply&espv=1"), false,
      true);

  // The same URL, when specified on the command line, should allow search term
  // extraction.
  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");

  // For the default search engine to reflect the new Google base URL we just
  // set, the TemplateURLService has to know the base URL changed. Normally the
  // GoogleURLTracker is the source of such change notifications, but since here
  // we're modifying the base URL value directly by changing the command-line
  // flags, we need to manually tell TemplateURLService to check for changes.
  TemplateURLServiceFactory::GetInstance()->GetForProfile(profile())->
      GoogleBaseURLChanged();

  NavigateAndCheckText(
      GURL("http://www.foo.com/search?q=tractor+supply&espv=1"),
      base::ASCIIToUTF16("tractor supply"), true, true);
}

TEST_F(ToolbarModelTest, ShouldElideLongURLs) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  const std::string long_text(content::kMaxURLDisplayChars + 1024, '0');
  NavigateAndCheckElided(
      GURL(std::string("https://www.foo.com/?") + long_text));
  NavigateAndCheckElided(GURL(std::string("data:abc") + long_text));
}

// Test that URL display in a popup respects the query extraction flag.
TEST_F(PopupToolbarModelTest, ShouldDisplayURL) {
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // Check with query extraction disabled.
  EXPECT_FALSE(search::IsQueryExtractionEnabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }

  // With query extraction enabled, search term replacement should be performed.
  search::EnableQueryExtractionForTesting();
  EXPECT_TRUE(search::IsQueryExtractionEnabled());
  EXPECT_TRUE(browser()->toolbar_model()->url_replacement_enabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_query_extraction,
                         test_item.would_perform_search_term_replacement,
                         test_item.should_display_url);
  }

  // Disabling URL replacement should reset to only showing URLs.
  browser()->toolbar_model()->set_url_replacement_enabled(false);
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }
}
