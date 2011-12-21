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

class TestPrefsTabHelper : public PrefsTabHelper {
 public:
  explicit TestPrefsTabHelper(TabContents* tab_contents)
      : PrefsTabHelper(tab_contents),
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

  void SetContents2(TestTabContents* contents) {
    contents_wrapper2_.reset(
        contents ? new TabContentsWrapper(contents) : NULL);
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
  const char* key = prefs::kWebKitJavascriptEnabled;
  PrefService* prefs1 = contents_wrapper()->prefs_tab_helper()->per_tab_prefs();
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
}

TEST_F(PrefsTabHelperTest, OverridePrefsOnViewCreation) {
  TestPrefsTabHelper* test_prefs_helper =
      new TestPrefsTabHelper(contents_wrapper()->tab_contents());
  contents_wrapper()->prefs_tab_helper_.reset(test_prefs_helper);
  EXPECT_FALSE(test_prefs_helper->was_update_web_preferences_called());
  test_prefs_helper->NotifyRenderViewCreated();
  EXPECT_TRUE(test_prefs_helper->was_update_web_preferences_called());
}
