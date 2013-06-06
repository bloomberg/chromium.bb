// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/native_web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace web_modal {

class WebContentsModalDialogManagerTest
    : public content::RenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    content::RenderViewHostTestHarness::SetUp();
    WebContentsModalDialogManager::CreateForWebContents(web_contents());
  }
};

class NativeWebContentsModalDialogManagerCloseTest
    : public NativeWebContentsModalDialogManager {
 public:
  NativeWebContentsModalDialogManagerCloseTest(
      NativeWebContentsModalDialogManagerDelegate* delegate)
      : delegate_(delegate) {}
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    delegate_->WillClose(dialog);
    close_count++;
  }
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  int close_count;
  NativeWebContentsModalDialogManagerDelegate* delegate_;
};

NativeWebContentsModalDialogManager* WebContentsModalDialogManager::
CreateNativeManager(
    NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerCloseTest(native_delegate);
}

TEST_F(WebContentsModalDialogManagerTest, WebContentsModalDialogs) {
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents());
  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);

  NativeWebContentsModalDialogManagerCloseTest* native_manager =
      new NativeWebContentsModalDialogManagerCloseTest(
          web_contents_modal_dialog_manager);
  native_manager->close_count = 0;

  test_api.ResetNativeManager(native_manager);

  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++)
    // WebContentsModalDialogManager treats the NativeWebContentsModalDialog as
    // an opaque type, so creating fake NativeWebContentsModalDialogs using
    // reinterpret_cast is valid.
    web_contents_modal_dialog_manager->ShowDialog(
        reinterpret_cast<NativeWebContentsModalDialog>(i));
  EXPECT_EQ(native_manager->close_count, 0);

  test_api.CloseAllDialogs();
  EXPECT_EQ(native_manager->close_count, kWindowCount);
}

}  // namespace web_modal
