// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <set>

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"
#include "content/common/view_messages.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cld/languages/public/languages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"

using testing::_;
using testing::Pointee;
using testing::Property;
using WebKit::WebContextMenuData;

class TranslateManagerTest : public TabContentsWrapperTestHarness,
                             public NotificationObserver {
 public:
  TranslateManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  // Simluates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url,
                          const std::string& lang,
                          bool page_translatable) {
    NavigateAndCommit(url);
    SimulateOnTranslateLanguageDetermined(lang, page_translatable);
  }

  void SimulateOnTranslateLanguageDetermined(const std::string& lang,
                                             bool page_translatable) {
    rvh()->TestOnMessageReceived(ViewHostMsg_TranslateLanguageDetermined(
        0, lang, page_translatable));
  }

  bool GetTranslateMessage(int* page_id,
                           std::string* original_lang,
                           std::string* target_lang) {
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(ViewMsg_TranslatePage::ID);
    if (!message)
      return false;
    Tuple4<int, std::string, std::string, std::string> translate_param;
    ViewMsg_TranslatePage::Read(message, &translate_param);
    if (page_id)
      *page_id = translate_param.a;
    // Ignore translate_param.b which is the script injected in the page.
    if (original_lang)
      *original_lang = translate_param.c;
    if (target_lang)
      *target_lang = translate_param.d;
    return true;
  }

  // Returns the translate infobar if there is 1 infobar and it is a translate
  // infobar.
  TranslateInfoBarDelegate* GetTranslateInfoBar() {
    return (contents()->infobar_count() == 1) ?
        contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate() :
        NULL;
  }

  // If there is 1 infobar and it is a translate infobar, closes it and returns
  // true.  Returns false otherwise.
  bool CloseTranslateInfoBar() {
    InfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->InfoBarDismissed();  // Simulates closing the infobar.
    contents()->RemoveInfoBar(infobar);
    return true;
  }

  // Checks whether |infobar| has been removed and clears the removed infobar
  // list.
  bool CheckInfoBarRemovedAndReset(InfoBarDelegate* delegate) {
    bool found = removed_infobars_.count(delegate) != 0;
    removed_infobars_.clear();
    return found;
  }

  // Returns true if at least one infobar was closed.
  bool InfoBarRemoved() {
    return !removed_infobars_.empty();
  }

  // Clears the list of stored removed infobars.
  void ClearRemovedInfoBars() {
    removed_infobars_.clear();
  }

  void ExpireTranslateScriptImmediately() {
    TranslateManager::GetInstance()->set_translate_script_expiration_delay(0);
  }

  // If there is 1 infobar and it is a translate infobar, deny translation and
  // returns true.  Returns false otherwise.
  bool DenyTranslation() {
    TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->TranslationDeclined();
    contents()->RemoveInfoBar(infobar);
    return true;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK_EQ(NotificationType::TAB_CONTENTS_INFOBAR_REMOVED, type.value);
    removed_infobars_.insert(Details<InfoBarDelegate>(details).ptr());
  }

 protected:
  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);

    // Access the TranslateManager singleton so it is created before we call
    // RenderViewHostTestHarness::SetUp() to match what's done in Chrome, where
    // the TranslateManager is created before the TabContents.  This matters as
    // they both register for similar events and we want the notifications to
    // happen in the same sequence (TranslateManager first, TabContents second).
    // Also clears the translate script so it is fetched everytime and sets the
    // expiration delay to a large value by default (in case it was zeroed in
    // a previous test).
    TranslateManager::GetInstance()->ClearTranslateScript();
    TranslateManager::GetInstance()->
        set_translate_script_expiration_delay(60 * 60 * 1000);

    TabContentsWrapperTestHarness::SetUp();

    notification_registrar_.Add(this,
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(contents()));
  }

  virtual void TearDown() {
    process()->sink().ClearMessages();

    notification_registrar_.Remove(this,
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(contents()));

    TabContentsWrapperTestHarness::TearDown();

    URLFetcher::set_factory(NULL);
  }

  void SimulateURLFetch(bool success) {
    TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    net::URLRequestStatus status;
    status.set_status(success ? net::URLRequestStatus::SUCCESS :
                                net::URLRequestStatus::FAILED);
    fetcher->delegate()->OnURLFetchComplete(fetcher, fetcher->original_url(),
                                            status, success ? 200 : 500,
                                            ResponseCookies(),
                                            std::string());
  }

  void SetPrefObserverExpectation(const char* path) {
    EXPECT_CALL(
        pref_observer_,
        Observe(NotificationType(NotificationType::PREF_CHANGED),
                _,
                Property(&Details<std::string>::ptr, Pointee(path))));
  }

  NotificationObserverMock pref_observer_;

 private:
  NotificationRegistrar notification_registrar_;
  TestURLFetcherFactory url_fetcher_factory_;
  BrowserThread ui_thread_;

  // The infobars that have been removed.
  // WARNING: the pointers point to deleted objects, use only for comparison.
  std::set<InfoBarDelegate*> removed_infobars_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManagerTest);
};

