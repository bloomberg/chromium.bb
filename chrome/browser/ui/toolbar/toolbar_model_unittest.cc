// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

struct TestItem {
  GURL url;
  string16 expected_text;
  // The expected text to display when query extraction is inactive.
  string16 expected_replace_text_inactive;
  // The expected text to display when query extraction is active.
  string16 expected_replace_text_active;
  ToolbarModel::SearchTermsType search_terms_type;
  bool should_display;
} test_items[] = {
  {
    GURL("view-source:http://www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("view-source:chrome://newtab/"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("chrome-extension://monkey/balls.html"),
    ASCIIToUTF16("chrome-extension://monkey/balls.html"),
    ASCIIToUTF16("chrome-extension://monkey/balls.html"),
    ASCIIToUTF16("chrome-extension://monkey/balls.html"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("chrome://newtab/"),
    string16(),
    string16(),
    string16(),
    ToolbarModel::NO_SEARCH_TERMS,
    false
  },
  {
    GURL(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("http://searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("tractor supply"),
    ToolbarModel::NORMAL_SEARCH_TERMS,
    true
  },
  {
    GURL("https://google.com/search?q=tractorsupply.com&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractorsupply.com&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractorsupply.com&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractorsupply.com&espv=1"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
  {
    GURL("https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=ftp://tractorsupply.ie&espv=1"),
    ToolbarModel::NO_SEARCH_TERMS,
    true
  },
};

}  // end namespace

class ToolbarModelTest : public BrowserWithTestWindowTest {
 public:
  ToolbarModelTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &AutocompleteClassifierFactory::BuildInstanceFor);
  }
  virtual void TearDown() OVERRIDE { BrowserWithTestWindowTest::TearDown(); }

 protected:

  void ResetDefaultTemplateURL() {
    ResetTemplateURLForInstant(GURL("http://does/not/exist"));
  }

  void ResetTemplateURLForInstant(const GURL& instant_url) {
    TemplateURLData data;
    data.short_name = ASCIIToUTF16("Google");
    data.SetURL("http://google.com/search?q={searchTerms}");
    data.instant_url = instant_url.spec();
    data.search_terms_replacement_key = "{google:instantExtendedEnabledKey}";
    TemplateURL* search_template_url = new TemplateURL(profile(), data);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    template_url_service->Add(search_template_url);
    template_url_service->SetDefaultSearchProvider(search_template_url);
    ASSERT_NE(0, search_template_url->id());
    template_url_service->Load();
  }

  void NavigateAndCheckText(const GURL& url,
                            const string16& expected_text,
                            const string16& expected_replace_text,
                            ToolbarModel::SearchTermsType search_terms_type,
                            bool should_display) {
    NavigateAndCheckTextImpl(url, false, expected_text, search_terms_type,
                             should_display);
    NavigateAndCheckTextImpl(url, true, expected_replace_text,
                             search_terms_type, should_display);
  }

  void NavigateAndCheckQueries(
      const std::vector<const char*>& queries,
      ToolbarModel::SearchTermsType search_terms_type) {
    const std::string kInstantExtendedPrefix(
        "https://google.com/search?espv=1&q=");

    chrome::EnableInstantExtendedAPIForTesting();

    ResetDefaultTemplateURL();
    AddTab(browser(), GURL(chrome::kAboutBlankURL));
    for (size_t i = 0; i < queries.size(); ++i) {
      std::string url_string = kInstantExtendedPrefix +
          net::EscapeQueryParamValue(queries[i], true);
      std::string expected_text;
      if (search_terms_type == ToolbarModel::NO_SEARCH_TERMS)
        expected_text = url_string;
      else
        expected_text = std::string(queries[i]);
      NavigateAndCheckTextImpl(GURL(url_string), true,
                               ASCIIToUTF16(expected_text),
                               search_terms_type, true);
    }
  }

 private:
  void NavigateAndCheckTextImpl(const GURL& url,
                                bool can_replace,
                                const string16 expected_text,
                                ToolbarModel::SearchTermsType search_terms_type,
                                bool should_display) {
    // The URL being navigated to should be treated as the Instant URL. Else
    // there will be no search term extraction.
    ResetTemplateURLForInstant(url);

    WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    browser()->OpenURL(OpenURLParams(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
        false));

    ToolbarModel* toolbar_model = browser()->toolbar_model();

    // Check while loading.
    contents->GetController().GetVisibleEntry()->GetSSL().security_style =
        content::SECURITY_STYLE_AUTHENTICATED;
    EXPECT_EQ(should_display, toolbar_model->ShouldDisplayURL());
    EXPECT_EQ(expected_text, toolbar_model->GetText(can_replace));
    EXPECT_EQ(search_terms_type, toolbar_model->GetSearchTermsType());

    // Check after commit.
    CommitPendingLoad(&contents->GetController());
    contents->GetController().GetVisibleEntry()->GetSSL().security_style =
        content::SECURITY_STYLE_AUTHENTICATED;
    EXPECT_EQ(should_display, toolbar_model->ShouldDisplayURL());
    EXPECT_EQ(expected_text, toolbar_model->GetText(can_replace));
    EXPECT_EQ(search_terms_type, toolbar_model->GetSearchTermsType());
  }
};

// Test that we don't replace any URLs when the query extraction is disabled.
TEST_F(ToolbarModelTest, ShouldDisplayURLQueryExtractionDisabled) {
  ASSERT_FALSE(chrome::IsQueryExtractionEnabled())
      << "This test expects query extraction to be disabled.";

  ResetDefaultTemplateURL();
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text,
                         test_item.expected_replace_text_inactive,
                         ToolbarModel::NO_SEARCH_TERMS,
                         test_item.should_display);
  }
}

// Test that we replace URLs when the query extraction API is enabled.
TEST_F(ToolbarModelTest, ShouldDisplayURLQueryExtractionEnabled) {
  chrome::EnableInstantExtendedAPIForTesting();

  ResetDefaultTemplateURL();
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text,
                         test_item.expected_replace_text_active,
                         test_item.search_terms_type,
                         test_item.should_display);
  }
}

