// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

class OneClickSigninBubbleGtkTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleGtkTest()
      : weak_ptr_factory_(this),
        start_sync_callback_(
            base::Bind(&OneClickSigninBubbleGtkTest::OnStartSync,
                       weak_ptr_factory_.GetWeakPtr())),
        bubble_(NULL) {}

  virtual OneClickSigninBubbleGtk* MakeBubble(
    BrowserWindow::OneClickSigninBubbleType bubbleType) {
    return new OneClickSigninBubbleGtk(
        static_cast<BrowserWindowGtk*>(browser()->window()),
        bubbleType,
        string16(),
        string16(),
        start_sync_callback_);
  }

  MOCK_METHOD1(OnStartSync, void(OneClickSigninSyncStarter::StartSyncMode));

 protected:
  base::WeakPtrFactory<OneClickSigninBubbleGtkTest> weak_ptr_factory_;
  BrowserWindow::StartSyncCallback start_sync_callback_;

  // Owns itself.
  OneClickSigninBubbleGtk* bubble_;
};

// Test that the dialog calls the callback if the OK button is clicked.
// Callback should be called to setup sync with default settings.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, DialogShowAndOK) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS)).Times(1);

  MakeBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG)->OnClickOK(NULL);
}

// Test that the dialog calls the callback if the Undo button is
// clicked. Callback should be called to abort the sync process.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, DialogShowAndUndo) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::UNDO_SYNC)).Times(1);

  MakeBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG)
      ->OnClickUndo(NULL);
}

// Test that the dialog calls the callback if the advanced link is clicked.
// Callback should be called to configure sync before starting.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, DialogShowAndClickAdvanced){
  EXPECT_CALL(*this,
              OnStartSync(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST)).
      Times(1);

  MakeBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG)
      ->OnClickAdvancedLink(NULL);
}

// Test that the dialog calls the callback if the bubble is closed.
// Callback should be called to setup sync with default settings.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, DialogShowAndClose) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS)).Times(1);

  MakeBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG)->bubble_->Close();
}

// Test that the bubble does not call the callback if the OK button is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, BubbleShowAndOK) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS)).Times(0);

  MakeBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE)->OnClickOK(NULL);
}

// Test that the bubble does not call the callback
// if the advanced link is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, BubbleShowAndClickAdvanced){
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST)).Times(0);

  MakeBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE)
      ->OnClickAdvancedLink(NULL);
}

// Test that the bubble does not call the callback if the bubble is closed.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, BubbleShowAndClose) {
  EXPECT_CALL(*this, OnStartSync(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS)).Times(0);

  MakeBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE)->bubble_->Close();
}


