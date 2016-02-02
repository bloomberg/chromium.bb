// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "url/gurl.h"

#if !defined(USE_AURA)
#include "components/translate/core/browser/translate_infobar_delegate.h"
#endif

// An observer that keeps track of whether a navigation entry was committed.
class NavEntryCommittedObserver : public content::NotificationObserver {
 public:
  explicit NavEntryCommittedObserver(content::WebContents* web_contents) {
    registrar_.Add(this,
                   content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::Source<content::NavigationController>(
                       &web_contents->GetController()));
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(type == content::NOTIFICATION_NAV_ENTRY_COMMITTED);
    details_ =
        *(content::Details<content::LoadCommittedDetails>(details).ptr());
  }

  const content::LoadCommittedDetails& load_committed_details() const {
    return details_;
  }

 private:
  content::LoadCommittedDetails details_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NavEntryCommittedObserver);
};

class TranslateManagerRenderViewHostTest
    : public ChromeRenderViewHostTestHarness,
      public content::NotificationObserver {
 public:
  TranslateManagerRenderViewHostTest()
      : pref_callback_(
            base::Bind(&TranslateManagerRenderViewHostTest::OnPreferenceChanged,
                       base::Unretained(this))) {}

  // Simulates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url,
                          const std::string& lang,
                          bool page_translatable) {
    if (main_rfh()->GetLastCommittedURL() == url)
      Reload();
    else
      NavigateAndCommit(url);
    SimulateOnTranslateLanguageDetermined(lang, page_translatable);
  }

  void SimulateOnTranslateLanguageDetermined(const std::string& lang,
                                             bool page_translatable) {
    translate::LanguageDetectionDetails details;
    details.adopted_language = lang;
    main_rfh()->OnMessageReceived(
        ChromeFrameHostMsg_TranslateLanguageDetermined(0, details,
                                                       page_translatable));
  }

  void SimulateOnPageTranslated(int routing_id,
                                const std::string& source_lang,
                                const std::string& target_lang,
                                translate::TranslateErrors::Type error) {
    main_rfh()->OnMessageReceived(ChromeFrameHostMsg_PageTranslated(
        routing_id, source_lang, target_lang, error));
  }

  void SimulateOnPageTranslated(const std::string& source_lang,
                                const std::string& target_lang) {
    SimulateOnPageTranslated(
        0, source_lang, target_lang, translate::TranslateErrors::NONE);
  }

  bool GetTranslateMessage(std::string* original_lang,
                           std::string* target_lang) {
    const IPC::Message* message = process()->sink().GetFirstMessageMatching(
        ChromeFrameMsg_TranslatePage::ID);
    if (!message)
      return false;
    base::Tuple<int, std::string, std::string, std::string> translate_param;
    ChromeFrameMsg_TranslatePage::Read(message, &translate_param);
    // Ignore get<0>(translate_param) which is the page seq no.
    // Ignore get<1>(translate_param) which is the script injected in the page.
    if (original_lang)
      *original_lang = base::get<2>(translate_param);
    if (target_lang)
      *target_lang = base::get<3>(translate_param);
    return true;
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

#if !defined(USE_AURA)
  // Returns the translate infobar if there is 1 infobar and it is a translate
  // infobar.
  translate::TranslateInfoBarDelegate* GetTranslateInfoBar() {
    return (infobar_service()->infobar_count() == 1) ?
        infobar_service()->infobar_at(0)->delegate()->
            AsTranslateInfoBarDelegate() :
        NULL;
  }

  // If there is 1 infobar and it is a translate infobar, closes it and returns
  // true.  Returns false otherwise.
  bool CloseTranslateInfoBar() {
    infobars::InfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->InfoBarDismissed();  // Simulates closing the infobar.
    infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
    return true;
  }

  // Checks whether |infobar| has been removed and clears the removed infobar
  // list.
  bool CheckInfoBarRemovedAndReset(infobars::InfoBarDelegate* delegate) {
    bool found = removed_infobars_.count(delegate) != 0;
    removed_infobars_.clear();
    return found;
  }

  void ExpireTranslateScriptImmediately() {
    translate::TranslateDownloadManager::GetInstance()
        ->SetTranslateScriptExpirationDelay(0);
  }

  // If there is 1 infobar and it is a translate infobar, deny translation and
  // returns true.  Returns false otherwise.
  bool DenyTranslation() {
    translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->TranslationDeclined();
    infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
    return true;
  }
#endif  // defined(USE_AURA)

  void ReloadAndWait(bool successful_reload) {
    NavEntryCommittedObserver nav_observer(web_contents());
    if (successful_reload)
      Reload();
    else
      FailedReload();

    // Ensures it is really handled a reload.
    const content::LoadCommittedDetails& nav_details =
        nav_observer.load_committed_details();
    EXPECT_TRUE(nav_details.entry != NULL);  // There was a navigation.
    EXPECT_EQ(content::NAVIGATION_TYPE_EXISTING_PAGE, nav_details.type);

    // The TranslateManager class processes the navigation entry committed
    // notification in a posted task; process that task.
    base::MessageLoop::current()->RunUntilIdle();
  }

  TestRenderViewContextMenu* CreateContextMenu() {
    content::ContextMenuParams params;
    params.media_type = blink::WebContextMenuData::MediaTypeNone;
    params.x = 0;
    params.y = 0;
    params.has_image_contents = true;
    params.media_flags = 0;
    params.spellcheck_enabled = false;
    params.is_editable = false;
    params.page_url =
        web_contents()->GetController().GetActiveEntry()->GetURL();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    params.edit_flags = blink::WebContextMenuData::CanTranslate;
    return new TestRenderViewContextMenu(web_contents()->GetMainFrame(),
                                         params);
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
    removed_infobars_.insert(
        content::Details<infobars::InfoBar::RemovedDetails>(
            details)->first->delegate());
  }

  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));

 protected:
  virtual void SetUp() {
    TranslateService::InitializeForTesting();

    // Clears the translate script so it is fetched everytime and sets the
    // expiration delay to a large value by default (in case it was zeroed in a
    // previous test).
    translate::TranslateDownloadManager* download_manager =
        translate::TranslateDownloadManager::GetInstance();
    download_manager->ClearTranslateScriptForTesting();
    download_manager->SetTranslateScriptExpirationDelay(60 * 60 * 1000);

    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
    ChromeTranslateClient::CreateForWebContents(web_contents());
    ChromeTranslateClient::FromWebContents(web_contents())
        ->translate_driver()
        .set_translate_max_reload_attempts(0);

    notification_registrar_.Add(
        this,
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::Source<InfoBarService>(infobar_service()));
  }

  virtual void TearDown() {
    process()->sink().ClearMessages();

    notification_registrar_.Remove(
        this,
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::Source<InfoBarService>(infobar_service()));

    ChromeRenderViewHostTestHarness::TearDown();
    TranslateService::ShutdownForTesting();
  }

  void SetApplicationLocale(const std::string& locale) {
    g_browser_process->SetApplicationLocale(locale);
    translate::TranslateDownloadManager::GetInstance()->set_application_locale(
        g_browser_process->GetApplicationLocale());
  }

  void SimulateTranslateScriptURLFetch(bool success) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
        translate::TranslateScript::kFetcherId);
    ASSERT_TRUE(fetcher);
    net::Error error = success ? net::OK : net::ERR_FAILED;
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_status(net::URLRequestStatus::FromError(error));
    fetcher->set_response_code(success ? 200 : 500);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateSupportedLanguagesURLFetch(
      bool success,
      const std::vector<std::string>& languages,
      bool use_alpha_languages,
      const std::vector<std::string>& alpha_languages) {
    net::Error error = success ? net::OK : net::ERR_FAILED;

    std::string data;
    if (success) {
      data = base::StringPrintf(
          "{\"sl\": {\"bla\": \"bla\"}, \"%s\": {",
          translate::TranslateLanguageList::kTargetLanguagesKey);
      const char* comma = "";
      for (size_t i = 0; i < languages.size(); ++i) {
        data += base::StringPrintf(
            "%s\"%s\": \"UnusedFullName\"", comma, languages[i].c_str());
        if (i == 0)
          comma = ",";
      }

      if (use_alpha_languages) {
        data += base::StringPrintf(
            "},\"%s\": {",
            translate::TranslateLanguageList::kAlphaLanguagesKey);
        comma = "";
        for (size_t i = 0; i < alpha_languages.size(); ++i) {
          data += base::StringPrintf(
              "%s\"%s\": 1", comma, alpha_languages[i].c_str());
          if (i == 0)
            comma = ",";
        }
      }

      data += "}}";
    }
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
        translate::TranslateLanguageList::kFetcherId);
    ASSERT_TRUE(fetcher != NULL);
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_status(net::URLRequestStatus::FromError(error));
    fetcher->set_response_code(success ? 200 : 500);
    fetcher->SetResponseString(data);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SetPrefObserverExpectation(const char* path) {
    EXPECT_CALL(*this, OnPreferenceChanged(std::string(path)));
  }

  PrefChangeRegistrar::NamedChangeCallback pref_callback_;

 private:
  content::NotificationRegistrar notification_registrar_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  // The infobars that have been removed.
  // WARNING: the pointers point to deleted objects, use only for comparison.
  std::set<infobars::InfoBarDelegate*> removed_infobars_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManagerRenderViewHostTest);
};

