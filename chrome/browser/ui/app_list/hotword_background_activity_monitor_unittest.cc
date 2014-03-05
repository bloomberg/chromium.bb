// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/hotword_background_activity_monitor.h"

#include "chrome/browser/ui/app_list/hotword_background_activity_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

namespace app_list {

class HotwordBackgroundActivityMonitorTest
    : public BrowserWithTestWindowTest,
      public HotwordBackgroundActivityDelegate {
 public:
  HotwordBackgroundActivityMonitorTest() : changed_count_(0) {}
  virtual ~HotwordBackgroundActivityMonitorTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    monitor_.reset(new HotwordBackgroundActivityMonitor(this));
  }
  virtual void TearDown() OVERRIDE {
    monitor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  int GetChangedCountAndReset() {
    int count = changed_count_;
    changed_count_ = 0;
    return count;
  }

  bool IsHotwordBackgroundActive() {
    return monitor_->IsHotwordBackgroundActive();
  }

  // Invokes the idle test explicitly.
  // TODO(mukai): make IdleDetector testable.
  void SetIdleState(bool is_idle) {
    monitor_->OnIdleStateChanged(is_idle);
  }

  void UpdateMediaRequest(int render_process_id, bool requesting) {
    monitor_->OnRequestUpdate(
        render_process_id,
        0 /* don't care render_view_id */,
        content::MediaStreamDevice() /* don't care the device */,
        requesting ? content::MEDIA_REQUEST_STATE_REQUESTED :
        content::MEDIA_REQUEST_STATE_CLOSING);
  }

  void AddWebContentsToWhitelist(content::WebContents* contents) {
    monitor_->AddWebContentsToWhitelistForTest(contents);
  }

  void RemoveWebContentsFromWhitelist(content::WebContents* contents) {
    monitor_->RemoveWebContentsFromWhitelistForTest(contents);
  }

  // HotwordBackgroundActivityDelegate overrides:
  virtual int GetRenderProcessID() OVERRIDE {
    // Use the invalid id for tests, because it cannot overlap with any existing
    // ids.
    return content::ChildProcessHost::kInvalidUniqueID;
  }

 private:
  // HotwordBackgroundActivityDelegate overrides:
  virtual void OnHotwordBackgroundActivityChanged() OVERRIDE {
    ++changed_count_;
  }

  int changed_count_;
  scoped_ptr<HotwordBackgroundActivityMonitor> monitor_;

  DISALLOW_COPY_AND_ASSIGN(HotwordBackgroundActivityMonitorTest);
};

#if defined(USE_ASH)
TEST_F(HotwordBackgroundActivityMonitorTest, LockStateTest) {
  if (!ash::Shell::HasInstance())
    return;

  ASSERT_TRUE(IsHotwordBackgroundActive());

  ash::Shell::GetInstance()->OnLockStateChanged(true);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  ash::Shell::GetInstance()->OnLockStateChanged(false);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());
}
#endif

TEST_F(HotwordBackgroundActivityMonitorTest, IdleStateTest) {
  ASSERT_TRUE(IsHotwordBackgroundActive());

  SetIdleState(true);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

#if defined(USE_ASH)
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->OnLockStateChanged(true);
    EXPECT_EQ(1, GetChangedCountAndReset());
    EXPECT_FALSE(IsHotwordBackgroundActive());

    ash::Shell::GetInstance()->OnLockStateChanged(false);
    EXPECT_EQ(1, GetChangedCountAndReset());
    EXPECT_FALSE(IsHotwordBackgroundActive());
  }
#endif

  SetIdleState(false);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());
}

TEST_F(HotwordBackgroundActivityMonitorTest, MediaRequests) {
  ASSERT_TRUE(IsHotwordBackgroundActive());

  // Request from itself should be ignored.
  UpdateMediaRequest(GetRenderProcessID(), true);
  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());

  int id1 = GetRenderProcessID() + 1;
  int id2 = GetRenderProcessID() + 2;

  UpdateMediaRequest(id1, true);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  UpdateMediaRequest(id2, true);
  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  UpdateMediaRequest(id2, false);
  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  UpdateMediaRequest(id1, false);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());
}

TEST_F(HotwordBackgroundActivityMonitorTest, TabTests) {
  ASSERT_TRUE(IsHotwordBackgroundActive());

  content::WebContents* contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddWebContents(
      contents, 0, content::PAGE_TRANSITION_LINK, TabStripModel::ADD_ACTIVE);
  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());

  AddWebContentsToWhitelist(contents);
  tab_strip_model->UpdateWebContentsStateAt(0, TabStripModelObserver::ALL);

  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  // Tab could be closed without UpdateWebContentsStateAt().
  RemoveWebContentsFromWhitelist(contents);
  tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());
}

TEST_F(HotwordBackgroundActivityMonitorTest, MediaAndTabCombined) {
  ASSERT_TRUE(IsHotwordBackgroundActive());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  int contents_id = contents->GetRenderProcessHost()->GetID();
  int extension_id = contents_id + 1;
  ASSERT_NE(GetRenderProcessID(), extension_id);

  tab_strip_model->AddWebContents(
      contents, 0, content::PAGE_TRANSITION_LINK, TabStripModel::ADD_ACTIVE);
  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());

  // Another recognizer for the tab has started.
  UpdateMediaRequest(extension_id, true);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  // Hotword is recognized -- switch to the speech recognition in the tab.
  AddWebContentsToWhitelist(contents);
  UpdateMediaRequest(contents_id, true);
  UpdateMediaRequest(extension_id, false);
  tab_strip_model->UpdateWebContentsStateAt(0, TabStripModelObserver::ALL);

  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  // The speech recognition has ended.
  RemoveWebContentsFromWhitelist(contents);
  tab_strip_model->UpdateWebContentsStateAt(0, TabStripModelObserver::ALL);
  UpdateMediaRequest(extension_id, true);

  EXPECT_EQ(2, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  // Switches to the speech recognition state again.
  AddWebContentsToWhitelist(contents);
  UpdateMediaRequest(contents_id, true);
  UpdateMediaRequest(extension_id, false);
  tab_strip_model->UpdateWebContentsStateAt(0, TabStripModelObserver::ALL);

  EXPECT_EQ(0, GetChangedCountAndReset());
  EXPECT_FALSE(IsHotwordBackgroundActive());

  // And then close the tab during the speech recognition.
  tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  EXPECT_EQ(1, GetChangedCountAndReset());
  EXPECT_TRUE(IsHotwordBackgroundActive());
}

}  // namespace app_list
