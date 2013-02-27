// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"

using testing::AtLeast;
using testing::Return;

class TestTranslateHelper : public TranslateHelper {
 public:
  explicit TestTranslateHelper(content::RenderView* render_view)
      : TranslateHelper(render_view) {
  }

  virtual bool DontDelayTasks() { return true; }

  void TranslatePage(int page_id,
                     const std::string& source_lang,
                     const std::string& target_lang,
                     const std::string& translate_script) {
    OnTranslatePage(page_id, translate_script, source_lang, target_lang);
  }

  MOCK_METHOD0(IsTranslateLibAvailable, bool());
  MOCK_METHOD0(IsTranslateLibReady, bool());
  MOCK_METHOD0(HasTranslationFinished, bool());
  MOCK_METHOD0(HasTranslationFailed, bool());
  MOCK_METHOD0(GetOriginalPageLanguage, std::string());
  MOCK_METHOD0(StartTranslation, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTranslateHelper);
};

class TranslateHelperBrowserTest : public ChromeRenderViewTest {
 public:
  TranslateHelperBrowserTest() : translate_helper_(NULL) {}

 protected:
  virtual void SetUp() {
    ChromeRenderViewTest::SetUp();
    translate_helper_ = new TestTranslateHelper(view_);
  }

  virtual void TearDown() {
    delete translate_helper_;
    ChromeRenderViewTest::TearDown();
  }

  bool GetPageTranslatedMessage(int* page_id,
                                std::string* original_lang,
                                std::string* target_lang,
                                TranslateErrors::Type* error) {
    const IPC::Message* message = render_thread_->sink().
        GetUniqueMessageMatching(ChromeViewHostMsg_PageTranslated::ID);
    if (!message)
      return false;
    Tuple4<int, std::string, std::string, TranslateErrors::Type>
        translate_param;
    ChromeViewHostMsg_PageTranslated::Read(message, &translate_param);
    if (page_id)
      *page_id = translate_param.a;
    if (original_lang)
      *original_lang = translate_param.b;
    if (target_lang)
      *target_lang = translate_param.c;
    if (error)
      *error = translate_param.d;
    return true;
  }

  TestTranslateHelper* translate_helper_;
};

// Tests that the browser gets notified of the translation failure if the
// translate library fails/times-out during initialization.
TEST_F(TranslateHelperBrowserTest, TranslateLibNeverReady) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .Times(AtLeast(5))  // See kMaxTranslateInitCheckAttempts in
                          // translate_helper.cc
      .WillRepeatedly(Return(false));

  translate_helper_->TranslatePage(
      view_->GetPageId(), "en", "fr", std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, NULL, NULL, &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ(TranslateErrors::INITIALIZATION_ERROR, error);
}

// Tests that the browser gets notified of the translation success when the
// translation succeeds.
TEST_F(TranslateHelperBrowserTest, TranslateSuccess) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_helper_, StartTranslation()).WillOnce(Return(true));

  // Succeed after few checks.
  EXPECT_CALL(*translate_helper_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_helper_, HasTranslationFinished())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(
      view_->GetPageId(), original_lang, target_lang, std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that the browser gets notified of the translation failure when the
// translation fails.
TEST_F(TranslateHelperBrowserTest, TranslateFailure) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_helper_, StartTranslation()).WillOnce(Return(true));

  // Fail after few checks.
  EXPECT_CALL(*translate_helper_, HasTranslationFailed())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_helper_, HasTranslationFinished())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));

  translate_helper_->TranslatePage(
      view_->GetPageId(), "en", "fr", std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, NULL, NULL, &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ(TranslateErrors::TRANSLATION_ERROR, error);
}