class MockTranslateBubbleFactory : public TranslateBubbleFactory {
 public:
  MockTranslateBubbleFactory() {}

  void ShowImplementation(
      BrowserWindow* window,
      content::WebContents* web_contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type) override {
    if (model_) {
      model_->SetViewState(
          TranslateBubbleModelImpl::TranslateStepToViewState(step));
      return;
    }

    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents);
    std::string source_language =
        chrome_translate_client->GetLanguageState().original_language();
    std::string target_language =
        translate::TranslateDownloadManager::GetLanguageCode(
            g_browser_process->GetApplicationLocale());

    scoped_ptr<translate::TranslateUIDelegate> ui_delegate(
        new translate::TranslateUIDelegate(
            chrome_translate_client->GetTranslateManager()->GetWeakPtr(),
            source_language,
            target_language));
    model_.reset(new TranslateBubbleModelImpl(step, std::move(ui_delegate)));
  }

  TranslateBubbleModel* model() { return model_.get(); }

 private:
  scoped_ptr<TranslateBubbleModel> model_;

  DISALLOW_COPY_AND_ASSIGN(MockTranslateBubbleFactory);
};

// A list of languages to fake being returned by the translate server.
// Use only langauges for which Chrome's copy of ICU has
// display names in English locale. To save space, Chrome's copy of ICU
// does not have the display name for a language unless it's in the
// Accept-Language list.
static const char* server_language_list[] =
    {"ach", "ak", "af", "en-CA", "zh", "yi", "fr-FR", "tl", "iw", "in", "xx"};
