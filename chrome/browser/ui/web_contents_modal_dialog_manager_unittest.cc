// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class WebContentsModalDialogManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  WebContentsModalDialogManagerTest()
      : ChromeRenderViewHostTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    WebContentsModalDialogManager::CreateForWebContents(web_contents());
  }

 private:
  content::TestBrowserThread ui_thread_;
};

class WebContentsModalDialogCloseTest : public WebContentsModalDialog {
 public:
  explicit WebContentsModalDialogCloseTest(content::WebContents* web_contents,
                                           int* close_count)
      : close_count_(close_count),
        web_contents_(web_contents) {
  }

  virtual void ShowWebContentsModalDialog() OVERRIDE {}
  virtual void CloseWebContentsModalDialog() OVERRIDE {
    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(web_contents_);
    web_contents_modal_dialog_manager->WillClose(this);
    (*close_count_)++;
  }
  virtual void FocusWebContentsModalDialog() OVERRIDE {}
  virtual void PulseWebContentsModalDialog() OVERRIDE {}
  virtual NativeWebContentsModalDialog GetNativeDialog() OVERRIDE {
    // The WebContentsModalDialogManager requires a unique opaque ID for
    // the dialog, so using reinterpret_cast of this is sufficient.
    return reinterpret_cast<NativeWebContentsModalDialog>(this);
  }
  virtual ~WebContentsModalDialogCloseTest() {}

  int* close_count_;
  content::WebContents* web_contents_;
};

class NativeWebContentsModalDialogManagerCloseTest
    : public NativeWebContentsModalDialogManager {
 public:
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    reinterpret_cast<WebContentsModalDialogCloseTest*>(dialog)->
        CloseWebContentsModalDialog();
  }
  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }
};

TEST_F(WebContentsModalDialogManagerTest, WebContentsModalDialogs) {
  ScopedVector<WebContentsModalDialogCloseTest> windows;
  int close_count = 0;
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents());
  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);

  test_api.ResetNativeManager(new NativeWebContentsModalDialogManagerCloseTest);

  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++) {
    windows.push_back(
        new WebContentsModalDialogCloseTest(web_contents(), &close_count));
    web_contents_modal_dialog_manager->AddDialog(windows.back());
  }
  EXPECT_EQ(0, close_count);

  test_api.CloseAllDialogs();
  EXPECT_EQ(kWindowCount, close_count);
}
