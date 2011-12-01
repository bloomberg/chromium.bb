// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/tab_contents/per_tab_prefs_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/test/test_browser_thread.h"

using content::BrowserThread;

class TestPerTabPrefsTabHelper : public PerTabPrefsTabHelper {
 public:
  explicit TestPerTabPrefsTabHelper(TabContentsWrapper* tab_contents)
      : PerTabPrefsTabHelper(tab_contents),
        was_override_web_prefernces_called_(false) {
  }
  virtual ~TestPerTabPrefsTabHelper() { }

  virtual void OverrideWebPreferences(WebPreferences* prefs) OVERRIDE {
    was_override_web_prefernces_called_ = true;
    PerTabPrefsTabHelper::OverrideWebPreferences(prefs);
  }

  void NotifyRenderViewCreated() {
    RenderViewCreated(NULL);
  }

  bool was_override_web_prefernces_called() {
    return was_override_web_prefernces_called_;
  }

 private:
  bool was_override_web_prefernces_called_;
};

class PerTabPrefsTabHelperTest : public TabContentsWrapperTestHarness {
 public:
  PerTabPrefsTabHelperTest()
      : TabContentsWrapperTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual ~PerTabPrefsTabHelperTest() {}

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

  DISALLOW_COPY_AND_ASSIGN(PerTabPrefsTabHelperTest);
};

TEST_F(PerTabPrefsTabHelperTest, PerTabJavaScriptEnabled) {
  const char* key = prefs::kWebKitJavascriptEnabled;
  PrefService* prefs1 = contents_wrapper()->per_tab_prefs_tab_helper()->prefs();
  PrefService* prefs2 =
      contents_wrapper2()->per_tab_prefs_tab_helper()->prefs();
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

TEST_F(PerTabPrefsTabHelperTest, OverridePrefsOnViewCreation) {
  TestPerTabPrefsTabHelper* test_prefs_helper = new TestPerTabPrefsTabHelper(
      contents_wrapper());
  contents_wrapper()->per_tab_prefs_tab_helper_.reset(test_prefs_helper);
  EXPECT_EQ(false, test_prefs_helper->was_override_web_prefernces_called());
  test_prefs_helper->NotifyRenderViewCreated();
  EXPECT_EQ(true, test_prefs_helper->was_override_web_prefernces_called());
}
