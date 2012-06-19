// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bidi_checker_web_ui_test.h"

#include "base/base_paths.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(TOOLKIT_GTK)
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

void WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(
    const std::string& page_url, bool is_rtl) {
  ui_test_utils::NavigateToURL(browser(), GURL(page_url));
  ASSERT_TRUE(RunJavascriptTest("runBidiChecker",
                                Value::CreateStringValue(page_url),
                                Value::CreateBooleanValue(is_rtl)));
}

void WebUIBidiCheckerBrowserTestLTR::RunBidiCheckerOnPage(
    const std::string& page_url) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(page_url, false);
}

void WebUIBidiCheckerBrowserTestRTL::RunBidiCheckerOnPage(
    const std::string& page_url) {
  WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(page_url, true);
}

// static
void WebUIBidiCheckerBrowserTestRTL::SetUpOnIOThread(
    base::WaitableEvent* event) {
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
        ResourceBundle::GetSharedInstance().ReloadLocaleResources("he"));
  }
  event->Signal();
  ASSERT_FALSE(locale.empty());
}

// static
void WebUIBidiCheckerBrowserTestRTL::CleanUpOnIOThread(
    base::WaitableEvent* event, const std::string& app_locale) {
  std::string locale;
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io_scope;
    locale.assign(
        ResourceBundle::GetSharedInstance().ReloadLocaleResources(app_locale));
  }
  event->Signal();
  ASSERT_EQ(app_locale, locale);
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

  // Since synchronization isn't complete for the ResourceBundle class, reload
  // locale resources on the IO thread.
  // crbug.com/95425, crbug.com/132752
  // TODO(scr): make this generic to share with CleanUpOnMainThread and use
  // PostTaskAndReply rather than WaitableEvent.
  base::WaitableEvent event(true, false);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&WebUIBidiCheckerBrowserTestRTL::SetUpOnIOThread,
                 base::Unretained(&event)));
  ui_test_utils::WaitEventSignaled(&event);

  base::i18n::SetICUDefaultLocale("he");
#if defined(OS_POSIX) && defined(TOOLKIT_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);
#endif
}

void WebUIBidiCheckerBrowserTestRTL::CleanUpOnMainThread() {
  WebUIBidiCheckerBrowserTest::CleanUpOnMainThread();
#if defined(OS_POSIX) && defined(TOOLKIT_GTK)
  gtk_widget_set_default_direction(GTK_TEXT_DIR_LTR);
#endif

  base::i18n::SetICUDefaultLocale(app_locale_);
  ResourceBundle::GetSharedInstance().OverrideLocalePakForTest(FilePath());

  // Since synchronization isn't complete for the ResourceBundle class, reload
  // locale resources on the IO thread.
  // crbug.com/95425, crbug.com/132752
  // TODO(scr): make this generic to share with SetUpOnMainThread and use
  // PostTaskAndReply rather than WaitableEvent.
  base::WaitableEvent event(true, false);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&WebUIBidiCheckerBrowserTestRTL::CleanUpOnIOThread,
                 base::Unretained(&event),
                 base::ConstRef(app_locale_)));
  ui_test_utils::WaitEventSignaled(&event);
}

// Tests

//==============================
// chrome://settings/history
//==============================

static void SetupHistoryPageTest(Browser* browser,
                                 const std::string page_url,
                                 const std::string page_title) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser->profile(),
                                           Profile::IMPLICIT_ACCESS);
  const GURL history_url = GURL(page_url);
  history_service->AddPage(history_url, history::SOURCE_BROWSED);
  history_service->SetPageTitle(history_url, UTF8ToUTF16(page_title));
}

// TODO(estade): fix this test: http://crbug.com/119595
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestHistoryPage) {
  // Test an Israeli news site with a Hebrew title.
  SetupHistoryPageTest(browser(),
                       "http://www.ynet.co.il",
                       "\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x21");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}

