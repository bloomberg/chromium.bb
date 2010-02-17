// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"

#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/render_messages.h"

class TestTranslateManager : public TranslateManager {
 public:
  TestTranslateManager() {}
};

class TranslateManagerTest : public RenderViewHostTestHarness {
 public:
  // Simluates navigating to a page and getting teh page contents and language
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

 protected:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();

    TranslateManager::set_test_enabled(true);
    // This must be created after set_test_enabled() has been called to register
    // notifications properly.
    translate_manager_.reset(new TestTranslateManager());
  }

  virtual void TearDown() {
    RenderViewHostTestHarness::TearDown();

    TranslateManager::set_test_enabled(false);
  }

 private:
  scoped_ptr<TestTranslateManager> translate_manager_;
};

TEST_F(TranslateManagerTest, NormalTranslate) {
  // Simulate navigating to a page.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // We should have an info-bar.
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  TranslateInfoBarDelegate* infobar =
      contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::kBeforeTranslate, infobar->state());

  // Simulate clicking translate.
  process()->sink().ClearMessages();
  infobar->Translate();

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::string original_lang, target_lang;
  EXPECT_TRUE(GetTranslateMessage(&page_id, &original_lang, &target_lang));
  EXPECT_EQ(0, page_id);
  EXPECT_EQ("fr", original_lang);
  EXPECT_EQ("en", target_lang);

  // The infobar should now be in the translating state.
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  ASSERT_EQ(infobar, contents()->GetInfoBarDelegateAt(0));  // Same instance.
  // TODO(jcampan): the state is not set if the button is not clicked.
  //                Refactor the infobar code so we can simulate the click.
  // EXPECT_EQ(TranslateInfoBarDelegate::kTranslating, infobar->state());

  // Simulate the render notifying the translation has been done.
  rvh()->TestOnMessageReceived(ViewHostMsg_PageTranslated(0, 0, "fr", "en"));

  // The infobar should have changed to the after state.
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  ASSERT_EQ(infobar, contents()->GetInfoBarDelegateAt(0));
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
  // Simulate navigating to a page and gettings its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate the user translating.
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  TranslateInfoBarDelegate* infobar =
      contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();
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
  // Simulate navigating to a page and gettings its language.
  SimulateNavigation(GURL("http://www.google.fr"), 0, L"Le Google", "fr");

  // Simulate clicking 'Nope' (don't translate).
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  TranslateInfoBarDelegate* infobar =
      contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(infobar != NULL);
  infobar->TranslationDeclined();
  contents()->RemoveInfoBar(infobar);
  EXPECT_EQ(0, contents()->infobar_delegate_count());

  // Send a new PageContents, we should not show an infobar.
  SimulateOnPageContents(GURL("http://www.google.fr"), 0, L"Le Google", "fr");
  EXPECT_EQ(0, contents()->infobar_delegate_count());

  // Do the same steps but simulate closing the infobar this time.
  SimulateNavigation(GURL("http://www.youtube.fr"), 1, L"Le YouTube", "fr");
  ASSERT_EQ(1, contents()->infobar_delegate_count());
  infobar = contents()->GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(infobar != NULL);
  infobar->InfoBarDismissed();  // Simulates closing the infobar.
  contents()->RemoveInfoBar(infobar);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  SimulateOnPageContents(GURL("http://www.youtube.fr"), 1, L"Le YouTube", "fr");
  EXPECT_EQ(0, contents()->infobar_delegate_count());
}