static const char* alpha_language_list[] = {"ach", "yi"};

// Test the fetching of languages from the translate server
TEST_F(TranslateManagerRenderViewHostTest, FetchLanguagesFromTranslateServer) {
  std::vector<std::string> server_languages;
  for (size_t i = 0; i < arraysize(server_language_list); ++i)
    server_languages.push_back(server_language_list[i]);

  std::vector<std::string> alpha_languages;
  for (size_t i = 0; i < arraysize(alpha_language_list); ++i)
    alpha_languages.push_back(alpha_language_list[i]);

  // First, get the default languages list. Note that calling
  // GetSupportedLanguages() invokes RequestLanguageList() internally.
  std::vector<std::string> default_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &default_supported_languages);
  // To make sure we got the defaults and don't confuse them with the mocks.
  ASSERT_NE(default_supported_languages.size(), server_languages.size());

  // Check that we still get the defaults until the URLFetch has completed.
  std::vector<std::string> current_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &current_supported_languages);
  EXPECT_EQ(default_supported_languages, current_supported_languages);

  // Also check that it didn't change if we failed the URL fetch.
  SimulateSupportedLanguagesURLFetch(
      false, std::vector<std::string>(), true, std::vector<std::string>());
  current_supported_languages.clear();
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &current_supported_languages);
  EXPECT_EQ(default_supported_languages, current_supported_languages);

  // Now check that we got the appropriate set of languages from the server.
  SimulateSupportedLanguagesURLFetch(
      true, server_languages, true, alpha_languages);
  current_supported_languages.clear();
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &current_supported_languages);
  // "xx" can't be displayed in the Translate infobar, so this is eliminated.
  EXPECT_EQ(server_languages.size() - 1, current_supported_languages.size());
  // Not sure we need to guarantee the order of languages, so we find them.
  for (size_t i = 0; i < server_languages.size(); ++i) {
    const std::string& lang = server_languages[i];
    if (lang == "xx")
      continue;
    EXPECT_NE(current_supported_languages.end(),
              std::find(current_supported_languages.begin(),
                        current_supported_languages.end(),
                        lang)) << "lang=" << lang;
    bool is_alpha =
        std::find(alpha_languages.begin(), alpha_languages.end(), lang) !=
        alpha_languages.end();
    EXPECT_EQ(translate::TranslateDownloadManager::IsAlphaLanguage(lang),
              is_alpha)
        << "lang=" << lang;
  }
}

// Test the fetching of languages from the translate server without 'al'
// parameter.
TEST_F(TranslateManagerRenderViewHostTest,
       FetchLanguagesFromTranslateServerWithoutAlpha) {
  std::vector<std::string> server_languages;
  for (size_t i = 0; i < arraysize(server_language_list); ++i)
    server_languages.push_back(server_language_list[i]);

  std::vector<std::string> alpha_languages;
  for (size_t i = 0; i < arraysize(alpha_language_list); ++i)
    alpha_languages.push_back(alpha_language_list[i]);

  // call GetSupportedLanguages to call RequestLanguageList internally.
  std::vector<std::string> default_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &default_supported_languages);

  SimulateSupportedLanguagesURLFetch(
      true, server_languages, false, alpha_languages);

  std::vector<std::string> current_supported_languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      &current_supported_languages);

  // "xx" can't be displayed in the Translate infobar, so this is eliminated.
  EXPECT_EQ(server_languages.size() - 1, current_supported_languages.size());

  for (size_t i = 0; i < server_languages.size(); ++i) {
    const std::string& lang = server_languages[i];
    if (lang == "xx")
      continue;
    EXPECT_NE(current_supported_languages.end(),
              std::find(current_supported_languages.begin(),
                        current_supported_languages.end(),
                        lang)) << "lang=" << lang;
    EXPECT_FALSE(translate::TranslateDownloadManager::IsAlphaLanguage(lang))
        << "lang=" << lang;
  }
}

// The rest of the tests in this file depend on the translate infobar. They
// should be ported to use the translate bubble. On Aura there is no infobar
// so the tests are not compiled.
#if !defined(USE_AURA)
TEST_F(TranslateManagerRenderViewHostTest, NormalTranslate) {
  // See BubbleNormalTranslate for corresponding bubble UX testing.
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // We should have an infobar.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            infobar->translate_step());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();

  // The "Translating..." infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATING, infobar->translate_step());

  // Simulate the translate script being retrieved (it only needs to be done
  // once in the test as it is cached).
  SimulateTranslateScriptURLFetch(true);

  // Test that we sent the right message to the renderer.
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // Simulate the render notifying the translation has been done.
  SimulateOnPageTranslated("fr", "en");

  // The after translate infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
            infobar->translate_step());

  // Simulate changing the original language and translating.
  process()->sink().ClearMessages();
  std::string new_original_lang = infobar->language_code_at(0);
  infobar->UpdateOriginalLanguage(new_original_lang);
  infobar->Translate();
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ(new_original_lang, original_lang);
  EXPECT_EQ("en", target_lang);
  // Simulate the render notifying the translation has been done.
  SimulateOnPageTranslated(new_original_lang, "en");
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);

  // Simulate changing the target language and translating.
  process()->sink().ClearMessages();
  std::string new_target_lang = infobar->language_code_at(1);
  infobar->UpdateTargetLanguage(new_target_lang);
  infobar->Translate();
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ(new_original_lang, original_lang);
  EXPECT_EQ(new_target_lang, target_lang);
  // Simulate the render notifying the translation has been done.
  SimulateOnPageTranslated(new_original_lang, new_target_lang);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(new_target_lang, infobar->target_language_code());

  // Reloading should trigger translation iff Always Translate is on.
  ReloadAndWait(true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            infobar->translate_step());
  infobar->UpdateTargetLanguage(new_target_lang);
  infobar->ToggleAlwaysTranslate();
  ReloadAndWait(true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATING, infobar->translate_step());
  EXPECT_EQ(new_target_lang, infobar->target_language_code());
}