// TODO(estade): fix this test: http://crbug.com/119595
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestHistoryPage) {
  SetupHistoryPageTest(browser(), "http://www.google.com", "Google");
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
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
// TestFeedbackPage fails on chromeos Debug build. crbug.com/105631
#if defined(OS_CHROMEOS)
#define MAYBE_TestFeedbackPage DISABLED_TestFeedbackPage
#else
#define MAYBE_TestFeedbackPage TestFeedbackPage
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       MAYBE_TestFeedbackPage) {
  // The bugreport page receives its contents as GET arguments. Here we provide
  // a custom, Hebrew typed, description message.
  RunBidiCheckerOnPage(
      "chrome://feedback#0?description=%D7%91%D7%93%D7%99%D7%A7%D7%94");
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestFeedbackPage) {
  RunBidiCheckerOnPage("chrome://feedback#0?description=test");
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

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestDownloadsPageLTR) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestDownloadsPageRTL) {
  RunBidiCheckerOnPage(chrome::kChromeUIDownloadsURL);
}

//==============================
// chrome://newtab
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestNewTabPage) {
  RunBidiCheckerOnPage(chrome::kChromeUINewTabURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestNewTabPage) {
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
// chrome://settings-frame
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestSettingsPage) {
  RunBidiCheckerOnPage(chrome::kChromeUISettingsFrameURL);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsPage DISABLED_TestSettingsPage
#else
#define MAYBE_TestSettingsPage TestSettingsPage
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, MAYBE_TestSettingsPage) {
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

// http://crbug.com/94642
// http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
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
}

// http://crbug.com/94642
// http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
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
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsClearBrowserDataPage DISABLED_TestSettingsClearBrowserDataPage
#else
#define MAYBE_TestSettingsClearBrowserDataPage TestSettingsClearBrowserDataPage
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsClearBrowserDataPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kClearBrowserDataSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsContentSettingsPage DISABLED_TestSettingsContentSettingsPage
#else
#define MAYBE_TestSettingsContentSettingsPage TestSettingsContentSettingsPage
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsContentSettingsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url);
}

// TestSettings tests are flaky http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsContentSettingsExceptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kContentSettingsExceptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url);
}

// TestSettings tests are flaky http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsLanguageOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kLanguageOptionsSubPage);
  RunBidiCheckerOnPage(url);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsSearchEnginesOptionsPage DISABLED_TestSettingsSearchEnginesOptionsPage
#else
#define MAYBE_TestSettingsSearchEnginesOptionsPage TestSettingsSearchEnginesOptionsPage
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsSearchEnginesOptionsPage) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSearchEnginesSubPage);
  RunBidiCheckerOnPage(url);
}

//===================================
// chrome://settings-frame/syncSetup
//===================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameSyncSetup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSyncSetupSubPage);
  RunBidiCheckerOnPage(url);
}

// TestSettings tests are flaky on all platforms. http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsFrameSyncSetup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += std::string(chrome::kSyncSetupSubPage);
  RunBidiCheckerOnPage(url);
}

//===================================
// chrome://settings-frame/startup
//===================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameStartup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "startup";
  RunBidiCheckerOnPage(url);
}

// TestSettingsFrameStartup test is flaky on all platforms.
// http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsFrameStartup) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "startup";
  RunBidiCheckerOnPage(url);
}

//===================================
// chrome://settings-frame/importData
//===================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameImportData) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kImportDataSubPage;
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameImportData DISABLED_TestSettingsFrameImportData
#else
#define MAYBE_TestSettingsFrameImportData TestSettingsFrameImportData
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameImportData) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kImportDataSubPage;
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/manageProfile
//========================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameMangageProfile) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kManageProfileSubPage;
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameMangageProfile DISABLED_TestSettingsFrameMangageProfile
#else
#define MAYBE_TestSettingsFrameMangageProfile TestSettingsFrameMangageProfile
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameMangageProfile) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kManageProfileSubPage;
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#cookies
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#cookies";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsCookies DISABLED_TestSettingsFrameContentExceptionsCookies
#else
#define MAYBE_TestSettingsFrameContentExceptionsCookies TestSettingsFrameContentExceptionsCookies
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#cookies";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#images
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsImages) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#images";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsImages DISABLED_TestSettingsFrameContentExceptionsImages
#else
#define MAYBE_TestSettingsFrameContentExceptionsImages TestSettingsFrameContentExceptionsImages
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsImages) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#images";
  RunBidiCheckerOnPage(url);
}

