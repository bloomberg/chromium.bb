// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/test/test_browser_thread.h"

using content::BrowserThread;
using content::WebContents;

class TestPrefsTabHelper : public PrefsTabHelper {
 public:
  explicit TestPrefsTabHelper(WebContents* web_contents)
      : PrefsTabHelper(web_contents),
        was_update_web_preferences_called_(false) {
  }
  virtual ~TestPrefsTabHelper() { }

  virtual void UpdateWebPreferences() OVERRIDE {
    was_update_web_preferences_called_ = true;
    PrefsTabHelper::UpdateWebPreferences();
  }

  void NotifyRenderViewCreated() {
    RenderViewCreated(NULL);
  }

  bool was_update_web_preferences_called() {
    return was_update_web_preferences_called_;
  }

 private:
  bool was_update_web_preferences_called_;
};

class PrefsTabHelperTest : public TabContentsWrapperTestHarness {
 public:
  PrefsTabHelperTest()
      : TabContentsWrapperTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual ~PrefsTabHelperTest() {}

  TabContentsWrapper* contents_wrapper2() {
    return contents_wrapper2_.get();
  }

  TestPrefsTabHelper* CreateTestPrefsTabHelper() {
    TestPrefsTabHelper* test_prefs_helper =
        new TestPrefsTabHelper(contents_wrapper()->web_contents());
    contents_wrapper()->prefs_tab_helper_.reset(test_prefs_helper);
    return test_prefs_helper;
  }

  void SetContents2(TestTabContents* contents) {
    contents_wrapper2_.reset(
        contents ? new TabContentsWrapper(contents) : NULL);
  }

  void TestBooleanPreference(const char* key) {
    PrefService* prefs1 =
        contents_wrapper()->prefs_tab_helper()->per_tab_prefs();
    PrefService* prefs2 =
        contents_wrapper2()->prefs_tab_helper()->per_tab_prefs();
    const bool initial_value = prefs1->GetBoolean(key);
    EXPECT_EQ(initial_value, prefs2->GetBoolean(key));

    prefs1->SetBoolean(key, !initial_value);
    EXPECT_EQ(!initial_value, prefs1->GetBoolean(key));
    EXPECT_EQ(initial_value, prefs2->GetBoolean(key));

    prefs1->SetBoolean(key, initial_value);
    EXPECT_EQ(initial_value, prefs1->GetBoolean(key));
    EXPECT_EQ(initial_value, prefs2->GetBoolean(key));

    prefs2->SetBoolean(key, !initial_value);
    EXPECT_EQ(initial_value, prefs1->GetBoolean(key));
    EXPECT_EQ(!initial_value, prefs2->GetBoolean(key));

    prefs1->SetBoolean(key, !initial_value);
    EXPECT_EQ(!initial_value, prefs1->GetBoolean(key));
    EXPECT_EQ(!initial_value, prefs2->GetBoolean(key));
  }

  void TestIntegerPreference(const char* key) {
    PrefService* prefs1 =
        contents_wrapper()->prefs_tab_helper()->per_tab_prefs();
    PrefService* prefs2 =
        contents_wrapper2()->prefs_tab_helper()->per_tab_prefs();
    const int initial_value = prefs1->GetInteger(key);
    EXPECT_EQ(initial_value, prefs2->GetInteger(key));

    const int modified_value = initial_value + 1;
    prefs1->SetInteger(key, modified_value);
    EXPECT_EQ(modified_value, prefs1->GetInteger(key));
    EXPECT_EQ(initial_value, prefs2->GetInteger(key));

    prefs1->SetInteger(key, initial_value);
    EXPECT_EQ(initial_value, prefs1->GetInteger(key));
    EXPECT_EQ(initial_value, prefs2->GetInteger(key));

    prefs2->SetInteger(key, modified_value);
    EXPECT_EQ(initial_value, prefs1->GetInteger(key));
    EXPECT_EQ(modified_value, prefs2->GetInteger(key));

    prefs1->SetInteger(key, modified_value);
    EXPECT_EQ(modified_value, prefs1->GetInteger(key));
    EXPECT_EQ(modified_value, prefs2->GetInteger(key));
  }