TEST_F(TranslateManagerRenderViewHostTest, TranslateScriptNotAvailable) {
  // See BubbleTranslateScriptNotAvailable for corresponding bubble UX testing.
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // We should have an infobar.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            infobar->translate_step());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();
  SimulateTranslateScriptURLFetch(false);

  // We should not have sent any message to translate to the renderer.
  EXPECT_FALSE(GetTranslateMessage(NULL, NULL));

  // And we should have an error infobar showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR,
            infobar->translate_step());
}

// Ensures we deal correctly with pages for which the browser does not recognize
// the language (the translate server may or not detect the language).
TEST_F(TranslateManagerRenderViewHostTest, TranslateUnknownLanguage) {
  // See BubbleUnknownLanguage for corresponding bubble UX testing.
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Simulate navigating to a page ("und" is the string returned by the CLD for
  // languages it does not recognize).
  SimulateNavigation(GURL("http://www.google.mys"), "und", true);

  // We should not have an infobar as we don't know the language.
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);

  // Translate the page anyway throught the context menu.
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);

  // To test that bug #49018 if fixed, make sure we deal correctly with errors.
  // Simulate a failure to fetch the translate script.
  SimulateTranslateScriptURLFetch(false);
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR,
            infobar->translate_step());
  EXPECT_TRUE(infobar->is_error());
  infobar->MessageInfoBarButtonPressed();
  SimulateTranslateScriptURLFetch(true);  // This time succeed.

  // Simulate the render notifying the translation has been done, the server
  // having detected the page was in a known and supported language.
  SimulateOnPageTranslated("fr", "en");

  // The after translate infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
            infobar->translate_step());
  EXPECT_EQ("fr", infobar->original_language_code());
  EXPECT_EQ("en", infobar->target_language_code());

  // Let's run the same steps but this time the server detects the page is
  // already in English.
  SimulateNavigation(GURL("http://www.google.com"), "und", true);
  menu.reset(CreateContextMenu());
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);
  SimulateOnPageTranslated(
      1, "en", "en", translate::TranslateErrors::IDENTICAL_LANGUAGES);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR,
            infobar->translate_step());
  EXPECT_EQ(translate::TranslateErrors::IDENTICAL_LANGUAGES,
            infobar->error_type());

  // Let's run the same steps again but this time the server fails to detect the
  // page's language (it returns an empty string).
  SimulateNavigation(GURL("http://www.google.com"), "und", true);
  menu.reset(CreateContextMenu());
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);
  SimulateOnPageTranslated(
      2, std::string(), "en", translate::TranslateErrors::UNKNOWN_LANGUAGE);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR,
            infobar->translate_step());
  EXPECT_EQ(translate::TranslateErrors::UNKNOWN_LANGUAGE,
            infobar->error_type());
}

// Tests that we show/don't show an info-bar for the languages.
TEST_F(TranslateManagerRenderViewHostTest, TestLanguages) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  std::vector<std::string> languages;
  languages.push_back("en");
  languages.push_back("ja");
  languages.push_back("fr");
  languages.push_back("ht");
  languages.push_back("xx");
  languages.push_back("zh");
  languages.push_back("zh-CN");
  languages.push_back("und");

  GURL url("http://www.google.com");
  for (size_t i = 0; i < languages.size(); ++i) {
    std::string lang = languages[i];
    SCOPED_TRACE(::testing::Message() << "Iteration " << i
                                      << " language=" << lang);

    // We should not have a translate infobar.
    translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar == NULL);

    SimulateNavigation(url, lang, true);

    // Verify we have/don't have an info-bar as expected.
    infobar = GetTranslateInfoBar();
    bool expected =
        translate::TranslateDownloadManager::IsSupportedLanguage(lang) &&
        lang != "en";
    EXPECT_EQ(expected, infobar != NULL);

    if (infobar != NULL)
      EXPECT_TRUE(CloseTranslateInfoBar());
  }
}

// Tests auto-translate on page.
TEST_F(TranslateManagerRenderViewHostTest, AutoTranslateOnNavigate) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  // Simulate the translate script being retrieved.
  SimulateTranslateScriptURLFetch(true);
  SimulateOnPageTranslated("fr", "en");

  // Now navigate to a new page in the same language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.fr"), "fr", true);

  // This should have automatically triggered a translation.
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // Now navigate to a page in a different language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.es"), "es", true);

  // This should not have triggered a translate.
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));
}

