// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"

#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_browser_process.h"
#include "third_party/cld/languages/public/languages.h"

class TestTranslateManager : public TranslateManager {
 public:
  TestTranslateManager() {}
};

class TranslateManagerTest : public RenderViewHostTestHarness,
                             public NotificationObserver {
 public:
  TranslateManagerTest() {}

  // Simluates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url, int page_id,
                          const std::wstring& contents,
                          const std::string& lang) {
    NavigateAndCommit(url);
    SimulateOnPageContents(url, page_id, contents, lang);
  }

  void SimulateOnPageContents(const GURL& url, int page_id,
                              const std::wstring& contents,
                              const std::string& lang) {
    rvh()->TestOnMessageReceived(ViewHostMsg_PageContents(0, url, page_id,
                                                          contents, lang));
  }

  bool GetTranslateMessage(int* page_id,
                           std::string* original_lang,
                           std::string* target_lang) {
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(ViewMsg_TranslatePage::ID);
    if (!message)
      return false;
    Tuple3<int, std::string, std::string> translate_param;
    ViewMsg_TranslatePage::Read(message, &translate_param);
    *page_id = translate_param.a;
    *original_lang = translate_param.b;
    *target_lang = translate_param.c;
    return true;
  }

  // Returns the translate infobar if there is 1 infobar and it is a translate
  // infobar.
  TranslateInfoBarDelegate* GetTranslateInfoBar() {
    if (contents()->infobar_delegate_count() != 1)
      return NULL;
    return contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();
  }

  // If there is 1 infobar and it is a translate infobar, closes it and returns
  // true.  Returns false otherwise.
  bool CloseTranslateInfoBar() {
    TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    if (!infobar)
      return false;
    infobar->InfoBarDismissed();  // Simulates closing the infobar.
    contents()->RemoveInfoBar(infobar);
    return true;
  }

  // Checks whether |infobar| has been removed and clears the removed infobar
  // list.
  bool CheckInfoBarRemovedAndReset(InfoBarDelegate* infobar) {
    bool found = std::find(removed_infobars_.begin(), removed_infobars_.end(),
                           infobar) != removed_infobars_.end();
    removed_infobars_.clear();
    return found;
  }

  // Returns true if at least one infobar was closed.
  bool InfoBarRemoved() {
    return !removed_infobars_.empty();
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
    DCHECK(type == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED);
    removed_infobars_.push_back(Details<InfoBarDelegate>(details).ptr());
  }

 protected:
  virtual void SetUp() {
    TranslateManager::set_test_enabled(true);
    // This must be created after set_test_enabled() has been called to register
    // notifications properly.  Note that we do this before calling
    // RenderViewHostTestHarness::SetUp() to match what's done in Chrome, where
    // the TranslateManager is created before the TabContents.  This matters for
    // as they both register for similar events and we want the notifications
    // to happen in the same sequence (TranslateManager first, TabContents
    // second).
    translate_manager_.reset(new TestTranslateManager());

    RenderViewHostTestHarness::SetUp();

    notification_registrar_.Add(
        this,
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(contents()));
  }

  virtual void TearDown() {
    notification_registrar_.Remove(
        this,
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(contents()));

    RenderViewHostTestHarness::TearDown();

    TranslateManager::set_test_enabled(false);
  }

 private:
  NotificationRegistrar notification_registrar_;

  scoped_ptr<TestTranslateManager> translate_manager_;

  // The list of infobars that have been removed.
  // WARNING: the pointers points to deleted objects, use only for comparison.
  std::vector<InfoBarDelegate*> removed_infobars_;

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