// Test that replacement doesn't happen for URLs.
TEST_F(ToolbarModelTest, DoNotExtractUrls) {
  static const char* const kQueries[] = {
    "www.google.com ",
    "www.google.ca",  // Oh, Canada!
    "www.google.ca.",
    "www.google.ca ",
    "www.google.ca. ",
    "something.xxx",
    "something.travel",
    "bankofamerica.com/",
    "bankofamerica.com/foo",
    "bankofamerica.com/foo bla",
    "BankOfAmerica.BIZ/foo bla",
    "   http:/monkeys",
    "http://monkeys",
    "javascript:alert(1)",
    "JavaScript:alert(1)",
    "localhost",
  };

  NavigateAndCheckQueries(
      std::vector<const char*>(&kQueries[0], &kQueries[arraysize(kQueries)]),
      ToolbarModel::NO_SEARCH_TERMS);
}

// Test that replacement does happen for non-URLs.
TEST_F(ToolbarModelTest, ExtractNonUrls) {
  static const char* const kQueries[] = {
    "facebook.com login",
    "site:amazon.com",
    "e.coli",
    "info:http://www.google.com/",
    "cache:http://www.apple.com/",
    "9/11",
    "<nomatch/>",
    "ac/dc",
    "ps/2",
  };

  NavigateAndCheckQueries(
      std::vector<const char*>(&kQueries[0], &kQueries[arraysize(kQueries)]),
      ToolbarModel::NORMAL_SEARCH_TERMS);
}

// Verify that URL search terms are correctly identified.
TEST_F(ToolbarModelTest, ProminentSearchTerm) {
  static const char* const kQueries[] = {
    "example.com"
  };
  browser()->toolbar_model()->SetSupportsExtractionOfURLLikeSearchTerms(true);
  NavigateAndCheckQueries(
      std::vector<const char*>(&kQueries[0], &kQueries[arraysize(kQueries)]),
      ToolbarModel::URL_LIKE_SEARCH_TERMS);
}

// Verify that search term type is normal while page is loading.
TEST_F(ToolbarModelTest, SearchTermSecurityLevel) {
  chrome::EnableInstantExtendedAPIForTesting();
  browser()->toolbar_model()->SetSupportsExtractionOfURLLikeSearchTerms(true);
  ResetDefaultTemplateURL();
  AddTab(browser(), GURL(chrome::kAboutBlankURL));

  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  contents->GetController().LoadURL(
      GURL("https://google.com/search?q=tractor+supply&espv=1"),
      content::Referrer(),
      content::PAGE_TRANSITION_LINK,
      std::string());

  // While loading search term type should be normal.
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  contents->GetController().GetVisibleEntry()->GetSSL().security_style =
      content::SECURITY_STYLE_UNKNOWN;
  EXPECT_EQ(ToolbarModel::NORMAL_SEARCH_TERMS,
            toolbar_model->GetSearchTermsType());

  CommitPendingLoad(&contents->GetController());

  // When done loading search term type should not be normal.
  contents->GetController().GetVisibleEntry()->GetSSL().security_style =
      content::SECURITY_STYLE_UNKNOWN;
  EXPECT_EQ(ToolbarModel::URL_LIKE_SEARCH_TERMS,
            toolbar_model->GetSearchTermsType());
}
