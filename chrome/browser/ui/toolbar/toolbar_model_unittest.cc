// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

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
  bool would_replace;
  bool should_display;
} test_items[] = {
  {
    GURL("view-source:http://www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    ASCIIToUTF16("view-source:www.google.com"),
    false,
    true
  },
  {
    GURL("view-source:chrome://newtab/"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    ASCIIToUTF16("view-source:chrome://newtab"),
    false,
    true
  },
  {
    GURL("chrome-extension://monkey/balls.html"),
    string16(),
    string16(),
    string16(),
    false,
    false
  },
  {
    GURL("chrome://newtab/"),
    string16(),
    string16(),
    string16(),
    false,
    false
  },
  {
    GURL(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    ASCIIToUTF16(chrome::kAboutBlankURL),
    false,
    true
  },
  {
    GURL("http://searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    ASCIIToUTF16("searchurl/?q=tractor+supply"),
    false,
    true
  },
  {
    GURL("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.ca/search?q=tractor+supply"),
    false,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply"),
    false,
    true
  },
  {
    GURL("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("https://google.com/search?q=tractor+supply&espv=1"),
    ASCIIToUTF16("tractor supply"),
    true,
    true
  }
};

}  // end namespace

class ToolbarModelTest : public BrowserWithTestWindowTest {
 public:
  ToolbarModelTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
  }
  virtual void TearDown() OVERRIDE { BrowserWithTestWindowTest::TearDown(); }

 protected:

  void ResetDefaultTemplateURL() {
    TemplateURLData data;
    data.SetURL("http://google.com/search?q={searchTerms}");
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
                            bool would_replace,
                            bool should_display) {
    NavigateAndCheckTextImpl(url, false, expected_text, would_replace,
                             should_display);
    NavigateAndCheckTextImpl(url, true, expected_replace_text, would_replace,
                             should_display);
  }

 private:
  void NavigateAndCheckTextImpl(const GURL& url,
                                bool can_replace,
                                const string16 expected_text,
                                bool would_replace,
                                bool should_display) {
    WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    browser()->OpenURL(OpenURLParams(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
        false));

    ToolbarModel* toolbar_model = browser()->toolbar_model();

    // Check while loading.
    EXPECT_EQ(should_display, toolbar_model->ShouldDisplayURL());
    EXPECT_EQ(expected_text, toolbar_model->GetText(can_replace));
    EXPECT_EQ(would_replace,
              toolbar_model->WouldReplaceSearchURLWithSearchTerms());

    // Check after commit.
    CommitPendingLoad(&contents->GetController());
    EXPECT_EQ(should_display, toolbar_model->ShouldDisplayURL());
    EXPECT_EQ(expected_text, toolbar_model->GetText(can_replace));
    EXPECT_EQ(would_replace,
              toolbar_model->WouldReplaceSearchURLWithSearchTerms());
  }
};

// Test that we don't replace any URLs when the query extraction is disabled.
TEST_F(ToolbarModelTest, ShouldDisplayURLQueryExtractionDisabled) {
  ASSERT_FALSE(chrome::search::IsQueryExtractionEnabled(profile()))
      << "This test expects query extraction to be disabled.";

  ResetDefaultTemplateURL();
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text,
                         test_item.expected_replace_text_inactive,
                         false,
                         test_item.should_display);
  }
}

// Test that we replace URLs when the query extraction API is enabled.
TEST_F(ToolbarModelTest, ShouldDisplayURLQueryExtractionEnabled) {
  chrome::search::EnableQueryExtractionForTesting();

  ResetDefaultTemplateURL();
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  for (size_t i = 0; i < arraysize(test_items); ++i) {
    const TestItem& test_item = test_items[i];
    NavigateAndCheckText(test_item.url,
                         test_item.expected_text,
                         test_item.expected_replace_text_active,
                         test_item.would_replace,
                         test_item.should_display);
  }
}
