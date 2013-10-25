// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include <map>

#include "base/memory/scoped_ptr.h"
#include "components/web_modal/native_web_contents_modal_dialog_manager.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_modal {

class TestNativeWebContentsModalDialogManager
    : public NativeWebContentsModalDialogManager {
 public:
  enum DialogState {
    UNKNOWN,
    NOT_SHOWN,
    SHOWN,
    HIDDEN,
    CLOSED
  };

  explicit TestNativeWebContentsModalDialogManager(
      NativeWebContentsModalDialogManagerDelegate* delegate)
      : delegate_(delegate) {}
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    dialog_state_[dialog] = NOT_SHOWN;
  }
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    dialog_state_[dialog] = SHOWN;
  }
  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    dialog_state_[dialog] = HIDDEN;
  }
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    delegate_->WillClose(dialog);
    dialog_state_[dialog] = CLOSED;
  }
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void HostChanged(WebContentsModalDialogHost* new_host) OVERRIDE {
  }

  int GetCloseCount() const {
    int count = 0;
    for (DialogStateMap::const_iterator it = dialog_state_.begin();
         it != dialog_state_.end(); ++it) {
      if (it->second == CLOSED)
        count++;
    }
    return count;
  }

  DialogState GetDialogState(NativeWebContentsModalDialog dialog) const {
    DialogStateMap::const_iterator loc = dialog_state_.find(dialog);
    return loc == dialog_state_.end() ? UNKNOWN : loc->second;
  }

 private:
  typedef std::map<NativeWebContentsModalDialog, DialogState> DialogStateMap;

  NativeWebContentsModalDialogManagerDelegate* delegate_;
  DialogStateMap dialog_state_;

  DISALLOW_COPY_AND_ASSIGN(TestNativeWebContentsModalDialogManager);
};

class WebContentsModalDialogManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  WebContentsModalDialogManagerTest()
      : next_dialog_id(1),
        manager(NULL),
        native_manager(NULL) {
  }

  virtual void SetUp() {
    content::RenderViewHostTestHarness::SetUp();

    delegate.reset(new TestWebContentsModalDialogManagerDelegate);
    WebContentsModalDialogManager::CreateForWebContents(web_contents());
    manager = WebContentsModalDialogManager::FromWebContents(web_contents());
    manager->SetDelegate(delegate.get());
    test_api.reset(new WebContentsModalDialogManager::TestApi(manager));
    native_manager = new TestNativeWebContentsModalDialogManager(manager);

    // |manager| owns |native_manager| as a result.
    test_api->ResetNativeManager(native_manager);
  }

  virtual void TearDown() {
    test_api.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  NativeWebContentsModalDialog MakeFakeDialog() {
    // WebContentsModalDialogManager treats the NativeWebContentsModalDialog as
    // an opaque type, so creating fake NativeWebContentsModalDialogs using
    // reinterpret_cast is valid.
    return reinterpret_cast<NativeWebContentsModalDialog>(next_dialog_id++);
  }

  int next_dialog_id;
  scoped_ptr<TestWebContentsModalDialogManagerDelegate> delegate;
  WebContentsModalDialogManager* manager;
  scoped_ptr<WebContentsModalDialogManager::TestApi> test_api;
  TestNativeWebContentsModalDialogManager* native_manager;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogManagerTest);
};

NativeWebContentsModalDialogManager*
WebContentsModalDialogManager::CreateNativeManager(
    NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new TestNativeWebContentsModalDialogManager(native_delegate);
}

// Test that the dialog is shown immediately when the delegate indicates the web
// contents is visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsVisible) {
  // Dialog should be shown while WebContents is visible.
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();

  manager->ShowDialog(dialog1);

  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog1));
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
}

// Test that the dialog is not shown immediately when the delegate indicates the
// web contents is not visible.
TEST_F(WebContentsModalDialogManagerTest, WebContentsNotVisible) {
  // Dialog should not be shown while WebContents is not visible.
  delegate->set_web_contents_visible(false);

  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();

  manager->ShowDialog(dialog1);

  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog1));
  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
}

// Test that only the first of multiple dialogs is shown.
TEST_F(WebContentsModalDialogManagerTest, ShowDialogs) {
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();

  manager->ShowDialog(dialog1);
  manager->ShowDialog(dialog2);
  manager->ShowDialog(dialog3);

  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog1));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog2));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog3));
}

// Test that the dialog is shown/hidden when the WebContents is shown/hidden.
TEST_F(WebContentsModalDialogManagerTest, VisibilityObservation) {
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();

  manager->ShowDialog(dialog1);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog1));

  test_api->WebContentsWasHidden();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::HIDDEN,
            native_manager->GetDialogState(dialog1));

  test_api->WebContentsWasShown();

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog1));
}

// Test that attaching an interstitial page closes dialogs configured to close.
TEST_F(WebContentsModalDialogManagerTest, InterstitialPage) {
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();

  manager->ShowDialog(dialog1);
  manager->ShowDialog(dialog2);
  manager->ShowDialog(dialog3);

#if defined(OS_WIN) || defined(USE_AURA)
  manager->SetCloseOnInterstitialPage(dialog2, false);
#else
  // TODO(wittman): Remove this section once Mac is changed to close on
  // interstitial pages by default.
  manager->SetCloseOnInterstitialPage(dialog1, true);
  manager->SetCloseOnInterstitialPage(dialog3, true);
#endif

  test_api->DidAttachInterstitialPage();
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog1));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog2));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog3));
}


// Test that the first dialog is always shown, regardless of the order in which
// dialogs are closed.
TEST_F(WebContentsModalDialogManagerTest, CloseDialogs) {
  // The front dialog is always shown regardless of dialog close order.
  const NativeWebContentsModalDialog dialog1 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog2 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog3 = MakeFakeDialog();
  const NativeWebContentsModalDialog dialog4 = MakeFakeDialog();

  manager->ShowDialog(dialog1);
  manager->ShowDialog(dialog2);
  manager->ShowDialog(dialog3);
  manager->ShowDialog(dialog4);

  native_manager->CloseDialog(dialog1);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog1));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog2));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog3));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog4));

  native_manager->CloseDialog(dialog3);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog2));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog3));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::NOT_SHOWN,
            native_manager->GetDialogState(dialog4));

  native_manager->CloseDialog(dialog2);

  EXPECT_TRUE(manager->IsDialogActive());
  EXPECT_TRUE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog2));
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::SHOWN,
            native_manager->GetDialogState(dialog4));

  native_manager->CloseDialog(dialog4);

  EXPECT_FALSE(manager->IsDialogActive());
  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_EQ(TestNativeWebContentsModalDialogManager::CLOSED,
            native_manager->GetDialogState(dialog4));
}

// Test that CloseAllDialogs does what it says.
TEST_F(WebContentsModalDialogManagerTest, CloseAllDialogs) {
  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++)
    manager->ShowDialog(MakeFakeDialog());

  EXPECT_EQ(0, native_manager->GetCloseCount());

  test_api->CloseAllDialogs();
  EXPECT_FALSE(delegate->web_contents_blocked());
  EXPECT_FALSE(manager->IsDialogActive());
  EXPECT_EQ(kWindowCount, native_manager->GetCloseCount());
}

}  // namespace web_modal
