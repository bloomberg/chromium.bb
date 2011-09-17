// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bidi_checker_web_ui_test.h"

#include "base/base_paths.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

// These tests are failing on linux views build. crbug.com/96891
#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_TestCrashesPageRTL DISABLED_TestCrashesPageRTL
#define MAYBE_TestDownloadsPageRTL DISABLED_TestDownloadsPageRTL
#define MAYBE_TestMainHistoryPageRTL DISABLED_TestMainHistoryPageRTL
#define MAYBE_TestNewTabPageRTL DISABLED_TestNewTabPageRTL
#define MAYBE_TestPluginsPageRTL DISABLED_TestPluginsPageRTL
#define MAYBE_TestSettingsAutofillPageRTL DISABLED_TestSettingsAutofillPageRTL
#define MAYBE_TestSettingsPageRTL DISABLED_TestSettingsPageRTL
#else
#define MAYBE_TestCrashesPageRTL TestCrashesPageRTL
#define MAYBE_TestDownloadsPageRTL TestDownloadsPageRTL
#define MAYBE_TestMainHistoryPageRTL TestMainHistoryPageRTL
#define MAYBE_TestNewTabPageRTL TestNewTabPageRTL
#define MAYBE_TestPluginsPageRTL TestPluginsPageRTL
#define MAYBE_TestSettingsAutofillPageRTL TestSettingsAutofillPageRTL
#define MAYBE_TestSettingsPageRTL TestSettingsPageRTL
#endif


static const FilePath::CharType* kWebUIBidiCheckerLibraryJS =
    FILE_PATH_LITERAL("third_party/bidichecker/bidichecker_packaged.js");

namespace {
FilePath WebUIBidiCheckerLibraryJSPath() {
  FilePath src_root;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_root))
    LOG(ERROR) << "Couldn't find source root";
  return src_root.Append(kWebUIBidiCheckerLibraryJS);
}
}

static const FilePath::CharType* kBidiCheckerTestsJS =
    FILE_PATH_LITERAL("bidichecker_tests.js");

WebUIBidiCheckerBrowserTest::~WebUIBidiCheckerBrowserTest() {}

WebUIBidiCheckerBrowserTest::WebUIBidiCheckerBrowserTest() {}

void WebUIBidiCheckerBrowserTest::SetUpInProcessBrowserTestFixture() {
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
  WebUIBrowserTest::AddLibrary(WebUIBidiCheckerLibraryJSPath());
  WebUIBrowserTest::AddLibrary(FilePath(kBidiCheckerTestsJS));
}

void WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(const char pageURL[],
                                                       bool isRTL) {
  ui_test_utils::NavigateToURL(browser(), GURL(pageURL));
  ASSERT_TRUE(RunJavascriptTest("runBidiChecker",
                                Value::CreateStringValue(pageURL),
                                Value::CreateBooleanValue(isRTL)));
}

// WebUIBidiCheckerBrowserTestFakeBidi

WebUIBidiCheckerBrowserTestFakeBidi::~WebUIBidiCheckerBrowserTestFakeBidi() {}

WebUIBidiCheckerBrowserTestFakeBidi::WebUIBidiCheckerBrowserTestFakeBidi() {}

void WebUIBidiCheckerBrowserTestFakeBidi::SetUpInProcessBrowserTestFixture() {
  WebUIBidiCheckerBrowserTest::SetUpInProcessBrowserTestFixture();
  FilePath pak_path;
  app_locale_ = base::i18n::GetConfiguredLocale();
  PathService::Get(base::FILE_MODULE, &pak_path);
  pak_path = pak_path.DirName();
  pak_path = pak_path.AppendASCII("pseudo_locales");
  pak_path = pak_path.AppendASCII("fake-bidi");
  pak_path = pak_path.ReplaceExtension(FILE_PATH_LITERAL("pak"));
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(pak_path);
  ResourceBundle::ReloadSharedInstance("he");
#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);
#endif
}

void
WebUIBidiCheckerBrowserTestFakeBidi::TearDownInProcessBrowserTestFixture() {
  WebUIBidiCheckerBrowserTest::TearDownInProcessBrowserTestFixture();
#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_LTR);
#endif
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(FilePath());
  ResourceBundle::ReloadSharedInstance(app_locale_);
}