// An observer that keeps track of whether a navigation entry was committed.
class NavEntryCommittedObserver : public NotificationObserver {
 public:
  explicit NavEntryCommittedObserver(TabContents* tab_contents) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(&tab_contents->controller()));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::NAV_ENTRY_COMMITTED);
    details_ =
        *(Details<NavigationController::LoadCommittedDetails>(details).ptr());
  }

  const NavigationController::LoadCommittedDetails&
      get_load_commited_details() const {
    return details_;
  }

 private:
  NavigationController::LoadCommittedDetails details_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NavEntryCommittedObserver);
};

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  static TestRenderViewContextMenu* CreateContextMenu(
      TabContents* tab_contents) {
    ContextMenuParams params;
    params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
    params.x = 0;
    params.y = 0;
    params.is_image_blocked = false;
    params.media_flags = 0;
    params.spellcheck_enabled = false;
    params.is_editable = false;
    params.page_url = tab_contents->controller().GetActiveEntry()->url();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    params.edit_flags = WebContextMenuData::CanTranslate;
    return new TestRenderViewContextMenu(tab_contents, params);
  }

  bool IsItemPresent(int id) {
    return menu_model_.GetIndexOfCommandId(id) != -1;
  }

  virtual void PlatformInit() { }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) { return false; }

 private:
  TestRenderViewContextMenu(TabContents* tab_contents,
                            const ContextMenuParams& params)
      : RenderViewContextMenu(tab_contents, params) {
  }

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

TEST_F(TranslateManagerTest, NormalTranslate) {
  // Simulate navigating to a page.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // We should have an infobar.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();

  // The "Translating..." infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATING, infobar->type());

  // Simulate the translate script being retrieved (it only needs to be done
  // once in the test as it is cached).
  SimulateURLFetch(true);

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // Simulate the render notifying the translation has been done.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // The after translate infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::AFTER_TRANSLATE, infobar->type());

  // Simulate changing the original language, this should trigger a translation.
  process()->sink().ClearMessages();
  std::string new_original_lang = infobar->GetLanguageCodeAt(0);
  infobar->SetOriginalLanguage(0);
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(new_original_lang, original_lang);
  EXPECT_EQ("en", target_lang);
  // Simulate the render notifying the translation has been done.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0,
      new_original_lang, "en", TranslateErrors::NONE));
  // infobar is now invalid.
  TranslateInfoBarDelegate* new_infobar = GetTranslateInfoBar();
  ASSERT_TRUE(new_infobar != NULL);
  infobar = new_infobar;

  // Simulate changing the target language, this should trigger a translation.
  process()->sink().ClearMessages();
  std::string new_target_lang = infobar->GetLanguageCodeAt(1);
  infobar->SetTargetLanguage(1);
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(new_original_lang, original_lang);
  EXPECT_EQ(new_target_lang, target_lang);
  // Simulate the render notifying the translation has been done.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0,
      new_original_lang, new_target_lang, TranslateErrors::NONE));
  // infobar is now invalid.
  new_infobar = GetTranslateInfoBar();
  ASSERT_TRUE(new_infobar != NULL);
}

TEST_F(TranslateManagerTest, TranslateScriptNotAvailable) {
  // Simulate navigating to a page.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // We should have an infobar.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();
  // Simulate a failure retrieving the translate script.
  SimulateURLFetch(false);

  // We should not have sent any message to translate to the renderer.
  EXPECT_FALSE(GetTranslateMessage(NULL, NULL, NULL));

  // And we should have an error infobar showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATION_ERROR, infobar->type());
}

