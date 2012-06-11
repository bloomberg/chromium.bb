// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"

#include "chrome/browser/ui/cocoa/tab_modal_confirm_dialog_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

TabModalConfirmDialog* TabModalConfirmDialogTest::CreateTestDialog(
    TabModalConfirmDialogDelegate* delegate, TabContents* tab_contents) {
  return new TabModalConfirmDialogMac(delegate, tab_contents);
}

void TabModalConfirmDialogTest::CloseDialog(bool accept) {
  NSWindow* window = [(NSAlert*)dialog_->sheet() window];
  ASSERT_TRUE(window);
  ASSERT_TRUE(dialog_->is_sheet_open());
  [NSApp endSheet:window
       returnCode:accept ? NSAlertFirstButtonReturn :
                           NSAlertSecondButtonReturn];
}
