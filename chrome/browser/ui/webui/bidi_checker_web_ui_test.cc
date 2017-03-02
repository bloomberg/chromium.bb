// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bidi_checker_web_ui_test.h"

#include "base/base_paths.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

// Test cases here are disabled on all platforms due to http://crbug.com/511439

using autofill::AutofillProfile;
using autofill::PersonalDataManager;

static const base::FilePath::CharType* kWebUIBidiCheckerLibraryJS =
    FILE_PATH_LITERAL("third_party/bidichecker/bidichecker_packaged.js");

namespace {
base::FilePath WebUIBidiCheckerLibraryJSPath() {
  base::FilePath src_root;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_root))
    LOG(ERROR) << "Couldn't find source root";
  return src_root.Append(kWebUIBidiCheckerLibraryJS);
}

// Since synchronization isn't complete for the ResourceBundle class, reload
// locale resources on the IO thread.
// crbug.com/95425, crbug.com/132752
void ReloadLocaleResourcesOnIOThread(const std::string& new_locale) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    LOG(ERROR)
        << content::BrowserThread::IO
        << " != " << base::PlatformThread::CurrentId();
    NOTREACHED();
  }

  std::string locale;
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io_scope;
    locale.assign(
        ResourceBundle::GetSharedInstance().ReloadLocaleResources(new_locale));
  }
  ASSERT_FALSE(locale.empty());
}

// Since synchronization isn't complete for the ResourceBundle class, reload
// locale resources on the IO thread.
// crbug.com/95425, crbug.com/132752
void ReloadLocaleResources(const std::string& new_locale) {
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ReloadLocaleResourcesOnIOThread, base::ConstRef(new_locale)),
      base::MessageLoop::QuitWhenIdleClosure());
  content::RunMessageLoop();
}

}  // namespace

static const base::FilePath::CharType* kBidiCheckerTestsJS =
    FILE_PATH_LITERAL("bidichecker_tests.js");

void WebUIBidiCheckerBrowserTest::SetUp() {
  argv_ = base::CommandLine::ForCurrentProcess()->GetArgs();
}

void WebUIBidiCheckerBrowserTest::TearDown() {
  // Reset command line to the way it was before the test was run.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(argv_);
}

WebUIBidiCheckerBrowserTest::~WebUIBidiCheckerBrowserTest() {}

WebUIBidiCheckerBrowserTest::WebUIBidiCheckerBrowserTest() {}

void WebUIBidiCheckerBrowserTest::SetUpInProcessBrowserTestFixture() {
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
  WebUIBrowserTest::AddLibrary(WebUIBidiCheckerLibraryJSPath());
  WebUIBrowserTest::AddLibrary(base::FilePath(kBidiCheckerTestsJS));
}

void WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(
    const std::string& page_url, bool is_rtl) {
  ui_test_utils::NavigateToURL(browser(), GURL(page_url));
  ASSERT_TRUE(RunJavascriptTest("runBidiChecker",
                                new base::StringValue(page_url),
                                new base::Value(is_rtl)));
}

void DISABLED_WebUIBidiCheckerBrowserTestLTR::RunBidiCheckerOnPage(
    const std::string& page_url) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(page_url, false);
}

void DISABLED_WebUIBidiCheckerBrowserTestRTL::RunBidiCheckerOnPage(
    const std::string& page_url) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(page_url, true);
}

void DISABLED_WebUIBidiCheckerBrowserTestRTL::SetUpOnMainThread() {
  WebUIBidiCheckerBrowserTest::SetUpOnMainThread();
  base::FilePath pak_path;
  app_locale_ = base::i18n::GetConfiguredLocale();
  ASSERT_TRUE(PathService::Get(base::FILE_MODULE, &pak_path));
  pak_path = pak_path.DirName();
  pak_path = pak_path.AppendASCII("pseudo_locales");
  pak_path = pak_path.AppendASCII("fake-bidi");
  pak_path = pak_path.ReplaceExtension(FILE_PATH_LITERAL("pak"));
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(pak_path);
  ReloadLocaleResources("he");
  base::i18n::SetICUDefaultLocale("he");
}

void DISABLED_WebUIBidiCheckerBrowserTestRTL::TearDownOnMainThread() {
  WebUIBidiCheckerBrowserTest::TearDownOnMainThread();

  base::i18n::SetICUDefaultLocale(app_locale_);
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(
      base::FilePath());
  ReloadLocaleResources(app_locale_);
}

// Tests

//==============================
// chrome://settings/history
//==============================

static void SetupHistoryPageTest(Browser* browser,
                                 const std::string& page_url,
                                 const std::string& page_title) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser->profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  const GURL history_url = GURL(page_url);
  history_service->AddPage(
      history_url, base::Time::Now(), history::SOURCE_BROWSED);
  history_service->SetPageTitle(history_url, base::UTF8ToUTF16(page_title));
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestHistoryPage) {
  // Test an Israeli news site with a Hebrew title.
  SetupHistoryPageTest(browser(),
                       "http://www.ynet.co.il",
                       "\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x21");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestHistoryPage) {
  SetupHistoryPageTest(browser(), "http://www.google.com", "Google");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}