// Ensures we deal correctly with pages for which the browser does not recognize
// the language (the translate server may or not detect the language).
TEST_F(TranslateManagerTest, TranslateUnknownLanguage) {
  // Simulate navigating to a page ("und" is the string returned by the CLD for
  // languages it does not recognize).
  SimulateNavigation(GURL("http://www.google.mys"), "und", true);

  // We should not have an infobar as we don't know the language.
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);

  // Translate the page anyway throught the context menu.
  scoped_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);

  // To test that bug #49018 if fixed, make sure we deal correctly with errors.
  SimulateURLFetch(false);  // Simulate a failure to fetch the translate script.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATION_ERROR, infobar->type());
  EXPECT_TRUE(infobar->IsError());
  infobar->MessageInfoBarButtonPressed();
  SimulateURLFetch(true);  // This time succeed.

  // Simulate the render notifying the translation has been done, the server
  // having detected the page was in a known and supported language.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // The after translate infobar should be showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::AFTER_TRANSLATE, infobar->type());
  EXPECT_EQ("fr", infobar->GetOriginalLanguageCode());
  EXPECT_EQ("en", infobar->GetTargetLanguageCode());

  // Let's run the same steps but this time the server detects the page is
  // already in English.
  SimulateNavigation(GURL("http://www.google.com"), "und", true);
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(1, 0, "en", "en",
      TranslateErrors::IDENTICAL_LANGUAGES));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATION_ERROR, infobar->type());
  EXPECT_EQ(TranslateErrors::IDENTICAL_LANGUAGES, infobar->error());

  // Let's run the same steps again but this time the server fails to detect the
  // page's language (it returns an empty string).
  SimulateNavigation(GURL("http://www.google.com"), "und", true);
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(2, 0, "", "en",
      TranslateErrors::UNKNOWN_LANGUAGE));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATION_ERROR, infobar->type());
  EXPECT_EQ(TranslateErrors::UNKNOWN_LANGUAGE, infobar->error());
}

// Tests that we show/don't show an info-bar for all languages the CLD can
// report.
TEST_F(TranslateManagerTest, TestAllLanguages) {
  // The index in kExpectation are the Language enum (see languages.pb.h).
  // true if we expect a translate infobar for that language.
  // Note the supported languages are in translation_manager.cc, see
  // kSupportedLanguages.
  bool kExpectations[] = {
    // 0-9
    false, true, true, true, true, true, true, true, true, true,
    // 10-19
    true, true, true, true, true, true, true, true, true, true,
    // 20-29
    true, true, true, true, true, false, false, true, true, true,
    // 30-39
    true, true, true, true, true, true, true, false, true, false,
    // 40-49
    true, false, true, false, false, true, false, true, false, false,
    // 50-59
    true, false, false, true, true, true, false, true, false, false,
    // 60-69
    false, false, true, true, false, true, true, false, true, true,
    // 70-79
    false, false, false, false, true, true, false, true, false, false,
    // 80-89
    false, false, false, false, false, false, false, false, false, false,
    // 90-99
    false, true, false, false, false, false, false, true, false, false,
    // 100-109
    false, true, false, false, false, false, false, false, false, false,
    // 110-119
    false, false, false, false, false, false, false, false, false, false,
    // 120-129
    false, false, false, false, false, false, false, false, false, false,
    // 130-139
    false, false, false, false, false, false, false, false, false, true,
    // 140-149
    false, false, false, false, false, false, false, false, false, false,
    // 150-159
    false, false, false, false, false, false, false, false, false, false,
    // 160
    false
  };

  GURL url("http://www.google.com");
  for (size_t i = 0; i < arraysize(kExpectations); ++i) {
    ASSERT_LT(i, static_cast<size_t>(NUM_LANGUAGES));

    std::string lang = LanguageCodeWithDialects(static_cast<Language>(i));
    SCOPED_TRACE(::testing::Message() << "Iteration " << i <<
        " language=" << lang);

    // We should not have a translate infobar.
    TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar == NULL);

    // Simulate navigating to a page.
    NavigateAndCommit(url);
    SimulateOnTranslateLanguageDetermined(lang, true);

    // Verify we have/don't have an info-bar as expected.
    infobar = GetTranslateInfoBar();
    EXPECT_EQ(kExpectations[i], infobar != NULL);

    // Close the info-bar if applicable.
    if (infobar != NULL)
      EXPECT_TRUE(CloseTranslateInfoBar());
  }
}

