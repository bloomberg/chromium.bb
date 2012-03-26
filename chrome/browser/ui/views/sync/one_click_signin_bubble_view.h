// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class MessageLoop;

namespace views {
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
  // will be placed visually beneath |anchor_view|.  The |browser| is used
  // to open links.
  static void ShowBubble(views::View* anchor_view, Browser* browser);

  static bool IsShowing();

  static void Hide();

  // Gets the global bubble view.  If its not showing returns NULL.  This
  // method is meant to be called only from tests.
  static OneClickSigninBubbleView* view_for_testing() { return bubble_view_; }

  // The following accessor message should only be used for testing.
  views::Link* learn_more_link_for_testing() const { return learn_more_link_; }
  views::Link* advanced_link_for_testing() const { return advanced_link_; }
  views::TextButton* close_button_for_testing() const { return close_button_; }
  void set_message_loop_for_testing(MessageLoop* loop) {
    message_loop_for_testing_ = loop;
  }

 private:
  // Creates a BookmarkBubbleView.
  OneClickSigninBubbleView(views::View* anchor_view, Browser* browser);

  virtual ~OneClickSigninBubbleView();

  // views::BubbleDelegateView methods:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void Init() OVERRIDE;

  // views::WidgetDelegate method:
  virtual void WindowClosing() OVERRIDE;

  // views::View method:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The bubble, if we're showing one.
  static OneClickSigninBubbleView* bubble_view_;

  // Link to web page to learn more about sync.
  views::Link* learn_more_link_;

  // Link to sync setup advanced page.
  views::Link* advanced_link_;

  // Button to close the window.
  views::TextButton* close_button_;

  // The browser that contains this bubble.
  Browser* browser_;

  // A message loop used only with unit tests.
  MessageLoop* message_loop_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_ONE_CLICK_SIGNIN_BUBBLE_VIEW_H_