// Tests that multiple OnPageContents do not cause multiple infobars.
TEST_F(TranslateManagerRenderViewHostTest, MultipleOnPageContents) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate clicking 'Nope' (don't translate).
  EXPECT_TRUE(DenyTranslation());
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  // Send a new PageContents, we should not show an infobar.
  SimulateOnTranslateLanguageDetermined("fr", true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  // Do the same steps but simulate closing the infobar this time.
  SimulateNavigation(GURL("http://www.youtube.fr"), "fr", true);
  EXPECT_TRUE(CloseTranslateInfoBar());
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  SimulateOnTranslateLanguageDetermined("fr", true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}

// Test that reloading the page brings back the infobar if the
// reload succeeded and does not bring it back the reload fails.
TEST_F(TranslateManagerRenderViewHostTest, Reload) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Reload should bring back the infobar if the reload succeeds.
  ReloadAndWait(true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());

  // ...But not show it if the reload fails.
  ReloadAndWait(false);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // If we set reload attempts to a high value, we will not see the infobar
  // immediately.
  ChromeTranslateClient::FromWebContents(web_contents())
      ->translate_driver()
      .set_translate_max_reload_attempts(100);
  ReloadAndWait(true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);
}

// Test that reloading the page by way of typing again the URL in the
// location bar brings back the infobar.
TEST_F(TranslateManagerRenderViewHostTest, ReloadFromLocationBar) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  GURL url("http://www.google.fr");
  SimulateNavigation(url, "fr", true);

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Create a pending navigation and simulate a page load.  That should be the
  // equivalent of typing the URL again in the location bar.
  NavEntryCommittedObserver nav_observer(web_contents());
  web_contents()->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int pending_id =
      web_contents()->GetController().GetPendingEntry()->GetUniqueID();
  content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
      ->SendNavigateWithTransition(0, pending_id, false, url,
                                   ui::PAGE_TRANSITION_TYPED);

  // Test that we are really getting a same page navigation, the test would be
  // useless if it was not the case.
  const content::LoadCommittedDetails& nav_details =
      nav_observer.load_committed_details();
  EXPECT_TRUE(nav_details.entry != NULL);  // There was a navigation.
  EXPECT_EQ(content::NAVIGATION_TYPE_SAME_PAGE, nav_details.type);

  // The TranslateManager class processes the navigation entry committed
  // notification in a posted task; process that task.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that a closed translate infobar does not reappear when navigating
// in-page.
TEST_F(TranslateManagerRenderViewHostTest, CloseInfoBarInPageNavigation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that a closed translate infobar does not reappear when navigating
// in a subframe. (http://crbug.com/48215)
TEST_F(TranslateManagerRenderViewHostTest, CloseInfoBarInSubframeNavigation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  EXPECT_TRUE(CloseTranslateInfoBar());

  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  // Simulate a sub-frame auto-navigating.
  subframe_tester->SendNavigateWithTransition(
      0, 0, false, GURL("http://pub.com"), ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Simulate the user navigating in a sub-frame.
  subframe_tester->SendNavigateWithTransition(
      1, 0, true, GURL("http://pub.com"), ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that denying translation is sticky when navigating in page.
TEST_F(TranslateManagerRenderViewHostTest, DenyTranslateInPageNavigation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate clicking 'Nope' (don't translate).
  EXPECT_TRUE(DenyTranslation());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that after translating and closing the infobar, the infobar does not
// return when navigating in page.
TEST_F(TranslateManagerRenderViewHostTest,
       TranslateCloseInfoBarInPageNavigation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  // Simulate the translate script being retrieved.
  SimulateTranslateScriptURLFetch(true);
  SimulateOnPageTranslated("fr", "en");

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  // Note that we navigate to a page in a different language so we don't trigger
  // the auto-translate feature (it would translate the page automatically and
  // the before translate infobar would not be shown).
  SimulateNavigation(GURL("http://www.google.de"), "de", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that the after translate the infobar still shows when navigating
// in-page.
TEST_F(TranslateManagerRenderViewHostTest, TranslateInPageNavigation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  SimulateTranslateScriptURLFetch(true);
  SimulateOnPageTranslated("fr", "en");
  // The after translate infobar is showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);

  // Navigate out of page, a new infobar should show.
  // See note in TranslateCloseInfoBarInPageNavigation test on why it is
  // important to navigate to a page in a different language for this test.
  SimulateNavigation(GURL("http://www.google.de"), "de", true);
  // The old infobar is gone.
  EXPECT_TRUE(CheckInfoBarRemovedAndReset(infobar));
  // And there is a new one.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that no translate infobar is shown when navigating to a page in an
// unsupported language.
TEST_F(TranslateManagerRenderViewHostTest, CLDReportsUnsupportedPageLanguage) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Simulate navigating to a page and getting an unsupported language.
  SimulateNavigation(GURL("http://www.google.com"), "qbz", true);

  // No info-bar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that we deal correctly with unsupported languages returned by the
// server.
// The translation server might return a language we don't support.
TEST_F(TranslateManagerRenderViewHostTest, ServerReportsUnsupportedLanguage) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://mail.google.fr"), "fr", true);
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  SimulateTranslateScriptURLFetch(true);
  // Simulate the render notifying the translation has been done, but it
  // reports a language we don't support.
  SimulateOnPageTranslated("qbz", "en");

  // An error infobar should be showing to report that we don't support this
  // language.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR,
            infobar->translate_step());

  // This infobar should have a button (so the string should not be empty).
  ASSERT_FALSE(infobar->GetMessageInfoBarButtonText().empty());

  // Pressing the button on that infobar should revert to the original language.
  process()->sink().ClearMessages();
  infobar->MessageInfoBarButtonPressed();
  const IPC::Message* message = process()->sink().GetFirstMessageMatching(
      ChromeFrameMsg_RevertTranslation::ID);
  EXPECT_TRUE(message != NULL);
  // And it should have removed the infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that no translate infobar is shown and context menu is disabled, when
// Chrome is in a language that the translate server does not support.
TEST_F(TranslateManagerRenderViewHostTest, UnsupportedUILanguage) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  std::string original_lang = g_browser_process->GetApplicationLocale();
  SetApplicationLocale("qbz");

  // Make sure that the accept language list only contains unsupported languages
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kAcceptLanguages, "qbz");

  // Simulate navigating to a page in a language supported by the translate
  // server.
  SimulateNavigation(GURL("http://www.google.com"), "en", true);

  // No info-bar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // And the context menu option should be disabled too.
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  SetApplicationLocale(original_lang);
}

// Tests that the first supported accept language is selected
TEST_F(TranslateManagerRenderViewHostTest, TranslateAcceptLanguage) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Set locate to non-existant language
  std::string original_lang = g_browser_process->GetApplicationLocale();
  SetApplicationLocale("qbz");

  // Set Qbz and French as the only accepted languages
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kAcceptLanguages, "qbz,fr");

  // Go to a German page
  SimulateNavigation(GURL("http://google.de"), "de", true);

  // Expect the infobar to pop up
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Set Qbz and English-US as the only accepted languages to test the country
  // code removal code which was causing a crash as filed in Issue 90106,
  // a crash caused by a language with a country code that wasn't recognized.
  prefs->SetString(prefs::kAcceptLanguages, "qbz,en-us");

  // Go to a German page
  SimulateNavigation(GURL("http://google.de"), "de", true);

  // Expect the infobar to pop up
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that the translate enabled preference is honored.
TEST_F(TranslateManagerRenderViewHostTest, TranslateEnabledPref) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Make sure the pref allows translate.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kEnableTranslate, true);

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // An infobar should be shown.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  EXPECT_TRUE(infobar != NULL);

  // Disable translate.
  prefs->SetBoolean(prefs::kEnableTranslate, false);

  // Navigate to a new page, that should close the previous infobar.
  GURL url("http://www.youtube.fr");
  NavigateAndCommit(url);
  infobar = GetTranslateInfoBar();
  EXPECT_TRUE(infobar == NULL);

  // Simulate getting the page contents and language, that should not trigger
  // a translate infobar.
  SimulateOnTranslateLanguageDetermined("fr", true);
  infobar = GetTranslateInfoBar();
  EXPECT_TRUE(infobar == NULL);
}

// Tests the "Never translate <language>" pref.
TEST_F(TranslateManagerRenderViewHostTest, NeverTranslateLanguagePref) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  GURL url("http://www.google.fr");
  SimulateNavigation(url, "fr", true);

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this language.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(translate::TranslatePrefs::kPrefTranslateBlockedLanguages,
                pref_callback_);
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(prefs));
  EXPECT_FALSE(translate_prefs->IsBlockedLanguage("fr"));
  translate::TranslateAcceptLanguages* accept_languages =
      ChromeTranslateClient::GetTranslateAcceptLanguages(profile);
  EXPECT_TRUE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateBlockedLanguages);
  translate_prefs->BlockLanguage("fr");
  EXPECT_TRUE(translate_prefs->IsBlockedLanguage("fr"));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(url.host()));
  EXPECT_FALSE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also in French.
  SimulateNavigation(GURL("http://wwww.youtube.fr"), "fr", true);

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the language from the blacklist.
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateBlockedLanguages);
  translate_prefs->UnblockLanguage("fr");
  EXPECT_FALSE(translate_prefs->IsBlockedLanguage("fr"));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(url.host()));
  EXPECT_TRUE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));

  // Navigate to a page in French.
  SimulateNavigation(url, "fr", true);

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Never translate this site" pref.
TEST_F(TranslateManagerRenderViewHostTest, NeverTranslateSitePref) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  GURL url("http://www.google.fr");
  std::string host(url.host());
  SimulateNavigation(url, "fr", true);

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this site.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(translate::TranslatePrefs::kPrefTranslateSiteBlacklist,
                pref_callback_);
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(prefs));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(host));
  translate::TranslateAcceptLanguages* accept_languages =
      ChromeTranslateClient::GetTranslateAcceptLanguages(profile);
  EXPECT_TRUE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateSiteBlacklist);
  translate_prefs->BlacklistSite(host);
  EXPECT_TRUE(translate_prefs->IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));

  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also on the same site.
  SimulateNavigation(GURL("http://www.google.fr/hello"), "fr", true);

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the site from the blacklist.
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateSiteBlacklist);
  translate_prefs->RemoveSiteFromBlacklist(host);
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs->CanTranslateLanguage(accept_languages, "fr"));

  // Navigate to a page in French.
  SimulateNavigation(url, "fr", true);

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Always translate this language" pref.
TEST_F(TranslateManagerRenderViewHostTest, AlwaysTranslateLanguagePref) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Select always translate French to English.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(translate::TranslatePrefs::kPrefTranslateWhitelists,
                pref_callback_);
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(prefs));
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateWhitelists);
  translate_prefs->WhitelistLanguagePair("fr", "en");

  // Load a page in French.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // It should have triggered an automatic translation to English.

  // The translating infobar should be showing.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATING, infobar->translate_step());
  SimulateTranslateScriptURLFetch(true);
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);
  process()->sink().ClearMessages();

  // Try another language, it should not be autotranslated.
  SimulateNavigation(GURL("http://www.google.es"), "es", true);
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Let's switch to incognito mode, it should not be autotranslated in that
  // case either.
  TestingProfile* test_profile =
      static_cast<TestingProfile*>(web_contents()->GetBrowserContext());
  test_profile->ForceIncognito(true);
  SimulateNavigation(GURL("http://www.youtube.fr"), "fr", true);
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());
  test_profile->ForceIncognito(false);  // Get back to non incognito.

  // Now revert the always translate pref and make sure we go back to expected
  // behavior, which is show a "before translate" infobar.
  SetPrefObserverExpectation(
      translate::TranslatePrefs::kPrefTranslateWhitelists);
  translate_prefs->RemoveLanguagePairFromWhitelist("fr", "en");
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            infobar->translate_step());
}

