// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/language_state.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/language_state_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockLanguageStateObserver : public LanguageStateObserver {
 public:
  MockLanguageStateObserver()
      : on_is_page_translated_changed_called_(false),
        on_translate_enabled_changed_called_(false) {
  }

  virtual ~MockLanguageStateObserver() {}

  virtual void OnIsPageTranslatedChanged(
      content::WebContents* source) OVERRIDE {
    on_is_page_translated_changed_called_ = true;
  }

  virtual void OnTranslateEnabledChanged(
      content::WebContents* source) OVERRIDE {
    on_translate_enabled_changed_called_ = true;
  }

  bool on_is_page_translated_changed_called() const {
    return on_is_page_translated_changed_called_;
  }

  bool on_translate_enabled_changed_called() const {
    return on_translate_enabled_changed_called_;
  }

 private:
  bool on_is_page_translated_changed_called_;
  bool on_translate_enabled_changed_called_;

  DISALLOW_COPY_AND_ASSIGN(MockLanguageStateObserver);
};

}  // namespace

typedef BrowserWithTestWindowTest LanguageStateTest;

TEST_F(LanguageStateTest, IsPageTranslated) {
  LanguageState language_state(NULL);
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

TEST_F(LanguageStateTest, Observer) {
  GURL url("http://foo/");
  AddTab(browser(), url);
  content::NavigationController& navigation_controller =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();

  scoped_ptr<MockLanguageStateObserver> observer(
      new MockLanguageStateObserver);
  LanguageState language_state(&navigation_controller);
  language_state.set_observer(observer.get());

  // Enable/Disable translate.
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(observer->on_translate_enabled_changed_called());
  language_state.SetTranslateEnabled(true);
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(observer->on_translate_enabled_changed_called());

  observer.reset(new MockLanguageStateObserver);
  language_state.set_observer(observer.get());
  language_state.SetTranslateEnabled(false);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_TRUE(observer->on_translate_enabled_changed_called());

  // Navigate to a French page.
  observer.reset(new MockLanguageStateObserver);
  language_state.set_observer(observer.get());
  language_state.LanguageDetermined("fr", true);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(observer->on_is_page_translated_changed_called());
  EXPECT_FALSE(observer->on_translate_enabled_changed_called());

  // Translate.
  language_state.SetCurrentLanguage("en");
  EXPECT_TRUE(language_state.IsPageTranslated());
  EXPECT_TRUE(observer->on_is_page_translated_changed_called());

  // Translate feature must be enabled after an actual translation.
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(observer->on_translate_enabled_changed_called());
}