TEST_F(TranslateManagerTest, NormalTranslate) {
  // Simulate navigating to a page.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // We should have an info-bar.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::kBeforeTranslate, infobar->state());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();
  EXPECT_FALSE(InfoBarRemoved());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(0, page_id);
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // The infobar should still be there but in the translating state.
  ASSERT_EQ(infobar, GetTranslateInfoBar());  // Same instance.
  // TODO(jcampan): the state is not set if the button is not clicked.
  //                Refactor the infobar code so we can simulate the click.
  // EXPECT_EQ(TranslateInfoBarDelegate::kTranslating, infobar->state());

  // Simulate the render notifying the translation has been done.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // The infobar should have changed to the after state.
  EXPECT_FALSE(InfoBarRemoved());
  ASSERT_EQ(infobar, GetTranslateInfoBar());
  // TODO(jcampan): the TranslateInfoBar is listening for the PAGE_TRANSLATED
  //                notification. Since in unit-test, no actual info-bar is
  //                created, it does not get the notification and does not
  //                update its state.  Ideally the delegate (or rather model)
  //                would be the one listening for notifications and updating
  //                states.  That would make this test work.
  // EXPECT_EQ(TranslateInfoBarDelegate::kAfterTranslate, infobar->state());

  // Simulate translating again from there but 2 different languages.
  infobar->ModifyOriginalLanguage(0);
  infobar->ModifyTargetLanguage(1);
  std::string new_original_lang = infobar->original_lang_code();
  std::string new_target_lang = infobar->target_lang_code();
  process()->sink().ClearMessages();
  infobar->Translate();

  // Test that we sent the right message to the renderer.
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(0, page_id);
  EXPECT_EQ(new_original_lang, original_lang);
  EXPECT_EQ(new_target_lang, target_lang);
}

// Tests that we show/don't show an info-bar for all languages the CLD can
// report.
TEST_F(TranslateManagerTest, TestAllLanguages) {
  // The index in kExpectation are the Language enum (see languages.pb.h).
  // true if we expect a translate infobar for that language.
  // Note the supported languages are in translation_service.cc, see
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
    false, false, false, true, true, true, false, false, false, false,
    // 60-69
    false, false, true, true, false, true, true, false, true, true,
    // 70-79
    false, false, false, false, false, false, false, true, false, false,
    // 80-89
    false, false, false, false, false, false, false, false, false, false,
    // 90-99
    false, true, false, false, false, false, false, false, false, false,
    // 100-109
    false, true, false, false, false, false, false, false, false, false,
    // 110-119
    false, false, false, false, false, false, false, false, false, false,
    // 120-129
    false, false, false, false, false, false, false, false, false, false,
    // 130-139
    false, false, false, false, false, false, false, false, false, false,
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
    SCOPED_TRACE(::testing::Message::Message() << "Iteration " << i <<
        " language=" << lang);

    // We should not have a translate infobar.
    TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
    ASSERT_TRUE(infobar == NULL);

    // Simulate navigating to a page.
    NavigateAndCommit(url);
    SimulateOnPageContents(url, i, L"", lang);

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
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // Now navigate to a new page in the same language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.fr"), 1, L"Les news", "fr");

  // This should have automatically triggered a translation.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(1, page_id);
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // Now navigate to a page in a different language.
  process()->sink().ClearMessages();
  SimulateNavigation(GURL("http://news.google.es"), 1, L"Las news", "es");

  // This should not have triggered a translate.
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
}

// Tests that multiple OnPageContents do not cause multiple infobars.
TEST_F(TranslateManagerTest, MultipleOnPageContents) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate clicking 'Nope' (don't translate).
  EXPECT_TRUE(DenyTranslation());
  EXPECT_EQ(0, contents()->infobar_delegate_count());

  // Send a new PageContents, we should not show an infobar.
  SimulateOnPageContents(GURL("http://www.google.fr"), 0, L"Le Google", "fr");
  EXPECT_EQ(0, contents()->infobar_delegate_count());

  // Do the same steps but simulate closing the infobar this time.
  SimulateNavigation(GURL("http://www.youtube.fr"), 1, L"Le YouTube", "fr");
  EXPECT_TRUE(CloseTranslateInfoBar());
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  SimulateOnPageContents(GURL("http://www.youtube.fr"), 1, L"Le YouTube", "fr");
  EXPECT_EQ(0, contents()->infobar_delegate_count());
}