// Context menu.
TEST_F(TranslateManagerRenderViewHostTest, ContextMenu) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  // Blacklist www.google.fr and French for translation.
  GURL url("http://www.google.fr");
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(profile->GetPrefs()));
  translate_prefs->BlockLanguage("fr");
  translate_prefs->BlacklistSite(url.host());
  EXPECT_TRUE(translate_prefs->IsBlockedLanguage("fr"));
  EXPECT_TRUE(translate_prefs->IsSiteBlacklisted(url.host()));

  // Simulate navigating to a page in French. The translate menu should show but
  // should only be enabled when the page language has been received.
  NavigateAndCommit(url);
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Simulate receiving the language.
  SimulateOnTranslateLanguageDetermined("fr", true);
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Use the menu to translate the page.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);

  // That should have triggered a translation.
  // The "translating..." infobar should be showing.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(translate::TRANSLATE_STEP_TRANSLATING, infobar->translate_step());
  SimulateTranslateScriptURLFetch(true);
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);
  process()->sink().ClearMessages();

  // This should also have reverted the blacklisting of this site and language.
  EXPECT_FALSE(translate_prefs->IsBlockedLanguage("fr"));
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(url.host()));

  // Let's simulate the page being translated.
  SimulateOnPageTranslated("fr", "en");

  // The translate menu should now be disabled.
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Test that selecting translate in the context menu WHILE the page is being
  // translated does nothing (this could happen if autotranslate kicks-in and
  // the user selects the menu while the translation is being performed).
  SimulateNavigation(GURL("http://www.google.es"), "es", true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  process()->sink().ClearMessages();
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);
  // No message expected since the translation should have been ignored.
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));

  // Now test that selecting translate in the context menu AFTER the page has
  // been translated does nothing.
  SimulateNavigation(GURL("http://www.google.de"), "de", true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  process()->sink().ClearMessages();
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
  SimulateOnPageTranslated("de", "en");
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);
  // No message expected since the translation should have been ignored.
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));

  // Test that the translate context menu is enabled when the page is in an
  // unknown language.
  SimulateNavigation(url, "und", true);
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Test that the translate context menu is enabled even if the page is in an
  // unsupported language.
  SimulateNavigation(url, "qbz", true);
  menu.reset(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
}

