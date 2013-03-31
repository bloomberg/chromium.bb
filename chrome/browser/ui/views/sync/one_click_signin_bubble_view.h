// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

namespace base {
class MessageLoop;
}

namespace views {
class GridLayout;
class TextButton;
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
                         ToolbarView* toolbar_view,
                         const BrowserWindow::StartSyncCallback& start_sync);

  static bool IsShowing();

  static void Hide();

  // Gets the global bubble view.  If its not showing returns NULL.  This
  // method is meant to be called only from tests.
  static OneClickSigninBubbleView* view_for_testing() { return bubble_view_; }

 protected:
  // Creates a OneClickSigninBubbleView.
  OneClickSigninBubbleView(
      views::View* anchor_view,
      const BrowserWindow::StartSyncCallback& start_sync_callback);

  virtual ~OneClickSigninBubbleView();

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  friend class OneClickSigninBubbleViewBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewBrowserTest, OkButton);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewBrowserTest, UndoButton);
  FRIEND_TEST_ALL_PREFIXES(OneClickSigninBubbleViewBrowserTest, AdvancedLink);

  // views::BubbleDelegateView methods:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void Init() OVERRIDE;

  // Method to build the main part of the bubble.  Derived classes should
  // reimplement this function.
  virtual void InitContent(views::GridLayout* layout);

  // Creates OK and Undo buttons to be used at the bottom of the bubble.
  // Derived classes can reimplement to have buttons with different labels,
  // colours, or sizes.  The caller of this function owns the returned buttons.
  virtual void GetButtons(views::TextButton** ok_button,
                          views::TextButton** undo_button);

  // Creates advanced link to be used at the bottom of the bubble.
  // Derived classes can reimplement.  The caller of this function owns the
  // returned link.
  virtual views::Link* GetAdvancedLink();

  // views::WidgetDelegate method:
  virtual void WindowClosing() OVERRIDE;

  // views::View method:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // The bubble, if we're showing one.
  static OneClickSigninBubbleView* bubble_view_;

  // Link to sync setup advanced page.
  views::Link* advanced_link_;

  // Controls at bottom of bubble.
  views::TextButton* ok_button_;
  views::TextButton* undo_button_;

  // This callback is nulled once its called, so that it is called only once.
  // It will be called when the bubble is closed if it has not been called
  // and nulled earlier.
  BrowserWindow::StartSyncCallback start_sync_callback_;

  // A message loop used only with unit tests.
  base::MessageLoop* message_loop_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