//======================================================
// chrome://settings-frame/contentExceptions#javascript
//======================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsJavascript) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#javascript";
  RunBidiCheckerOnPage(url);
}

// TestSettings tests are flaky http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsFrameContentExceptionsJavascript) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#javascript";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#plugins
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsPlugins) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#plugins";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsPlugins DISABLED_TestSettingsFrameContentExceptionsPlugins
#else
#define MAYBE_TestSettingsFrameContentExceptionsPlugins TestSettingsFrameContentExceptionsPlugins
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsPlugins) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#plugins";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#popups
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsPopups) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#popups";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_MAC)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsPopups DISABLED_TestSettingsFrameContentExceptionsPopups
#else
#define MAYBE_TestSettingsFrameContentExceptionsPopups TestSettingsFrameContentExceptionsPopups
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsPopups) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#popups";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#location
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsLocation) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#location";
  RunBidiCheckerOnPage(url);
}

// TestSettings tests are flaky http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsFrameContentExceptionsLocation) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#location";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#notifications
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsNotifications) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#notifications";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsNotifications DISABLED_TestSettingsFrameContentExceptionsNotifications
#else
#define MAYBE_TestSettingsFrameContentExceptionsNotifications TestSettingsFrameContentExceptionsNotifications
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsNotifications) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#notifications";
  RunBidiCheckerOnPage(url);
}

//===================================================
// chrome://settings-frame/contentExceptions#mouselock
//===================================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameContentExceptionsMouseLock) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#mouselock";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_LINUX)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameContentExceptionsMouseLock DISABLED_TestSettingsFrameContentExceptionsMouseLock
#else
#define MAYBE_TestSettingsFrameContentExceptionsMouseLock TestSettingsFrameContentExceptionsMouseLock
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameContentExceptionsMouseLock) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kContentSettingsExceptionsSubPage;
  url += "#mouselock";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/handlers
//========================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameHandler) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kHandlerSettingsSubPage;
  RunBidiCheckerOnPage(url);
}

// Fails on chromeos. http://crbug.com/125367
#if defined(OS_CHROMEOS)
#define MAYBE_TestSettingsFrameHandler DISABLED_TestSettingsFrameHandler
#elif defined(OS_LINUX)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameHandler DISABLED_TestSettingsFrameHandler
#else
#define MAYBE_TestSettingsFrameHandler TestSettingsFrameHandler
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameHandler) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += chrome::kHandlerSettingsSubPage;
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/cookies
//========================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "cookies";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_LINUX)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameCookies DISABLED_TestSettingsFrameCookies
#else
#define MAYBE_TestSettingsFrameCookies TestSettingsFrameCookies
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameCookies) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "cookies";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/passwords
//========================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFramePasswords) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "passwords";
  RunBidiCheckerOnPage(url);
}

// TestSettingsFramePasswords test is flaky http://crbug.com/95425
IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       DISABLED_TestSettingsFramePasswords) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "passwords";
  RunBidiCheckerOnPage(url);
}

//========================================
// chrome://settings-frame/fonts
//========================================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR,
                       TestSettingsFrameFonts) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "fonts";
  RunBidiCheckerOnPage(url);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestSettingsFrameFonts DISABLED_TestSettingsFrameFonts
#else
#define MAYBE_TestSettingsFrameFonts TestSettingsFrameFonts
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       MAYBE_TestSettingsFrameFonts) {
  std::string url(chrome::kChromeUISettingsFrameURL);
  url += "fonts";
  RunBidiCheckerOnPage(url);
}

// Test other uber iframes.

//==============================
// chrome://extensions-frame
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestExtensionsFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIExtensionsFrameURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL,
                       TestExtensionsFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIExtensionsFrameURL);
}

//==============================
// chrome://help-frame
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestHelpFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHelpFrameURL);
}

#if defined(OS_WIN)
// TestSettings tests are flaky http://crbug.com/95425
#define MAYBE_TestHelpFrame DISABLED_TestHelpFrame
#else
#define MAYBE_TestHelpFrame TestHelpFrame
#endif

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, MAYBE_TestHelpFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHelpFrameURL);
}

//==============================
// chrome://history-frame
//==============================

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestLTR, TestHistoryFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTestRTL, TestHistoryFrame) {
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryFrameURL);
}
