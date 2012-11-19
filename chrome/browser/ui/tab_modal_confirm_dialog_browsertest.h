// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"

class MockTabModalConfirmDialogDelegate;

class TabModalConfirmDialogTest : public InProcessBrowserTest {
 public:
  TabModalConfirmDialogTest();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

 protected:
  // Owned by |dialog_|.
  MockTabModalConfirmDialogDelegate* delegate_;

  // Deletes itself.
  TabModalConfirmDialog* dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogTest);
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
