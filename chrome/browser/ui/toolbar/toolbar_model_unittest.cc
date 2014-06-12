// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/google/core/browser/google_switches.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"


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
  virtual ~ToolbarModelTest();

  // BrowserWithTestWindowTest:
  virtual void SetUp() OVERRIDE;

 protected:
  void EnableOriginChipFieldTrial();
  void NavigateAndCheckText(const GURL& url,
                            const base::string16& expected_text,
                            bool would_perform_search_term_replacement,
                            bool should_display_url);

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

void ToolbarModelTest::EnableOriginChipFieldTrial() {
  field_trial_list_.reset(new base::FieldTrialList(
      new metrics::SHA1EntropyProvider("platypus")));
  base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                         "Group1 espv:2 origin_chip:1");
}

void ToolbarModelTest::NavigateAndCheckText(
    const GURL& url,
    const base::string16& expected_text,
    bool would_perform_search_term_replacement,
    bool should_display_url) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  controller->LoadURL(url, content::Referrer(), content::PAGE_TRANSITION_LINK,
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
  if (url.SchemeIsSecure()) {
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

class PopupToolbarModelTest : public ToolbarModelTest {
 public:
  PopupToolbarModelTest();
  virtual ~PopupToolbarModelTest();

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
  EXPECT_FALSE(chrome::IsQueryExtractionEnabled())
      << "This test expects query extraction to be disabled.";
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }

  chrome::EnableQueryExtractionForTesting();
  EXPECT_TRUE(chrome::IsQueryExtractionEnabled());
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

// Test that we remove or replace URLs appropriately when the origin chip is
// enabled.
TEST_F(ToolbarModelTest, ShouldDisplayURL_OriginChip) {
  EnableOriginChipFieldTrial();
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // Check each case with the origin chip enabled but query extraction disabled.
  EXPECT_TRUE(chrome::ShouldDisplayOriginChip());
  EXPECT_FALSE(chrome::IsQueryExtractionEnabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url, base::string16(), false,
                         test_item.should_display_url);
  }

  // Check with both enabled.
  chrome::EnableQueryExtractionForTesting();
  EXPECT_TRUE(chrome::IsQueryExtractionEnabled());
  EXPECT_TRUE(browser()->toolbar_model()->url_replacement_enabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url, test_item.expected_text_both,
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
  chrome::EnableQueryExtractionForTesting();
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // While loading, we should be willing to extract search terms.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  controller->LoadURL(GURL("https://google.com/search?q=tractor+supply&espv=1"),
                      content::Referrer(), content::PAGE_TRANSITION_LINK,
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
  chrome::EnableQueryExtractionForTesting();
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
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kGoogleBaseURL,
                                                      "http://www.foo.com/");
  NavigateAndCheckText(
      GURL("http://www.foo.com/search?q=tractor+supply&espv=1"),
      base::ASCIIToUTF16("tractor supply"), true, true);
}

// Popup windows don't have an origin chip, so test that URL display in a popup
// ignores whether the origin chip is enabled and only respects the query
// extraction flag.
TEST_F(PopupToolbarModelTest, ShouldDisplayURL) {
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // Check with neither query extraction nor the origin chip enabled.
  EXPECT_FALSE(chrome::ShouldDisplayOriginChip());
  EXPECT_FALSE(chrome::IsQueryExtractionEnabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }

  // Check with the origin chip enabled.
  EnableOriginChipFieldTrial();
  EXPECT_TRUE(chrome::ShouldDisplayOriginChip());
  EXPECT_FALSE(chrome::IsQueryExtractionEnabled());
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text_url_replacement_inactive,
                         false, test_item.should_display_url);
  }

  // With both origin chip and query extraction enabled, only search term
  // replacement should be performed.
  chrome::EnableQueryExtractionForTesting();
  EXPECT_TRUE(chrome::IsQueryExtractionEnabled());
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
