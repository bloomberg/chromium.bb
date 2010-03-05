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
  browser_process->set_application_locale("qbz");

  // Simulate navigating to a page in a language supported by the translate
  // server.
  SimulateNavigation(GURL("http://www.google.com"), 0, L"Google", "en");

  // No info-bar should be shown.
  EXPECT_TRUE(GetTranslateInfoBar() == NULL);

  browser_process->set_application_locale(original_lang);
}

// Tests that the translate preference is honored.
TEST_F(TranslateManagerTest, TranslatePref) {
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