  void TestStringPreference(const char* key) {
    PrefService* prefs1 =
        contents_wrapper()->prefs_tab_helper()->per_tab_prefs();
    PrefService* prefs2 =
        contents_wrapper2()->prefs_tab_helper()->per_tab_prefs();
    const std::string initial_value = prefs1->GetString(key);
    EXPECT_EQ(initial_value, prefs2->GetString(key));

    const std::string modified_value = initial_value + "_";
    prefs1->SetString(key, modified_value);
    EXPECT_EQ(modified_value, prefs1->GetString(key));
    EXPECT_EQ(initial_value, prefs2->GetString(key));

    prefs1->SetString(key, initial_value);
    EXPECT_EQ(initial_value, prefs1->GetString(key));
    EXPECT_EQ(initial_value, prefs2->GetString(key));

    prefs2->SetString(key, modified_value);
    EXPECT_EQ(initial_value, prefs1->GetString(key));
    EXPECT_EQ(modified_value, prefs2->GetString(key));

    prefs1->SetString(key, modified_value);
    EXPECT_EQ(modified_value, prefs1->GetString(key));
    EXPECT_EQ(modified_value, prefs2->GetString(key));
  }

 protected:
  virtual void SetUp() OVERRIDE {
    TabContentsWrapperTestHarness::SetUp();
    SetContents2(CreateTestTabContents());
  }

  virtual void TearDown() OVERRIDE {
    contents_wrapper2_.reset();
    TabContentsWrapperTestHarness::TearDown();
  }

 private:
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TabContentsWrapper> contents_wrapper2_;

  DISALLOW_COPY_AND_ASSIGN(PrefsTabHelperTest);
};

TEST_F(PrefsTabHelperTest, PerTabJavaScriptEnabled) {
  TestBooleanPreference(prefs::kWebKitJavascriptEnabled);
}

TEST_F(PrefsTabHelperTest, PerTabJavascriptCanOpenWindowsAutomatically) {
  TestBooleanPreference(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
}

TEST_F(PrefsTabHelperTest, PerTabLoadsImagesAutomatically) {
  TestBooleanPreference(prefs::kWebKitLoadsImagesAutomatically);
}

TEST_F(PrefsTabHelperTest, PerTabPluginsEnabled) {
  TestBooleanPreference(prefs::kWebKitPluginsEnabled);
}

TEST_F(PrefsTabHelperTest, PerTabDefaultFontSize) {
  TestIntegerPreference(prefs::kWebKitDefaultFontSize);
}

TEST_F(PrefsTabHelperTest, PerTabDefaultFixedFontSize) {
  TestIntegerPreference(prefs::kWebKitDefaultFixedFontSize);
}

TEST_F(PrefsTabHelperTest, PerTabMinimumFontSize) {
  TestIntegerPreference(prefs::kWebKitMinimumFontSize);
}

TEST_F(PrefsTabHelperTest, PerTabMinimumLogicalFontSize) {
  TestIntegerPreference(prefs::kWebKitMinimumLogicalFontSize);
}

TEST_F(PrefsTabHelperTest, PerTabDefaultCharset) {
  TestStringPreference(prefs::kDefaultCharset);
}

TEST_F(PrefsTabHelperTest, PerTabDefaultCharsetUpdate) {
  TestPrefsTabHelper* test_prefs_helper = CreateTestPrefsTabHelper();
  EXPECT_FALSE(test_prefs_helper->was_update_web_preferences_called());
  const char* pref = prefs::kDefaultCharset;
  PrefService* prefs =
      contents_wrapper()->prefs_tab_helper()->per_tab_prefs();
  prefs->SetString(pref, prefs->GetString(pref) + "_");
  EXPECT_TRUE(test_prefs_helper->was_update_web_preferences_called());
}

TEST_F(PrefsTabHelperTest, PerTabStandardFontFamily) {
  TestStringPreference(prefs::kWebKitStandardFontFamily);
}

TEST_F(PrefsTabHelperTest, PerTabFixedFontFamily) {
  TestStringPreference(prefs::kWebKitFixedFontFamily);
}

TEST_F(PrefsTabHelperTest, PerTabSerifFontFamily) {
  TestStringPreference(prefs::kWebKitSerifFontFamily);
}

TEST_F(PrefsTabHelperTest, PerTabSansSerifFontFamily) {
  TestStringPreference(prefs::kWebKitSansSerifFontFamily);
}

TEST_F(PrefsTabHelperTest, PerTabCursiveFontFamily) {
  TestStringPreference(prefs::kWebKitCursiveFontFamily);
}

TEST_F(PrefsTabHelperTest, PerTabFantasyFontFamily) {
  TestStringPreference(prefs::kWebKitFantasyFontFamily);
}

TEST_F(PrefsTabHelperTest, OverridePrefsOnViewCreation) {
  TestPrefsTabHelper* test_prefs_helper = CreateTestPrefsTabHelper();
  EXPECT_FALSE(test_prefs_helper->was_update_web_preferences_called());
  test_prefs_helper->NotifyRenderViewCreated();
  EXPECT_TRUE(test_prefs_helper->was_update_web_preferences_called());
}
