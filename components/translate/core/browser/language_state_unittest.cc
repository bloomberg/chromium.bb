// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/language_state.h"

#include "base/memory/scoped_ptr.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

namespace {

const std::string kHtmlMimeType = "text/html";

class MockTranslateDriver : public TranslateDriver {
 public:
  MockTranslateDriver()
      : on_is_page_translated_changed_called_(false),
        on_translate_enabled_changed_called_(false),
        language_state_(this) {
  }

  void Reset() {
    on_is_page_translated_changed_called_ = false;
    on_translate_enabled_changed_called_ = false;
  }

  virtual ~MockTranslateDriver() {}

  virtual void OnIsPageTranslatedChanged() OVERRIDE {
    on_is_page_translated_changed_called_ = true;
  }

  virtual void OnTranslateEnabledChanged() OVERRIDE {
    on_translate_enabled_changed_called_ = true;
  }

  virtual bool IsLinkNavigation() OVERRIDE {
    return false;
  }

  virtual void TranslatePage(int page_seq_no,
                             const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang) OVERRIDE {}

  virtual void RevertTranslation(int page_seq_no) OVERRIDE {}

  virtual bool IsOffTheRecord() OVERRIDE { return false; }

  virtual const std::string& GetContentsMimeType() OVERRIDE {
    return kHtmlMimeType;
  }

  virtual const GURL& GetLastCommittedURL() OVERRIDE {
    return GURL::EmptyGURL();
  }

  virtual const GURL& GetActiveURL() OVERRIDE { return GURL::EmptyGURL(); }

  virtual const GURL& GetVisibleURL() OVERRIDE { return GURL::EmptyGURL(); }

  virtual bool HasCurrentPage() OVERRIDE { return true; }

  virtual void OpenUrlInNewTab(const GURL& url) OVERRIDE {}

  bool on_is_page_translated_changed_called() const {
    return on_is_page_translated_changed_called_;
  }

  bool on_translate_enabled_changed_called() const {
    return on_translate_enabled_changed_called_;
  }

 private:
  bool on_is_page_translated_changed_called_;
  bool on_translate_enabled_changed_called_;
  LanguageState language_state_;

  DISALLOW_COPY_AND_ASSIGN(MockTranslateDriver);
};

}  // namespace

TEST(LanguageStateTest, IsPageTranslated) {
  scoped_ptr<MockTranslateDriver> driver(
      new MockTranslateDriver);
  LanguageState language_state(driver.get());
  EXPECT_FALSE(language_state.IsPageTranslated());

  // Navigate to a French page.
  language_state.LanguageDetermined("fr", true);
  EXPECT_EQ("fr", language_state.original_language());
  EXPECT_EQ("fr", language_state.current_language());
  EXPECT_FALSE(language_state.IsPageTranslated());

  // Translate the page into English.
  language_state.SetCurrentLanguage("en");
  EXPECT_EQ("fr", language_state.original_language());
  EXPECT_EQ("en", language_state.current_language());
  EXPECT_TRUE(language_state.IsPageTranslated());

  // Move on another page in Japanese.
  language_state.LanguageDetermined("ja", true);
  EXPECT_EQ("ja", language_state.original_language());
  EXPECT_EQ("ja", language_state.current_language());
  EXPECT_FALSE(language_state.IsPageTranslated());
}

TEST(LanguageStateTest, Driver) {

  scoped_ptr<MockTranslateDriver> driver(
      new MockTranslateDriver);
  LanguageState language_state(driver.get());

  // Enable/Disable translate.
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(driver->on_translate_enabled_changed_called());
  language_state.SetTranslateEnabled(true);
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(driver->on_translate_enabled_changed_called());

  driver->Reset();
  language_state.SetTranslateEnabled(false);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_TRUE(driver->on_translate_enabled_changed_called());

  // Navigate to a French page.
  driver->Reset();
  language_state.LanguageDetermined("fr", true);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(driver->on_is_page_translated_changed_called());
  EXPECT_FALSE(driver->on_translate_enabled_changed_called());

  // Translate.
  language_state.SetCurrentLanguage("en");
  EXPECT_TRUE(language_state.IsPageTranslated());
  EXPECT_TRUE(driver->on_is_page_translated_changed_called());

  // Translate feature must be enabled after an actual translation.
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(driver->on_translate_enabled_changed_called());
}

}  // namespace translate