// Test that reloading the page brings back the infobar.
TEST_F(TranslateManagerTest, Reload) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

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
  SimulateNavigation(url, 0, L"Le Google", "fr");

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

// Tests that a close translate infobar does not reappear when navigating
// in-page.
TEST_F(TranslateManagerTest, CloseInfoBarInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), 0, L"Le Google", "fr");
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), 0, L"Le Google", "fr");
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that denying translation is sticky when navigating in page.
TEST_F(TranslateManagerTest, DenyTranslateInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate clicking 'Nope' (don't translate).
  EXPECT_TRUE(DenyTranslation());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), 0, L"Le Google", "fr");
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  SimulateNavigation(GURL("http://www.google.fr/foot"), 0, L"Le Google", "fr");
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that after translating and closing the infobar, the infobar does not
// return when navigating in page.
TEST_F(TranslateManagerTest, TranslateCloseInfoBarInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate in page, no infobar should be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), 0, L"Le Google", "fr");
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Navigate out of page, a new infobar should show.
  // Note that we navigate to a page in a different language so we don't trigger
  // the auto-translate feature (it would translate the page automatically and
  // the before translate inforbar would not be shown).
  SimulateNavigation(GURL("http://www.google.de"), 0, L"Das Google", "de");
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that the after translate the infobar still shows when navigating
// in-page.
TEST_F(TranslateManagerTest, TranslateInPageNavigation) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate the user translating.
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  infobar->Translate();
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // Navigate in page, the same infobar should still be shown.
  SimulateNavigation(GURL("http://www.google.fr/#ref1"), 0, L"Le Google", "fr");
  EXPECT_FALSE(InfoBarRemoved());
  EXPECT_EQ(infobar, GetTranslateInfoBar());

  // Navigate out of page, a new infobar should show.
  // See note in TranslateCloseInfoBarInPageNavigation test on why it is
  // important to navigate to a page in a different language for this test.
  SimulateNavigation(GURL("http://www.google.de"), 0, L"Das Google", "de");
  // The old infobar is gone.
  EXPECT_TRUE(CheckInfoBarRemovedAndReset(infobar));
  // And there is a new one.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests that no translate infobar is shown when navigating to a page in an
// unsupported language.
TEST_F(TranslateManagerTest, UnsupportedPageLanguage) {
  // Simulate navigating to a page and getting an unsupported language.
  SimulateNavigation(GURL("http://www.google.com"), 0, L"Google", "qbz");

  // No info-bar should be shown.
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
  SimulateNavigation(GURL("http://www.google.com"), 0, L"Google", "en");

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
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

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
  SimulateOnPageContents(url, 1, L"Le YouTube", "fr");
  infobar = GetTranslateInfoBar();
  EXPECT_TRUE(infobar == NULL);
}