// Tests auto-translate on page.
TEST_F(TranslateManagerTest, AutoTranslateOnNavigate) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  SimulateURLFetch(true);  // Simulate the translate script being retrieved.

  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // Now navigate to a new page in the same language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.fr"), "fr", true);

  // This should have automatically triggered a translation.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(1, page_id);
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // Now navigate to a page in a different language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.es"), "es", true);

  // This should not have triggered a translate.
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
}

// Tests that multiple OnPageContents do not cause multiple infobars.
TEST_F(TranslateManagerTest, MultipleOnPageContents) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate clicking 'Nope' (don't translate).
  EXPECT_TRUE(DenyTranslation());
  EXPECT_EQ(0U, contents()->infobar_count());

  // Send a new PageContents, we should not show an infobar.
  SimulateOnTranslateLanguageDetermined("fr", true);
  EXPECT_EQ(0U, contents()->infobar_count());

  // Do the same steps but simulate closing the infobar this time.
  SimulateNavigation(GURL("http://www.youtube.fr"), "fr", true);
  EXPECT_TRUE(CloseTranslateInfoBar());
  EXPECT_EQ(0U, contents()->infobar_count());
  SimulateOnTranslateLanguageDetermined("fr", true);
  EXPECT_EQ(0U, contents()->infobar_count());
}