//==============================
// chrome://about
//==============================

// This page isn't localized to an RTL language so we test it only in English.
IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR, TestAboutPage) {
  RunBidiCheckerOnPage(chrome::kChromeUIAboutURL);
}

//==============================
// chrome://crashes
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestCrashesPage) {
  RunBidiCheckerOnPage(chrome::kChromeUICrashesURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestCrashesPage) {
  RunBidiCheckerOnPage(chrome::kChromeUICrashesURL);
}

//==============================
// chrome://downloads
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestDownloadsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestDownloadsPageRTL) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

//==============================
// chrome://newtab
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestNewTabPage) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestNewTabPage) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL);
}

//==============================
// chrome://settings-frame
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsFrameURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsFrameURL);
}

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
  autofill::test::DisableSystemServices(profile->GetPrefs());
  AutofillProfile autofill_profile;
  autofill::test::SetProfileInfo(&autofill_profile,
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
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(personal_data_manager);
  personal_data_manager->AddProfile(autofill_profile);
}

static void TearDownSettingsAutofillPageTest() {
  autofill::test::ReenableSystemServices();
}

// http://crbug.com/94642
IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       DISABLED_TestSettingsAutofillPage) {
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
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kAutofillSubPage);
  RunBidiCheckerOnPage(url);
  TearDownSettingsAutofillPageTest();
}

// http://crbug.com/94642
IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsAutofillPage) {
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
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kAutofillSubPage);
  RunBidiCheckerOnPage(url);
  TearDownSettingsAutofillPageTest();
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url +=
      std::string(chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url +=
      std::string(chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url);
}

//===================================
// chrome://settings-frame/startup
//===================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameStartup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "startup";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameStartup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "startup";
  RunBidiCheckerOnPage(url);
}

//===================================
// chrome://settings-frame/importData
//===================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameImportData) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kImportDataSubPage;
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameImportData) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kImportDataSubPage;
  RunBidiCheckerOnPage(url);
}

#if !defined(OS_CHROMEOS)
//========================================
// chrome://settings-frame/manageProfile
//========================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameMangageProfile) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kManageProfileSubPage;
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameMangageProfile) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kManageProfileSubPage;
  RunBidiCheckerOnPage(url);
}
#endif  // !defined(OS_CHROMEOS)

//===================================================
// chrome://settings-frame/contentExceptions#cookies
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#cookies";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#cookies";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#images
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsImages) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#images";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsImages) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#images";
  RunBidiCheckerOnPage(url);
}

//======================================================
// chrome://settings-frame/contentExceptions#javascript
//======================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsJavascript) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#javascript";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsJavascript) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#javascript";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#plugins
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsPlugins) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#plugins";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsPlugins) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#plugins";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#popups
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsPopups) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#popups";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsPopups) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#popups";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#location
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsLocation) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#location";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsLocation) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#location";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#notifications
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsNotifications) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#notifications";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsNotifications) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#notifications";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#mouselock
//===================================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsMouseLock) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#mouselock";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameContentExceptionsMouseLock) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kDeprecatedOptionsContentSettingsExceptionsSubPage;
  url += "#mouselock";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/handlers
//========================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameHandler) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kHandlerSettingsSubPage;
  RunBidiCheckerOnPage(url);
}

// Fails on chromeos. http://crbug.com/125367
#if defined(OS_CHROMEOS)
#define MAYBE_TestSettingsFrameHandler DISABLED_TestSettingsFrameHandler
#else
#define MAYBE_TestSettingsFrameHandler TestSettingsFrameHandler
#endif

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameHandler) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kHandlerSettingsSubPage;
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/cookies
//========================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "cookies";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "cookies";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/passwords
//========================================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFramePasswords) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "passwords";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFramePasswords) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "passwords";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/fonts
//========================================

#if defined(OS_MACOSX)
#define MAYBE_TestSettingsFrameFonts DISABLED_TestSettingsFrameFonts
#else
#define MAYBE_TestSettingsFrameFonts TestSettingsFrameFonts
#endif
IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       MAYBE_TestSettingsFrameFonts) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "fonts";
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestSettingsFrameFonts) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "fonts";
  RunBidiCheckerOnPage(url);
}

// Test other uber iframes.

//==============================
// chrome://extensions-frame
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestExtensionsFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIExtensionsFrameURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestExtensionsFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIExtensionsFrameURL);
}

//==============================
// chrome://help-frame
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR, TestHelpFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHelpFrameURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL, TestHelpFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHelpFrameURL);
}

//==============================
// chrome://history-frame
//==============================

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestLTR,
                       TestHistoryFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebUIBidiCheckerBrowserTestRTL,
                       TestHistoryFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}
