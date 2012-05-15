// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#pragma once

#include "chrome/test/base/in_process_browser_test.h"
#include "content/test/test_browser_thread.h"

#if defined(OS_MACOSX)
class TabModalConfirmDialogMac;
typedef TabModalConfirmDialogMac TabModalConfirmDialog;
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/tab_modal_confirm_dialog_gtk.h"
typedef TabModalConfirmDialogGtk TabModalConfirmDialog;
#else
#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"
typedef TabModalConfirmDialogViews TabModalConfirmDialog;
#endif

class MockTabModalConfirmDialogDelegate;
class TabContentsWrapper;
class TabModalConfirmDialogDelegate;

class TabModalConfirmDialogTest : public InProcessBrowserTest {
 public:
  TabModalConfirmDialogTest();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

 protected:
  void CloseDialog(bool accept);

  // Owned by |dialog_|.
  MockTabModalConfirmDialogDelegate* delegate_;

 private:
  TabModalConfirmDialog* CreateTestDialog(
      TabModalConfirmDialogDelegate* delegate, TabContentsWrapper* wrapper);

  // Deletes itself.
  TabModalConfirmDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogTest);
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
