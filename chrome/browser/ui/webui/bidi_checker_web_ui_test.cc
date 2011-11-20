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
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
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

void WebUIBidiCheckerBrowserTestLTR::RunBidiCheckerOnPage(
    const char pageURL[]) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(pageURL, false);
}

void WebUIBidiCheckerBrowserTestRTL::RunBidiCheckerOnPage(
    const char pageURL[]) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(pageURL, true);
}

void WebUIBidiCheckerBrowserTestRTL::SetUpOnMainThread() {
  WebUIBidiCheckerBrowserTest::SetUpOnMainThread();
  FilePath pak_path;
  app_locale_ = base::i18n::GetConfiguredLocale();
  ASSERT_TRUE(PathService::Get(base::FILE_MODULE, &pak_path));
  pak_path = pak_path.DirName();
  pak_path = pak_path.AppendASCII("pseudo_locales");
  pak_path = pak_path.AppendASCII("fake-bidi");
  pak_path = pak_path.ReplaceExtension(FILE_PATH_LITERAL("pak"));
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(pak_path);
  ResourceBundle::ReloadSharedInstance("he");
  base::i18n::SetICUDefaultLocale("he");
#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);
#endif
}

void WebUIBidiCheckerBrowserTestRTL::CleanUpOnMainThread() {
  WebUIBidiCheckerBrowserTest::CleanUpOnMainThread();
#if defined(OS_POSIX) && defined(TOOLKIT_USES_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_LTR);
#endif
  base::i18n::SetICUDefaultLocale(app_locale_);
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(FilePath());
  ResourceBundle::ReloadSharedInstance(app_locale_);
}

// Tests

//==============================
// chrome://history
//==============================

static void SetupHistoryPageTest(Browser* browser,
                                 const std::string page_url,
                                 const std::string page_title) {
  HistoryService* history_service =
      browser->profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  const GURL history_url = GURL(page_url);
  history_service->AddPage(history_url, history::SOURCE_BROWSED);
  history_service->SetPageTitle(history_url, UTF8ToUTF16(page_title));
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestHistoryPage) {
  // Test an Israeli news site with a Hebrew title.
  SetupHistoryPageTest(browser(),
                       "http://www.ynet.co.il",
                       "\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x21");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestHistoryPage) {
  SetupHistoryPageTest(browser(), "http://www.google.com", "Google");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryURL);
}

//==============================
// chrome://about
//==============================

// This page isn't localized to an RTL language so we test it only in English.
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestAboutPage) {
  RunBidiCheckerOnPage(chrome::kChromeUIAboutURL);
}

//==============================
// chrome://bugreport
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestBugReportPage) {
  // The bugreport page receives its contents as GET arguments. Here we provide
  // a custom, Hebrew typed, description message.
  RunBidiCheckerOnPage(
      "chrome://bugreport#0?description=%D7%91%D7%93%D7%99%D7%A7%D7%94"
      "&issueType=1");
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestBugReportPage) {
  RunBidiCheckerOnPage("chrome://bugreport#0?description=test&issueType=1");
}

//==============================
// chrome://crashes
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestCrashesPage) {
  RunBidiCheckerOnPage(chrome::kChromeUICrashesURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestCrashesPage) {
  RunBidiCheckerOnPage(chrome::kChromeUICrashesURL);
}

//==============================
// chrome://downloads
//==============================

#if defined(OS_WIN) || defined(OS_LINUX)
// http://crbug.com/104129
#define TestDownloadsPageLTR FLAKY_TestDownloadsPage
#endif
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestDownloadsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestDownloadsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

//==============================
// chrome://newtab
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestNewTabPage) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL);
}

// http://crbug.com/97453
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestNewTabPage) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL);
}

//==============================
// chrome://plugins
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestPluginsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUIPluginsURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestPluginsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUIPluginsURL);
}

//==============================
// chrome://settings
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestSettingsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestSettingsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsURL);
}