// Tests that an extra always/never translate button is shown on the "before
// translate" infobar when the translation is accepted/declined 3 times,
// only when not in incognito mode.
TEST_F(TranslateManagerRenderViewHostTest, BeforeTranslateExtraButtons) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(profile->GetPrefs()));
  translate_prefs->ResetTranslationAcceptedCount("fr");
  translate_prefs->ResetTranslationDeniedCount("fr");
  translate_prefs->ResetTranslationAcceptedCount("de");
  translate_prefs->ResetTranslationDeniedCount("de");

  // We'll do 4 times in incognito mode first to make sure the button is not
  // shown in that case, then 4 times in normal mode.
  translate::TranslateInfoBarDelegate* infobar;
  TestingProfile* test_profile =
      static_cast<TestingProfile*>(web_contents()->GetBrowserContext());
  test_profile->ForceIncognito(true);
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i << " incognito mode="
                                      << test_profile->IsOffTheRecord());
    SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
    infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar != NULL);
    EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
              infobar->translate_step());
    if (i < 7) {
      EXPECT_FALSE(infobar->ShouldShowAlwaysTranslateShortcut());
      infobar->Translate();
      process()->sink().ClearMessages();
    } else {
      EXPECT_TRUE(infobar->ShouldShowAlwaysTranslateShortcut());
    }
    if (i == 3)
      test_profile->ForceIncognito(false);
  }
  // Simulate the user pressing "Always translate French".
  infobar->AlwaysTranslatePageLanguage();
  EXPECT_TRUE(translate_prefs->IsLanguagePairWhitelisted("fr", "en"));
  // Simulate the translate script being retrieved (it only needs to be done
  // once in the test as it is cached).
  SimulateTranslateScriptURLFetch(true);
  // That should have triggered a page translate.
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  process()->sink().ClearMessages();

  // Now test that declining the translation causes a "never translate" button
  // to be shown (in non incognito mode only).
  test_profile->ForceIncognito(true);
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i << " incognito mode="
                                      << test_profile->IsOffTheRecord());
    SimulateNavigation(GURL("http://www.google.de"), "de", true);
    infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar != NULL);
    EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
              infobar->translate_step());
    if (i < 7) {
      EXPECT_FALSE(infobar->ShouldShowNeverTranslateShortcut());
      infobar->TranslationDeclined();
    } else {
      EXPECT_TRUE(infobar->ShouldShowNeverTranslateShortcut());
    }
    if (i == 3)
      test_profile->ForceIncognito(false);
  }
  // Simulate the user pressing "Never translate French".
  infobar->NeverTranslatePageLanguage();
  EXPECT_TRUE(translate_prefs->IsBlockedLanguage("de"));
  // No translation should have occured and the infobar should be gone.
  EXPECT_FALSE(GetTranslateMessage(&original_lang, &target_lang));
  process()->sink().ClearMessages();
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that we don't show a translate infobar when a page instructs that it
// should not be translated.
TEST_F(TranslateManagerRenderViewHostTest, NonTranslatablePage) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  SimulateNavigation(GURL("http://mail.google.fr"), "fr", false);

  // We should not have an infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // The context menu is enabled to allow users to force translation.
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
}

