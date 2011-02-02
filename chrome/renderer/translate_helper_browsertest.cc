// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Return;

class TestTranslateHelper : public TranslateHelper {
 public:
  explicit TestTranslateHelper(RenderView* render_view)
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

class TranslateHelperTest : public RenderViewTest {
 public:
  TranslateHelperTest() : translate_helper_(NULL) {}

 protected:
  virtual void SetUp() {
    RenderViewTest::SetUp();
    translate_helper_ = new TestTranslateHelper(view_);
  }

  bool GetPageTranslatedMessage(int* page_id,
                                std::string* original_lang,
                                std::string* target_lang,
                                TranslateErrors::Type* error) {
    const IPC::Message* message = render_thread_.sink().
        GetUniqueMessageMatching(ViewHostMsg_PageTranslated::ID);
    if (!message)
      return false;
    Tuple4<int, std::string, std::string, TranslateErrors::Type>
        translate_param;
    ViewHostMsg_PageTranslated::Read(message, &translate_param);
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
TEST_F(TranslateHelperTest, TranslateLibNeverReady) {
  // We make IsTranslateLibAvailable true so we don't attempt to inject the
  // library.
  EXPECT_CALL(*translate_helper_, IsTranslateLibAvailable())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*translate_helper_, IsTranslateLibReady())
      .Times(AtLeast(5))  // See kMaxTranslateInitCheckAttempts in
                          // translate_helper.cc
      .WillRepeatedly(Return(false));

  translate_helper_->TranslatePage(view_->page_id(), "en", "fr", std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, NULL, NULL, &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ(TranslateErrors::INITIALIZATION_ERROR, error);
}

// Tests that the browser gets notified of the translation success when the
// translation succeeds.
TEST_F(TranslateHelperTest, TranslateSuccess) {
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
  translate_helper_->TranslatePage(view_->page_id(), original_lang, target_lang,
                                   std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that the browser gets notified of the translation failure when the
// translation fails.
TEST_F(TranslateHelperTest, TranslateFailure) {
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

  translate_helper_->TranslatePage(view_->page_id(), "en", "fr", std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, NULL, NULL, &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ(TranslateErrors::TRANSLATION_ERROR, error);
}

// Tests that when the browser translate a page for which the language is
// undefined we query the translate element to get the language.
TEST_F(TranslateHelperTest, UndefinedSourceLang) {
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

  translate_helper_->TranslatePage(view_->page_id(),
                                   chrome::kUnknownLanguageCode, "fr",
                                   std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  TranslateErrors::Type error;
  std::string original_lang;
  std::string target_lang;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id, &original_lang, &target_lang,
                                       &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ("de", original_lang);
  EXPECT_EQ("fr", target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that starting a translation while a similar one is pending does not
// break anything.
TEST_F(TranslateHelperTest, MultipleSimilarTranslations) {
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
  translate_helper_->TranslatePage(view_->page_id(), original_lang, target_lang,
                                   std::string());
  // While this is running call again TranslatePage to make sure noting bad
  // happens.
  translate_helper_->TranslatePage(view_->page_id(), original_lang, target_lang,
                                   std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}

// Tests that starting a translation while a different one is pending works.
TEST_F(TranslateHelperTest, MultipleDifferentTranslations) {
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
  translate_helper_->TranslatePage(view_->page_id(), original_lang, target_lang,
                                   std::string());
  // While this is running call again TranslatePage with a new target lang.
  std::string new_target_lang("de");
  translate_helper_->TranslatePage(view_->page_id(), original_lang,
                                   new_target_lang, std::string());
  MessageLoop::current()->RunAllPending();

  int page_id;
  std::string received_original_lang;
  std::string received_target_lang;
  TranslateErrors::Type error;
  ASSERT_TRUE(GetPageTranslatedMessage(&page_id,
                                       &received_original_lang,
                                       &received_target_lang,
                                       &error));
  EXPECT_EQ(view_->page_id(), page_id);
  EXPECT_EQ(original_lang, received_original_lang);
  EXPECT_EQ(new_target_lang, received_target_lang);
  EXPECT_EQ(TranslateErrors::NONE, error);
}
