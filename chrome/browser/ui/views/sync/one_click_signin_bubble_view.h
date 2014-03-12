// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/one_click_signin_bubble_delegate.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

namespace views {
class GridLayout;
class ImageButton;
class LabelButton;
class View;
}

// OneClickSigninBubbleView is a view intended to be used as the content of an
// Bubble. It provides simple and concise feedback to the user that sync'ing
// has started after using the one-click singin infobar.
class OneClickSigninBubbleView : public views::BubbleDelegateView,
                                 public views::LinkListener,
                                 public views::ButtonListener {
 public:
  // Show the one-click signin bubble if not already showing.  The bubble
  // will be placed visually beneath |anchor_view|.  |start_sync| is called
  // to start sync.
  static void ShowBubble(BrowserWindow::OneClickSigninBubbleType type,
                         const base::string16& email,
                         const base::string16& error_message,
                         scoped_ptr<OneClickSigninBubbleDelegate> delegate,
                         views::View* anchor_view,
                         const BrowserWindow::StartSyncCallback& start_sync);

  static bool IsShowing();

  static void Hide();

  // Gets the global bubble view.  If its not showing returns NULL.  This
  // method is meant to be called only from tests.
  static OneClickSigninBubbleView* view_for_testing() { return bubble_view_; }

 protected:
  // Creates a OneClickSigninBubbleView.
  OneClickSigninBubbleView(
      const base::string16& error_message,
      const base::string16& email,
      scoped_ptr<OneClickSigninBubbleDelegate> delegate,
      views::View* anchor_view,
      const BrowserWindow::StartSyncCallback& start_sync_callback,
      bool is_sync_dialog);

  virtual ~OneClickSigninBubbleView();

 private:
  friend class OneClickSigninBubbleViewTest;

  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, BubbleOkButton);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, DialogOkButton);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, DialogUndoButton);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, BubbleAdvancedLink);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, DialogAdvancedLink);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, BubbleLearnMoreLink);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewTest, DialogLearnMoreLink);

  // Overridden from views::BubbleDelegateView:
  virtual void Init() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // Builds a popup bubble anchored under the wrench menu
  void InitBubbleContent(views::GridLayout* layout);

  // Builds a modal dialog aligned center top
  void InitDialogContent(views::GridLayout* layout);

  // Initializes the OK/Undo buttons to be used at the bottom of the bubble.
  void InitButtons(views::GridLayout* layout);
  void GetButtons(views::LabelButton** ok_button,
                          views::LabelButton** undo_button);

  // Creates learn more link to be used at the bottom of the bubble.
  void InitLearnMoreLink();

  // Creates advanced link to be used at the bottom of the bubble.
  void InitAdvancedLink();

  // Delegate to handle clicking on links in the bubble.
  scoped_ptr<OneClickSigninBubbleDelegate> delegate_;

  // Alternate error message to be displayed.
  const base::string16 error_message_;

  // The user's email address to be used for sync.
  const base::string16 email_;

  // This callback is nulled once its called, so that it is called only once.
  // It will be called when the bubble is closed if it has not been called
  // and nulled earlier.
  BrowserWindow::StartSyncCallback start_sync_callback_;

  const bool is_sync_dialog_;

  // Link to sync setup advanced page.
  views::Link* advanced_link_;

  // Link to the Learn More details page
  views::Link* learn_more_link_;

  // Controls at bottom of bubble.
  views::LabelButton* ok_button_;
  views::LabelButton* undo_button_;

  // Close button for the modal dialog
  views::ImageButton* close_button_;

  bool clicked_learn_more_;

  // The bubble, if we're showing one.
  static OneClickSigninBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