#if defined(OS_MACOSX)
// http://crbug.com/94642
#define MAYBE_TestSettingsAutofillPage FLAKY_TestSettingsAutofillPage
#elif defined(OS_WIN)
// http://crbug.com/95425
#define MAYBE_TestSettingsAutofillPage FAILS_TestSettingsAutofillPage
#else
#define MAYBE_TestSettingsAutofillPage TestSettingsAutofillPage
#endif  // defined(OS_MACOSX)

static void SetupSettingsAutofillPageTest(Profile* profile,
                                          const char* first_name,
                                          const char* middle_name,
                                          const char* last_name,
                                          const char* email,
                                          const char* company,
                                          const char* address1,
                                          const char* address2,
                                          const char* city,
                                          const char* state,
                                          const char* zipcode,
                                          const char* country,
                                          const char* phone) {
  autofill_test::DisableSystemServices(profile);
  AutofillProfile autofill_profile;
  autofill_test::SetProfileInfo(&autofill_profile,
                                first_name,
                                middle_name,
                                last_name,
                                email,
                                company,
                                address1,
                                address2,
                                city,
                                state,
                                zipcode,
                                country,
                                phone);
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(personal_data_manager);
  personal_data_manager->AddProfile(autofill_profile);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsAutofillPage) {
  SetupSettingsAutofillPageTest(browser()->profile(),
                                "\xD7\x9E\xD7\xA9\xD7\x94",
                                "\xD7\x91",
                                "\xD7\x9B\xD7\x94\xD7\x9F",
                                "moshe.b.cohen@biditest.com",
                                "\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x20"
                                "\xD7\x91\xD7\xA2\xD7\x9E",
                                "\xD7\x93\xD7\xA8\xD7\x9A\x20\xD7\x9E\xD7\xA0"
                                "\xD7\x97\xD7\x9D\x20\xD7\x91\xD7\x92\xD7"
                                "\x99\xD7\x9F\x20\x32\x33",
                                "\xD7\xA7\xD7\x95\xD7\x9E\xD7\x94\x20\x32\x36",
                                "\xD7\xAA\xD7\x9C\x20\xD7\x90\xD7\x91\xD7\x99"
                                "\xD7\x91",
                                "",
                                "66183",
                                "\xD7\x99\xD7\xA9\xD7\xA8\xD7\x90\xD7\x9C",
                                "0000");
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kAutofillSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsAutofillPage) {
  SetupSettingsAutofillPageTest(browser()->profile(),
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
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kAutofillSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

static void SetupSettingsBrowserOptionsTest(Profile* profile,
                                            const GURL url,
                                            const std::string title) {
  // First, add a history entry for the site. This is needed so the site's
  // name will appear in the startup sites lists.
  HistoryService* history_service =
      profile->GetHistoryService(Profile::IMPLICIT_ACCESS);
  history_service->AddPage(url, history::SOURCE_BROWSED);
  history_service->SetPageTitle(url, UTF8ToUTF16(title));
  // Next, add the site to the startup sites
  PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);
  pref.urls.push_back(url);
  SessionStartupPref::SetStartupPref(prefs, pref);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsBrowserOptionsPage) {
  SetupSettingsBrowserOptionsTest(browser()->profile(),
                                  GURL("http://ynet.co.il"),
                                  "\x79\x6E\x65\x74\x20\xD7\x97\xD7\x93\xD7"
                                  "\xA9\xD7\x95\xD7\xAA\x20\xD7\xAA\xD7\x95"
                                  "\xD7\x9B\xD7\x9F\x20\xD7\x95\xD7\xA2\xD7"
                                  "\x93\xD7\x9B\xD7\x95\xD7\xA0\xD7\x99\xD7"
                                  "\x9D\x20\x2D\x20\xD7\x99\xD7\x93\xD7\x99"
                                  "\xD7\xA2\xD7\x95\xD7\xAA\x20\xD7\x90\xD7"
                                  "\x97\xD7\xA8\xD7\x95\xD7\xA0\xD7\x95\xD7"
                                  "\xAA");
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kBrowserOptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsBrowserOptionsPage) {
  SetupSettingsBrowserOptionsTest(browser()->profile(),
                                  GURL("http://google.com"),
                                  "Google");
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kBrowserOptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url.c_str());
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url.c_str());
}