// Test that reloading the page brings back the infobar.
TEST_F(TranslateManagerTest, Reload) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Reload should bring back the infobar.
  NavEntryCommittedObserver nav_observer(contents());
  Reload();

  // Ensures it is really handled a reload.
  const NavigationController::LoadCommittedDetails& nav_details =
      nav_observer.get_load_commited_details();
  EXPECT_TRUE(nav_details.entry != NULL);  // There was a navigation.
  EXPECT_EQ(NavigationType::EXISTING_PAGE, nav_details.type);

  // The TranslateManager class processes the navigation entry committed
  // notification in a posted task; process that task.
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Test that reloading the page by way of typing again the URL in the
// location bar brings back the infobar.
TEST_F(TranslateManagerTest, ReloadFromLocationBar) {
  GURL url("http://www.google.fr");

  // Simulate navigating to a page and getting its language.
  SimulateNavigation(url, "fr", true);

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Create a pending navigation and simulate a page load.  That should be the
  // equivalent of typing the URL again in the location bar.
  NavEntryCommittedObserver nav_observer(contents());
  contents()->controller().LoadURL(url, GURL(), PageTransition::TYPED);
  rvh()->SendNavigate(0, url);

  // Test that we are really getting a same page navigation, the test would be
  // useless if it was not the case.
  const NavigationController::LoadCommittedDetails& nav_details =
      nav_observer.get_load_commited_details();
  EXPECT_TRUE(nav_details.entry != NULL);  // There was a navigation.
  EXPECT_EQ(NavigationType::SAME_PAGE, nav_details.type);

  // The TranslateManager class processes the navigation entry committed
  // notification in a posted task; process that task.
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that a closed translate infobar does not reappear when navigating
// in-page.
TEST_F(TranslateManagerTest, CloseInfoBarInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr",
                     true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that a closed translate infobar does not reappear when navigating
// in a subframe. (http://crbug.com/48215)
TEST_F(TranslateManagerTest, CloseInfoBarInSubframeNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Simulate a sub-frame auto-navigating.
  rvh()->SendNavigateWithTransition(1, GURL("http://pub.com"),
                                    PageTransition::AUTO_SUBFRAME);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Simulate the user navigating in a sub-frame.
  rvh()->SendNavigateWithTransition(2, GURL("http://pub.com"),
                                    PageTransition::MANUAL_SUBFRAME);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}



// Tests that denying translation is sticky when navigating in page.
TEST_F(TranslateManagerTest, DenyTranslateInPageNavigation) {
  // Simulate navigating to a page and getting its language.
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
TEST_F(TranslateManagerTest, TranslateCloseInfoBarInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  SimulateURLFetch(true);  // Simulate the translate script being retrieved.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr", true);
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  // Note that we navigate to a page in a different language so we don't trigger
  // the auto-translate feature (it would translate the page automatically and
  // the before translate inforbar would not be shown).
  SimulateNavigation(GURL("http://www.google.de"), "de", true);
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that the after translate the infobar still shows when navigating
// in-page.
TEST_F(TranslateManagerTest, TranslateInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  SimulateURLFetch(true);  // Simulate the translate script being retrieved.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));
  // The after translate infobar is showing.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);

  // Navigate in page, the same infobar should still be shown.
  ClearRemovedInfoBars();
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), "fr",
                     true);
  EXPECT_FALSE(InfoBarRemoved());
  EXPECT_EQ(infobar, GetTranslateInfoBar());

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
TEST_F(TranslateManagerTest, CLDReportsUnsupportedPageLanguage) {
  // Simulate navigating to a page and getting an unsupported language.
  SimulateNavigation(GURL("http://www.google.com"), "qbz", true);

  // No info-bar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that we deal correctly with unsupported languages returned by the
// server.
// The translation server might return a language we don't support.
TEST_F(TranslateManagerTest, ServerReportsUnsupportedLanguage) {
  // Simulate navigating to a page and translating it.
  SimulateNavigation(GURL("http://mail.google.fr"), "fr", true);
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  SimulateURLFetch(true);
  // Simulate the render notifying the translation has been done, but it
  // reports a language we don't support.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "qbz", "en",
      TranslateErrors::NONE));

  // An error infobar should be showing to report that we don't support this
  // language.
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATION_ERROR, infobar->type());

  // This infobar should have a button (so the string should not be empty).
  ASSERT_FALSE(infobar->GetMessageInfoBarButtonText().empty());

  // Pressing the button on that infobar should revert to the original language.
  process()->sink().ClearMessages();
  infobar->MessageInfoBarButtonPressed();
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(ViewMsg_RevertTranslation::ID);
  EXPECT_TRUE(message != NULL);
  // And it should have removed the infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that no translate infobar is shown when Chrome is in a language that
// the translate server does not support.
TEST_F(TranslateManagerTest, UnsupportedUILanguage) {
  TestingBrowserProcess* browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  std::string original_lang = browser_process->GetApplicationLocale();
  browser_process->SetApplicationLocale("qbz");

  // Simulate navigating to a page in a language supported by the translate
  // server.
  SimulateNavigation(GURL("http://www.google.com"), "en", true);

  // No info-bar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  browser_process->SetApplicationLocale(original_lang);
}

// Tests that the translate enabled preference is honored.
TEST_F(TranslateManagerTest, TranslateEnabledPref) {
  // Make sure the pref allows translate.
  PrefService* prefs = contents()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kEnableTranslate, true);

  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // An infobar should be shown.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
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
TEST_F(TranslateManagerTest, NeverTranslateLanguagePref) {
  // Simulate navigating to a page and getting its language.
  GURL url("http://www.google.fr");
  SimulateNavigation(url, "fr", true);

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this language.
  PrefService* prefs = contents()->profile()->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(TranslatePrefs::kPrefTranslateLanguageBlacklist,
                &pref_observer_);
  TranslatePrefs translate_prefs(prefs);
  EXPECT_FALSE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateLanguageBlacklist);
  translate_prefs.BlacklistLanguage("fr");
  EXPECT_TRUE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_FALSE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also in French.
  SimulateNavigation(GURL("http://wwww.youtube.fr"), "fr", true);

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the language from the blacklist.
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateLanguageBlacklist);
  translate_prefs.RemoveLanguageFromBlacklist("fr");
  EXPECT_FALSE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Navigate to a page in French.
  SimulateNavigation(url, "fr", true);

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Never translate this site" pref.
TEST_F(TranslateManagerTest, NeverTranslateSitePref) {
  // Simulate navigating to a page and getting its language.
  GURL url("http://www.google.fr");
  std::string host(url.host());
  SimulateNavigation(url, "fr", true);

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this site.
  PrefService* prefs = contents()->profile()->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(TranslatePrefs::kPrefTranslateSiteBlacklist,
                &pref_observer_);
  TranslatePrefs translate_prefs(prefs);
  EXPECT_FALSE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateSiteBlacklist);
  translate_prefs.BlacklistSite(host);
  EXPECT_TRUE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_FALSE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also on the same site.
  SimulateNavigation(GURL("http://www.google.fr/hello"), "fr", true);

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the site from the blacklist.
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateSiteBlacklist);
  translate_prefs.RemoveSiteFromBlacklist(host);
  EXPECT_FALSE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Navigate to a page in French.
  SimulateNavigation(url, "fr", true);

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Always translate this language" pref.
TEST_F(TranslateManagerTest, AlwaysTranslateLanguagePref) {
  // Select always translate French to English.
  PrefService* prefs = contents()->profile()->GetPrefs();
  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(TranslatePrefs::kPrefTranslateWhitelists,
                &pref_observer_);
  TranslatePrefs translate_prefs(prefs);
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateWhitelists);
  translate_prefs.WhitelistLanguagePair("fr", "en");

  // Load a page in French.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);

  // It should have triggered an automatic translation to English.

  // The translating infobar should be showing.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATING, infobar->type());

  SimulateURLFetch(true);  // Simulate the translate script being retrieved.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);
  process()->sink().ClearMessages();

  // Try another language, it should not be autotranslated.
  SimulateNavigation(GURL("http://www.google.es"), "es", true);
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Let's switch to incognito mode, it should not be autotranslated in that
  // case either.
  TestingProfile* test_profile =
      static_cast<TestingProfile*>(contents()->profile());
  test_profile->set_incognito(true);
  SimulateNavigation(GURL("http://www.youtube.fr"), "fr", true);
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());
  test_profile->set_incognito(false);  // Get back to non incognito.

  // Now revert the always translate pref and make sure we go back to expected
  // behavior, which is show a "before translate" infobar.
  SetPrefObserverExpectation(TranslatePrefs::kPrefTranslateWhitelists);
  translate_prefs.RemoveLanguagePairFromWhitelist("fr", "en");
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());
}