// Tests

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestMainHistoryPageLTR) {
  HistoryService* history_service =
      browser()->profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  GURL history_url = GURL("http://www.ynet.co.il");
  history_service->AddPage(history_url, history::SOURCE_BROWSED);
  string16 title;
  ASSERT_TRUE(UTF8ToUTF16("\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x21",
                          12,
                          &title));
  history_service->SetPageTitle(history_url, title);
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestMainHistoryPageRTL) {
  HistoryService* history_service =
      browser()->profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  GURL history_url = GURL("http://www.google.com");
  history_service->AddPage(history_url, history::SOURCE_BROWSED);
  string16 title = UTF8ToUTF16("Google");
  history_service->SetPageTitle(history_url, title);
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(chrome::kChromeUIHistoryURL,
                                                    true);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestAboutPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIAboutURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestBugReportPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIBugReportURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestCrashesPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUICrashesURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestCrashesPageRTL) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(chrome::kChromeUICrashesURL,
                                                    true);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestDownloadsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestDownloadsPageRTL) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(
      chrome::kChromeUIDownloadsURL, true);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestNewTabPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestNewTabPageRTL) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(chrome::kChromeUINewTabURL,
                                                    true);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestPluginsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIPluginsURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestPluginsPageRTL) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(chrome::kChromeUIPluginsURL,
                                                    true);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestSettingsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsURL, false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestSettingsPageRTL) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(
      chrome::kChromeUISettingsURL, true);
}

#if defined(OS_MACOSX)
// http://crbug.com/94642
#define MAYBE_TestSettingsAutofillPageLTR FLAKY_TestSettingsAutofillPageLTR
#elif defined(OS_WIN)
// http://crbug.com/95425
#define MAYBE_TestSettingsAutofillPageLTR FAILS_TestSettingsAutofillPageLTR
#else
#define MAYBE_TestSettingsAutofillPageLTR TestSettingsAutofillPageLTR
#endif  // defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest,
                       MAYBE_TestSettingsAutofillPageLTR) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kAutofillSubPage);

  autofill_test::DisableSystemServices(browser()->profile());
  AutofillProfile profile;
  autofill_test::SetProfileInfo(
      &profile,
      "\xD7\x9E\xD7\xA9\xD7\x94",
      "\xD7\x91",
      "\xD7\x9B\xD7\x94\xD7\x9F",
      "moshe.b.cohen@biditest.com",
      "\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x20\xD7\x91\xD7\xA2\xD7\x9E",
      "\xD7\x93\xD7\xA8\xD7\x9A\x20\xD7\x9E\xD7\xA0\xD7\x97\xD7\x9D\x20\xD7\x91\xD7\x92\xD7\x99\xD7\x9F\x20\x32\x33",
      "\xD7\xA7\xD7\x95\xD7\x9E\xD7\x94\x20\x32\x36",
      "\xD7\xAA\xD7\x9C\x20\xD7\x90\xD7\x91\xD7\x99\xD7\x91",
      "",
      "66183",
      "\xD7\x99\xD7\xA9\xD7\xA8\xD7\x90\xD7\x9C",
      "0000");

  PersonalDataManager* personal_data_manager =
    browser()->profile()->GetPersonalDataManager();
  ASSERT_TRUE(personal_data_manager);

  personal_data_manager->AddProfile(profile);

  RunBidiCheckerOnPage(url.c_str(), false);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestFakeBidi,
                       MAYBE_TestSettingsAutofillPageRTL) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kAutofillSubPage);

  autofill_test::DisableSystemServices(browser()->profile());
  AutofillProfile profile;
  autofill_test::SetProfileInfo(
                                &profile,
                                "Milton",
                                "C.",
                                "Waddams",
                                "red.swingline@initech.com",
                                "Initech",
                                "4120 Freidrich Lane",
                                "Basement",
                                "Austin",
                                "Texas",
                                "78744",
                                "United States",
                                "5125551234");

  PersonalDataManager* personal_data_manager =
      browser()->profile()->GetPersonalDataManager();
  ASSERT_TRUE(personal_data_manager);

  personal_data_manager->AddProfile(profile);

  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(url.c_str(), true);
}