// Tests the "Never translate <language>" pref.
TEST_F(TranslateManagerTest, NeverTranslateLanguagePref) {
  // Simulate navigating to a page and getting its language.
  GURL url("http://www.google.fr");
  SimulateNavigation(url, 0, L"Le Google", "fr");

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this language.
  PrefService* prefs = contents()->profile()->GetPrefs();
  TranslatePrefs translate_prefs(contents()->profile()->GetPrefs());
  EXPECT_FALSE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));
  translate_prefs.BlacklistLanguage("fr");
  EXPECT_TRUE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_FALSE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also in French.
  SimulateNavigation(GURL("http://wwww.youtube.fr"), 1, L"Le YouTube", "fr");

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the language from the blacklist.
  translate_prefs.RemoveLanguageFromBlacklist("fr");
  EXPECT_FALSE(translate_prefs.IsLanguageBlacklisted("fr"));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Navigate to a page in French.
  SimulateNavigation(url, 2, L"Le Google", "fr");

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Never translate this site" pref.
TEST_F(TranslateManagerTest, NeverTranslateSitePref) {
  // Simulate navigating to a page and getting its language.
  GURL url("http://www.google.fr");
  std::string host(url.host());
  SimulateNavigation(url, 0, L"Le Google", "fr");

  // An infobar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);

  // Select never translate this site.
  PrefService* prefs = contents()->profile()->GetPrefs();
  TranslatePrefs translate_prefs(contents()->profile()->GetPrefs());
  EXPECT_FALSE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));
  translate_prefs.BlacklistSite(host);
  EXPECT_TRUE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_FALSE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Navigate to a new page also on the same site.
  SimulateNavigation(GURL("http://www.google.fr/hello"), 1, L"Bonjour", "fr");

  // There should not be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Remove the site from the blacklist.
  translate_prefs.RemoveSiteFromBlacklist(host);
  EXPECT_FALSE(translate_prefs.IsSiteBlacklisted(host));
  EXPECT_TRUE(translate_prefs.CanTranslate(prefs, "fr", url));

  // Navigate to a page in French.
  SimulateNavigation(url, 0, L"Le Google", "fr");

  // There should be a translate infobar.
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests the "Always translate this language" pref.
TEST_F(TranslateManagerTest, AlwaysTranslateLanguagePref) {
  // Select always translate French to English.
  TranslatePrefs translate_prefs(contents()->profile()->GetPrefs());
  translate_prefs.WhitelistLanguagePair("fr", "en");

  // Load a page in French.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // It should have triggered an automatic translation to English.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(0, page_id);
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);
  process()->sink().ClearMessages();
  // And we should have no infobar (since we don't send the page translated
  // notification, the after translate infobar is not shown).
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  // Try another language, it should not be autotranslated.
  SimulateNavigation(GURL("http://www.google.es"), 1, L"El Google", "es");
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());

  // Let's switch to incognito mode, it should not be autotranslated in that
  // case either.
  TestingProfile* test_profile =
      static_cast<TestingProfile*>(contents()->profile());
  test_profile->set_off_the_record(true);
  SimulateNavigation(GURL("http://www.youtube.fr"), 2, L"Le YouTube", "fr");
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
  EXPECT_TRUE(CloseTranslateInfoBar());
  test_profile->set_off_the_record(false);  // Get back to non incognito.

  // Now revert the always translate pref and make sure we go back to expected
  // behavior, which is show an infobar.
  translate_prefs.RemoveLanguagePairFromWhitelist("fr", "en");
  SimulateNavigation(GURL("http://www.google.fr"), 3, L"Le Google", "fr");
  EXPECT_FALSE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_TRUE(GetTranslateInfoBar() != NULL);
}

// Tests TranslateManager::ShowInfoBar.
TEST_F(TranslateManagerTest, ShowInfoBar) {
  // Simulate navigating to a page and getting its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");
  TranslateInfoBarDelegate* infobar = GetTranslateInfoBar();
  EXPECT_TRUE(infobar != NULL);

  // ShowInfobar should have no effect since a bar is already showing.
  EXPECT_FALSE(TranslateManager::ShowInfoBar(contents()));
  // The infobar should still be showing.
  EXPECT_EQ(infobar, GetTranslateInfoBar());

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // ShowInfoBar should bring back the infobar.
  EXPECT_TRUE(TranslateManager::ShowInfoBar(contents()));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);

  // Translate.
  infobar->Translate();
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // Test again that ShowInfobar has no effect since a bar is already showing.
  EXPECT_FALSE(TranslateManager::ShowInfoBar(contents()));
  EXPECT_EQ(infobar, GetTranslateInfoBar());

  // Close the infobar.
  EXPECT_TRUE(CloseTranslateInfoBar());

  // ShowInfoBar should bring back the infobar, with the right languages.
  EXPECT_TRUE(TranslateManager::ShowInfoBar(contents()));
  infobar = GetTranslateInfoBar();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ("fr", infobar->original_lang_code());
  EXPECT_EQ("en", infobar->target_lang_code());
}