// Context menu.
TEST_F(TranslateManagerTest, ContextMenu) {
  // Blacklist www.google.fr and French for translation.
  GURL url("http://www.google.fr");
  TranslatePrefs translate_prefs(contents()->profile()->GetPrefs());
  translate_prefs.BlacklistLanguage("fr");
  translate_prefs.BlacklistSite(url.host());
  EXPECT_TRUE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_TRUE(translate_prefs.IsSiteBlacklisted(url.host()));

  // Simulate navigating to a page in French. The translate menu should show but
  // should only be enabled when the page language has been received.
  NavigateAndCommit(url);
  scoped_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Simulate receiving the language.
  SimulateOnTranslateLanguageDetermined("fr", true);
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Use the menu to translate the page.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);

  // That should have triggered a translation.
  // The "translating..." infobar should be showing.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::TRANSLATING, infobar->type());
  SimulateURLFetch(true);  // Simulate the translate script being retrieved.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);
  process()->sink().ClearMessages();

  // This should also have reverted the blacklisting of this site and language.
  EXPECT_FALSE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_FALSE(translate_prefs.IsSiteBlacklisted(url.host()));

  // Let's simulate the page being translated.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // The translate menu should now be disabled.
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
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
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  process()->sink().ClearMessages();
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);
  // No message expected since the translation should have been ignored.
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));

  // Now test that selecting translate in the context menu AFTER the page has
  // been translated does nothing.
  SimulateNavigation(GURL("http://www.google.de"), "de", true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  process()->sink().ClearMessages();
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "de", "en",
      TranslateErrors::NONE));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_TRANSLATE);
  // No message expected since the translation should have been ignored.
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));

  // Test that the translate context menu is enabled when the page is in an
  // unknown language.
  SimulateNavigation(url, "und", true);
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_TRUE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));

  // Test that the translate context menu is disabled when the page is in an
  // unsupported language.
  SimulateNavigation(url, "qbz", true);
  menu.reset(TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
}

