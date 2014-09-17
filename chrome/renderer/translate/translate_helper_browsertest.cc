// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/renderer/isolated_world_ids.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/content/renderer/translate_helper.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "extensions/renderer/extension_groups.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using testing::AtLeast;
using testing::Return;
using testing::_;

class TestTranslateHelper : public translate::TranslateHelper {
 public:
  explicit TestTranslateHelper(content::RenderView* render_view)
      : translate::TranslateHelper(
            render_view,
            chrome::ISOLATED_WORLD_ID_TRANSLATE,
            extensions::EXTENSION_GROUP_INTERNAL_TRANSLATE_SCRIPTS,
            extensions::kExtensionScheme) {}

  virtual base::TimeDelta AdjustDelay(int delayInMs) OVERRIDE {
    // Just returns base::TimeDelta() which has initial value 0.
    // Tasks doesn't need to be delayed in tests.
    return base::TimeDelta();
  }

  void TranslatePage(const std::string& source_lang,
                     const std::string& target_lang,
                     const std::string& translate_script) {
    OnTranslatePage(0, translate_script, source_lang, target_lang);
  }

  MOCK_METHOD0(IsTranslateLibAvailable, bool());
  MOCK_METHOD0(IsTranslateLibReady, bool());
  MOCK_METHOD0(HasTranslationFinished, bool());
  MOCK_METHOD0(HasTranslationFailed, bool());
  MOCK_METHOD0(GetOriginalPageLanguage, std::string());
  MOCK_METHOD0(StartTranslation, bool());
  MOCK_METHOD1(ExecuteScript, void(const std::string&));
  MOCK_METHOD2(ExecuteScriptAndGetBoolResult, bool(const std::string&, bool));
  MOCK_METHOD1(ExecuteScriptAndGetStringResult,
               std::string(const std::string&));
  MOCK_METHOD1(ExecuteScriptAndGetDoubleResult, double(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTranslateHelper);
};

class TranslateHelperBrowserTest : public ChromeRenderViewTest {
 public:
  TranslateHelperBrowserTest() : translate_helper_(NULL) {}

 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewTest::SetUp();
    translate_helper_ = new TestTranslateHelper(view_);
  }

  virtual void TearDown() OVERRIDE {
    delete translate_helper_;
    ChromeRenderViewTest::TearDown();
  }

  bool GetPageTranslatedMessage(std::string* original_lang,
                                std::string* target_lang,
                                translate::TranslateErrors::Type* error) {
    const IPC::Message* message = render_thread_->sink().
        GetUniqueMessageMatching(ChromeViewHostMsg_PageTranslated::ID);
    if (!message)
      return false;
    Tuple3<std::string, std::string, translate::TranslateErrors::Type>
        translate_param;
    ChromeViewHostMsg_PageTranslated::Read(message, &translate_param);
    if (original_lang)
      *original_lang = translate_param.a;
    if (target_lang)
      *target_lang = translate_param.b;
    if (error)
      *error = translate_param.c;
    return true;
  }

  TestTranslateHelper* translate_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateHelperBrowserTest);
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

  translate_helper_->TranslatePage("en", "fr", std::string());
  base::MessageLoop::current()->RunUntilIdle();

  translate::TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(NULL, NULL, &error));
  EXPECT_EQ(translate::TranslateErrors::INITIALIZATION_ERROR, error);
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

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_helper_,
              ExecuteScriptAndGetDoubleResult(_)).Times(3);

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(original_lang, target_lang, std::string());
  base::MessageLoop::current()->RunUntilIdle();

  std::string received_original_lang;
  std::string received_target_lang;
  translate::TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
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

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_helper_,
              ExecuteScriptAndGetDoubleResult(_)).Times(2);

  translate_helper_->TranslatePage("en", "fr", std::string());
  base::MessageLoop::current()->RunUntilIdle();

  translate::TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(NULL, NULL, &error));
  EXPECT_EQ(translate::TranslateErrors::TRANSLATION_ERROR, error);
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

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_helper_,
              ExecuteScriptAndGetDoubleResult(_)).Times(3);

  translate_helper_->TranslatePage(translate::kUnknownLanguageCode,
                                   "fr",
                                   std::string());
  base::MessageLoop::current()->RunUntilIdle();

  translate::TranslateErrors::Type error;
  std::string original_lang;
  std::string target_lang;
  ASSERT_TRUE(GetPageTranslatedMessage(&original_lang, &target_lang, &error));
  EXPECT_EQ("de", original_lang);
  EXPECT_EQ("fr", target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
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

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_helper_,
              ExecuteScriptAndGetDoubleResult(_)).Times(3);

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(original_lang, target_lang, std::string());
  // While this is running call again TranslatePage to make sure noting bad
  // happens.
  translate_helper_->TranslatePage(original_lang, target_lang, std::string());
  base::MessageLoop::current()->RunUntilIdle();

  std::string received_original_lang;
  std::string received_target_lang;
  translate::TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
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

  // V8 call for performance monitoring should be ignored.
  EXPECT_CALL(*translate_helper_,
              ExecuteScriptAndGetDoubleResult(_)).Times(5);

  std::string original_lang("en");
  std::string target_lang("fr");
  translate_helper_->TranslatePage(original_lang, target_lang, std::string());
  // While this is running call again TranslatePage with a new target lang.
  std::string new_target_lang("de");
  translate_helper_->TranslatePage(
      original_lang, new_target_lang, std::string());
  base::MessageLoop::current()->RunUntilIdle();

  std::string received_original_lang;
  std::string received_target_lang;
  translate::TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(new_target_lang, received_target_lang);
  EXPECT_EQ(translate::TranslateErrors::NONE, error);
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
  EXPECT_EQ("es", params.a.adopted_language);
  render_thread_->sink().ClearMessages();

  // Makes sure we support multiple languages specified.
  LoadHTML("<html><head><meta http-equiv=\"content-language\" "
           "content=\" fr , es,en \">"
           "</head><body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a.adopted_language);
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
  EXPECT_EQ("es", params.a.adopted_language);
  render_thread_->sink().ClearMessages();

  // Makes sure we support multiple languages specified.
  LoadHTML("<html><head><meta http-equiv=\"Content-Language\" "
           "content=\" fr , es,en \">"
           "</head><body>A random page with random content.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a.adopted_language);
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
  EXPECT_EQ("en-US", params.a.adopted_language);
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
  EXPECT_EQ("zh", params.a.adopted_language);
  render_thread_->sink().ClearMessages();

  content::PageState back_state = GetCurrentPageState();

  LoadHTML("<html><head><meta http-equiv=\"content-language\" content=\"fr\">"
           "</head><body>This page is in French.</body></html>");
  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("fr", params.a.adopted_language);
  render_thread_->sink().ClearMessages();

  GoBack(back_state);

  message = render_thread_->sink().GetUniqueMessageMatching(
      ChromeViewHostMsg_TranslateLanguageDetermined::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  ChromeViewHostMsg_TranslateLanguageDetermined::Read(message, &params);
  EXPECT_EQ("zh", params.a.adopted_language);
  render_thread_->sink().ClearMessages();
}
