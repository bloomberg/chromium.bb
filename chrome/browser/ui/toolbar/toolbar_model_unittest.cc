// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/toolbar_model.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/toolbar/toolbar_model.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/common/extension_builder.h"
#endif

// Test data ------------------------------------------------------------------

namespace {

struct TestItem {
  GURL url;
  const char* expected_formatted_full_url;
  const char* expected_elided_url_for_display = expected_formatted_full_url;
} test_items[] = {
    {
        GURL("view-source:http://www.google.com"), "view-source:www.google.com",
        "view-source:google.com",
    },
    {
        GURL("chrome://newtab/"), "",
    },
    {
        GURL("view-source:chrome://newtab/"), "view-source:chrome://newtab",
    },
    {
        GURL("chrome-search://local-ntp/local-ntp.html"), "",
    },
    {
        GURL("view-source:chrome-search://local-ntp/local-ntp.html"),
        "view-source:chrome-search://local-ntp/local-ntp.html",
    },
    {
        GURL("chrome-extension://fooooooooooooooooooooooooooooooo/bar.html"),
        "chrome-extension://fooooooooooooooooooooooooooooooo/bar.html",
    },
    {
        GURL(url::kAboutBlankURL), url::kAboutBlankURL,
    },
    {
        GURL("http://searchurl/?q=tractor+supply"),
        "searchurl/?q=tractor+supply",
    },
    {
        GURL("http://www.google.com/search?q=tractor+supply"),
        "www.google.com/search?q=tractor+supply",
        "google.com/search?q=tractor+supply",
    },
    {
        GURL("https://m.google.ca/search?q=tractor+supply"),
        "https://m.google.ca/search?q=tractor+supply",
        "google.ca/search?q=tractor+supply",
    },
};

}  // namespace


// ToolbarModelTest -----------------------------------------------------------

class ToolbarModelTest : public BrowserWithTestWindowTest {
 public:
  ToolbarModelTest();
  ~ToolbarModelTest() override;

  // BrowserWithTestWindowTest:
  void SetUp() override;

 protected:
  void NavigateAndCheckText(
      const GURL& url,
      const base::string16& expected_formatted_full_url,
      const base::string16& expected_elided_url_for_display);
  void NavigateAndCheckElided(const GURL& https_url);

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarModelTest);
};

ToolbarModelTest::ToolbarModelTest() {
}

ToolbarModelTest::~ToolbarModelTest() {
}

void ToolbarModelTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &AutocompleteClassifierFactory::BuildInstanceFor);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Install a fake extension so that the ID in the chrome-extension test URL is
  // valid. Invalid extension URLs may result in error pages (if blocked by
  // ExtensionNavigationThrottle), which this test doesn't wish to exercise.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test")
          .SetID("fooooooooooooooooooooooooooooooo")
          .Build();
  extension_system->extension_service()->AddExtension(extension.get());
#endif
}

void ToolbarModelTest::NavigateAndCheckText(
    const GURL& url,
    const base::string16& expected_formatted_full_url,
    const base::string16& expected_elided_url_for_display) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  EXPECT_EQ(expected_formatted_full_url, toolbar_model->GetFormattedFullURL());
  EXPECT_NE(expected_formatted_full_url.empty(),
            toolbar_model->ShouldDisplayURL());
  EXPECT_EQ(expected_elided_url_for_display, toolbar_model->GetURLForDisplay());

  // Check after commit.
  CommitPendingLoad(controller);
  EXPECT_EQ(expected_formatted_full_url, toolbar_model->GetFormattedFullURL());
  EXPECT_NE(expected_formatted_full_url.empty(),
            toolbar_model->ShouldDisplayURL());
  EXPECT_EQ(expected_elided_url_for_display, toolbar_model->GetURLForDisplay());
}

void ToolbarModelTest::NavigateAndCheckElided(const GURL& url) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  ToolbarModel* toolbar_model = browser()->toolbar_model();
  const base::string16 formatted_full_url_before(
      toolbar_model->GetFormattedFullURL());
  EXPECT_LT(formatted_full_url_before.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(formatted_full_url_before,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
  const base::string16 display_url_before(toolbar_model->GetURLForDisplay());
  EXPECT_LT(display_url_before.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(display_url_before,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));

  // Check after commit.
  CommitPendingLoad(controller);
  const base::string16 formatted_full_url_after(
      toolbar_model->GetFormattedFullURL());
  EXPECT_LT(formatted_full_url_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(formatted_full_url_after,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
  const base::string16 display_url_after(toolbar_model->GetURLForDisplay());
  EXPECT_LT(display_url_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(display_url_after,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
}

// Actual tests ---------------------------------------------------------------

// Test URL display.
TEST_F(ToolbarModelTest, ShouldDisplayURL) {
  AddTab(browser(), GURL(url::kAboutBlankURL));

  for (const TestItem& test_item : test_items) {
    NavigateAndCheckText(
        test_item.url,
        base::ASCIIToUTF16(test_item.expected_formatted_full_url),
        base::ASCIIToUTF16(test_item.expected_elided_url_for_display));
  }
}

TEST_F(ToolbarModelTest, ShouldElideLongURLs) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  const std::string long_text(content::kMaxURLDisplayChars + 1024, '0');
  NavigateAndCheckElided(
      GURL(std::string("https://www.foo.com/?") + long_text));
  NavigateAndCheckElided(GURL(std::string("data:abc") + long_text));
}

// Regression test for crbug.com/792401.
TEST_F(ToolbarModelTest, ShouldDisplayURLWhileNavigatingAwayFromNTP) {
  ToolbarModel* toolbar_model = browser()->toolbar_model();

  // Open an NTP. Its URL should not be displayed.
  AddTab(browser(), GURL("chrome://newtab"));
  ASSERT_FALSE(toolbar_model->ShouldDisplayURL());
  ASSERT_TRUE(toolbar_model->GetFormattedFullURL().empty());

  const std::string other_url = "https://www.foo.com";

  // Start loading another page. Its URL should be displayed, even though the
  // current page is still the NTP.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(GURL(other_url), content::Referrer(),
                      ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(toolbar_model->ShouldDisplayURL());
  EXPECT_EQ(base::ASCIIToUTF16(other_url),
            toolbar_model->GetFormattedFullURL());

  // Of course the same should still hold after committing.
  CommitPendingLoad(controller);
  EXPECT_TRUE(toolbar_model->ShouldDisplayURL());
  EXPECT_EQ(base::ASCIIToUTF16(other_url),
            toolbar_model->GetFormattedFullURL());
}