// Tests that an extra always/never translate button is shown on the "before
// translate" infobar when the translation is accepted/declined 3 times,
// only when not in incognito mode.
TEST_F(TranslateManagerTest, BeforeTranslateExtraButtons) {
  TranslatePrefs translate_prefs(contents()->profile()->GetPrefs());
  translate_prefs.ResetTranslationAcceptedCount("fr");
  translate_prefs.ResetTranslationDeniedCount("fr");
  translate_prefs.ResetTranslationAcceptedCount("de");
  translate_prefs.ResetTranslationDeniedCount("de");

  // We'll do 4 times in incognito mode first to make sure the button is not
  // shown in that case, then 4 times in normal mode.
  TranslateInfoBarDelegate* infobar;
  TestingProfile* test_profile =
      static_cast<TestingProfile*>(contents()->profile());
  test_profile->set_incognito(true);
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i <<
        " incognito mode=" << test_profile->IsOffTheRecord());
    SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
    infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar != NULL);
    EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());
    if (i < 7) {
      EXPECT_FALSE(infobar->ShouldShowAlwaysTranslateButton());
      infobar->Translate();
      process()->sink().ClearMessages();
    } else {
      EXPECT_TRUE(infobar->ShouldShowAlwaysTranslateButton());
    }
    if (i == 3)
      test_profile->set_incognito(false);
  }
  // Simulate the user pressing "Always translate French".
  infobar->AlwaysTranslatePageLanguage();
  EXPECT_TRUE(translate_prefs.IsLanguagePairWhitelisted("fr", "en"));
  // Simulate the translate script being retrieved (it only needs to be done
  // once in the test as it is cached).
  SimulateURLFetch(true);
  // That should have triggered a page translate.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  process()->sink().ClearMessages();

  // Now test that declining the translation causes a "never translate" button
  // to be shown (in non incognito mode only).
  test_profile->set_incognito(true);
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i <<
        " incognito mode=" << test_profile->IsOffTheRecord());
    SimulateNavigation(GURL("http://www.google.de"), "de", true);
    infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar != NULL);
    EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());
    if (i < 7) {
      EXPECT_FALSE(infobar->ShouldShowNeverTranslateButton());
      infobar->TranslationDeclined();
    } else {
      EXPECT_TRUE(infobar->ShouldShowNeverTranslateButton());
    }
    if (i == 3)
      test_profile->set_incognito(false);
  }
  // Simulate the user pressing "Never translate French".
  infobar->NeverTranslatePageLanguage();
  EXPECT_TRUE(translate_prefs.IsLanguageBlacklisted("de"));
  // No translation should have occured and the infobar should be gone.
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  process()->sink().ClearMessages();
  ASSERT_TRUE(GetTranslateInfoBar() == NULL);
}

// Tests that we don't show a translate infobar when a page instructs that it
// should not be translated.
TEST_F(TranslateManagerTest, NonTranslatablePage) {
  // Simulate navigating to a page.
  SimulateNavigation(GURL("http://mail.google.fr"), "fr", false);

  // We should not have an infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // The context menu should be disabled.
  scoped_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::CreateContextMenu(contents()));
  menu->Init();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_TRANSLATE));
  EXPECT_FALSE(menu->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_TRANSLATE));
}

// Tests that the script is expired and refetched as expected.
TEST_F(TranslateManagerTest, ScriptExpires) {
  ExpireTranslateScriptImmediately();

  // Simulate navigating to a page and translating it.
  SimulateNavigation(GURL("http://www.google.fr"), "fr", true);
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  SimulateURLFetch(true);
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en",
      TranslateErrors::NONE));

  // A task should have been posted to clear the script, run it.
  MessageLoop::current()->RunAllPending();

  // Do another navigation and translation.
  SimulateNavigation(GURL("http://www.google.es"), "es", true);
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  process()->sink().ClearMessages();
  infobar->Translate();
  // If we don't simulate the URL fetch, the TranslateManager should be waiting
  // for the script and no message should have been sent to the renderer.
  EXPECT_TRUE(
      process()->sink().GetFirstMessageMatching(ViewMsg_TranslatePage::ID) ==
      NULL);
  // Now simulate the URL fetch.
  SimulateURLFetch(true);
  // Now the message should have been sent.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ("es", original_lang);
  EXPECT_EQ("en", target_lang);
}
