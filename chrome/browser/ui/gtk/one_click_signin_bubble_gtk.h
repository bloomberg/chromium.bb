// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class BrowserWindowGtk;
class CustomDrawButton;

// Displays the one-click signin confirmation bubble (before syncing
// has started).
class OneClickSigninBubbleGtk : public BubbleDelegateGtk {
 public:
  // Deletes self on close.  The given callback will be called if the
  // user decides to start sync.
  OneClickSigninBubbleGtk(
      BrowserWindowGtk* browser_window_gtk,
      BrowserWindow::OneClickSigninBubbleType type,
      const BrowserWindow::StartSyncCallback& start_sync_callback);

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(
      BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleGtkTest, ShowAndOK);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleGtkTest, ShowAndUndo);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleGtkTest, ShowAndClickAdvanced);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleGtkTest, ShowAndClose);

  virtual ~OneClickSigninBubbleGtk();

  void InitializeWidgets(BrowserWindowGtk* window);
  GtkWidget* LayoutWidgets();
  void ShowWidget(BrowserWindowGtk* browser_window_gtk,
                  GtkWidget* content_widget);

  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickAdvancedLink);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickOK);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickUndo);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickLearnMoreLink);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickCloseButton);

  BubbleGtk* bubble_;

  // This callback is nulled once its called, so that it is called only once.
  // It will be called when the bubble is closed if it has not been called
  // and nulled earlier.
  BrowserWindow::StartSyncCallback start_sync_callback_;

  bool is_modal_;

  GtkWidget* message_label_;
  GtkWidget* advanced_link_;
  GtkWidget* ok_button_;
  GtkWidget* undo_button_;

  // These widgets are only used in the modal dialog, and not in the bubble.
  GtkWidget* learn_more_;
  GtkWidget* header_label_;
  scoped_ptr<CustomDrawButton> close_button_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_