// Tests that when the browser translate a page for which the language is
// undefined we query the translate element to get the language.
TEST_F(TranslateHelperBrowserTest, UndefinedSourceLang) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .WillOnce(Return(true));

  EXPECT_CALL(*translate_helper_, GetOriginalPageLanguage())
      .WillOnce(Return("de"));

  EXPECT_CALL(*translate_helper_, StartTranslation()).WillOnce(Return(true));
  EXPECT_CALL(*translate_helper_, HasTranslationFailed())
      .WillOnce(Return(false));
  EXPECT_CALL(*translate_helper_, HasTranslationFinished())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  translate_helper_->TranslatePage(view_->GetPageId(),
                                   chrome::kUnknownLanguageCode, "fr",
                                   std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  TranslateErrors::Type error;
  std::string original_lang;
  std::string target_lang;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, &original_lang, &target_lang,
                                       &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ("de", original_lang);
  EXPECT_EQ("fr", target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that starting a translation while a similar one is pending does not
// break anything.
TEST_F(TranslateHelperBrowserTest, MultipleSimilarTranslations) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_helper_, StartTranslation())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_helper_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_helper_, HasTranslationFinished())
      .WillOnce(Return(true));

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(
      view_->GetPageId(), original_lang, target_lang, std::string());
  // While this is running call again TranslatePage to make sure noting bad
  // happens.
  translate_helper_->TranslatePage(
      view_->GetPageId(), original_lang, target_lang, std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that starting a translation while a different one is pending works.
TEST_F(TranslateHelperBrowserTest, MultipleDifferentTranslations) {
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_helper_, StartTranslation())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*translate_helper_, HasTranslationFailed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*translate_helper_, HasTranslationFinished())
      .WillOnce(Return(true));

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(
      view_->GetPageId(), original_lang, target_lang, std::string());
  // While this is running call again TranslatePage with a new target lang.
  std::string new_target_lang("de");
  translate_helper_->TranslatePage(
      view_->GetPageId(), original_lang, new_target_lang, std::string());
  MessageLoop::current()->RunUntilIdle();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->GetPageId(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(new_target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that we send the right translate language message for a page and that
// we respect the "no translate" meta-tag.
TEST_F(ChromeRenderViewTest, TranslatablePage) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  SendContentStateImmediately();

  LoadHTML("<html><body>A random page with random content.</body></html>");
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Param params;
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_TRUE(params.b) << "Page should be translatable.";
  render_thread_->sink().ClearMessages();

  // Now the page specifies the META tag to prevent translation.
  LoadHTML("<html><head><meta name=\"google\" value=\"notranslate\"></head>"
           "<body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_FALSE(params.b) << "Page should not be translatable.";
  render_thread_->sink().ClearMessages();

  // Try the alternate version of the META tag (content instead of value).
  LoadHTML("<html><head><meta name=\"google\" content=\"notranslate\"></head>"
           "<body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_FALSE(params.b) << "Page should not be translatable.";
}

// Tests that the language meta tag takes precedence over the CLD when reporting
// the page's language.
TEST_F(ChromeRenderViewTest, LanguageMetaTag) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  SendContentStateImmediately();

  LoadHTML("<html><head><meta http-equiv=\"content-language\" content=\"es\">"
           "</head><body>A random page with random content.</body></html>");
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Param params;
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("es", params.a);
  render_thread_->sink().ClearMessages();

  // Makes sure we support multiple languages specified.
  LoadHTML("<html><head><meta http-equiv=\"content-language\" "
           "content=\" fr , es,en \">"
           "</head><body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a);
}

// Tests that the language meta tag works even with non-all-lower-case.
// http://code.google.com/p/chromium/issues/detail?id=145689
TEST_F(ChromeRenderViewTest, LanguageMetaTagCase) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  SendContentStateImmediately();

  LoadHTML("<html><head><meta http-equiv=\"Content-Language\" content=\"es\">"
           "</head><body>A random page with random content.</body></html>");
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Param params;
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("es", params.a);
  render_thread_->sink().ClearMessages();

  // Makes sure we support multiple languages specified.
  LoadHTML("<html><head><meta http-equiv=\"Content-Language\" "
           "content=\" fr , es,en \">"
           "</head><body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a);
}

// Tests that the language meta tag is converted to Chrome standard of dashes
// instead of underscores and proper capitalization.
// http://code.google.com/p/chromium/issues/detail?id=159487
TEST_F(ChromeRenderViewTest, LanguageCommonMistakesAreCorrected) {
  // Suppress the normal delay that occurs when the page is loaded before which
  // the renderer sends the page contents to the browser.
  SendContentStateImmediately();

  LoadHTML("<html><head><meta http-equiv='Content-Language' content='EN_us'>"
           "</head><body>A random page with random content.</body></html>");
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Param params;
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("en-US", params.a);
  render_thread_->sink().ClearMessages();
}

// Tests that a back navigation gets a translate language message.
TEST_F(ChromeRenderViewTest, BackToTranslatablePage) {
  SendContentStateImmediately();
  LoadHTML("<html><head><meta http-equiv=\"content-language\" content=\"zh\">"
           "</head><body>This page is in Chinese.</body></html>");
  const IPC::Message* message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Param params;
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("zh", params.a);
  render_thread_->sink().ClearMessages();

  LoadHTML("<html><head><meta http-equiv=\"content-language\" content=\"fr\">"
           "</head><body>This page is in French.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a);
  render_thread_->sink().ClearMessages();

  GoBack(GetMainFrame()->previousHistoryItem());

  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("zh", params.a);
  render_thread_->sink().ClearMessages();
}