// Tests that the script is expired and refetched as expected.
TEST_F(TranslateManagerRenderViewHostTest, ScriptExpires) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ExpireTranslateScriptImmediately();

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  SimulateTranslateScriptURLFetch(true);
  SimulateOnPageTranslated("fr", "en");

  // A task should have been posted to clear the script, run it.
  base::MessageLoop::current()->RunUntilIdle();

  // Do another navigation and translation.
  SimulateNavigation(GURL("http://www.google.es"), "es", true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  // If we don't simulate the URL fetch, the TranslateManager should be waiting
  // for the script and no message should have been sent to the renderer.
  EXPECT_TRUE(process()->sink().GetFirstMessageMatching(
                  ChromeFrameMsg_TranslatePage::ID) == NULL);
  // Now simulate the URL fetch.
  SimulateTranslateScriptURLFetch(true);
  // Now the message should have been sent.
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&original_lang, &target_lang));
  EXPECT_EQ("es", original_lang);
  EXPECT_EQ("en", target_lang);
}

TEST_F(TranslateManagerRenderViewHostTest, DownloadsAndHistoryNotTranslated) {
  ASSERT_FALSE(
      TranslateService::IsTranslatableURL(GURL(chrome::kChromeUIDownloadsURL)));
  ASSERT_FALSE(
      TranslateService::IsTranslatableURL(GURL(chrome::kChromeUIHistoryURL)));
}

TEST_F(TranslateManagerRenderViewHostTest, BubbleNormalTranslate) {
  // See NormalTranslate for corresponding infobar UX testing.
  if (!TranslateService::IsTranslateBubbleEnabled())
    return;

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  scoped_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Check the bubble exists instead of the infobar.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar == NULL);
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble->GetViewState());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  bubble->Translate();

  // Check the bubble shows "Translating...".
  bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble->GetViewState());

  // Simulate the translate script being retrieved (it only needs to be done
  // once in the test as it is cached).
  SimulateTranslateScriptURLFetch(true);

  // Simulate the render notifying the translation has been done.
  SimulateOnPageTranslated("fr", "en");

  // Check the bubble shows "Translated."
  bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble->GetViewState());
}

TEST_F(TranslateManagerRenderViewHostTest, BubbleTranslateScriptNotAvailable) {
  // See TranslateScriptNotAvailable for corresponding infobar UX testing.
  if (!TranslateService::IsTranslateBubbleEnabled())
    return;

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  scoped_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Check the bubble exists instead of the infobar.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar == NULL);
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble->GetViewState());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  bubble->Translate();
  SimulateTranslateScriptURLFetch(false);

  // We should not have sent any message to translate to the renderer.
  EXPECT_FALSE(GetTranslateMessage(NULL, NULL));

  // And we should have an error infobar showing.
  bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ERROR, bubble->GetViewState());
}

TEST_F(TranslateManagerRenderViewHostTest, BubbleUnknownLanguage) {
  // See TranslateUnknownLanguage for corresponding infobar UX testing.
  if (!TranslateService::IsTranslateBubbleEnabled())
    return;

  MockTranslateBubbleFactory* factory = new MockTranslateBubbleFactory;
  scoped_ptr<TranslateBubbleFactory> factory_ptr(factory);
  TranslateBubbleFactory::SetFactory(factory);

  // Simulate navigating to a page ("und" is the string returned by the CLD for
  // languages it does not recognize).
  SimulateNavigation(GURL("http://www.google.mys"), "und", true);

  // We should not have a bubble as we don't know the language.
  ASSERT_TRUE(factory->model() == NULL);

  // Translate the page anyway throught the context menu.
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenu());
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE, 0);

  // Check the bubble exists instead of the infobar.
  translate::TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar == NULL);
  TranslateBubbleModel* bubble = factory->model();
  ASSERT_TRUE(bubble != NULL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble->GetViewState());
}
#endif  // defined(USE_AURA)
