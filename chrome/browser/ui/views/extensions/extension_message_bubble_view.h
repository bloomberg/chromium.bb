// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Profile;

namespace views {
class Label;
class LabelButton;
class View;
}

namespace extensions {

class ExtensionMessageBubbleController;

// This is a class that implements the UI for the bubble showing which
// extensions look suspicious and have therefore been automatically disabled.
class ExtensionMessageBubbleView : public views::BubbleDelegateView,
                                   public views::ButtonListener,
                                   public views::LinkListener {
 public:
  ExtensionMessageBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow_location,
      scoped_ptr<ExtensionMessageBubbleController> controller);

  // Shows the bubble after a five-second delay.
  void Show();

  // WidgetObserver methods.
  void OnWidgetDestroying(views::Widget* widget) override;

  static void set_bubble_appearance_wait_time_for_testing(int time_in_seconds);

 private:
  ~ExtensionMessageBubbleView() override;

  void ShowBubble();

  // views::BubbleDelegateView overrides:
  void Init() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::View implementation.
  void GetAccessibleState(ui::AXViewState* state) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // The controller for this bubble.
  scoped_ptr<ExtensionMessageBubbleController> controller_;

  // The view this bubble is anchored against.
  views::View* anchor_view_;

  // The headline, labels and buttons on the bubble.
  views::Label* headline_;
  views::Link* learn_more_;
  views::LabelButton* action_button_;
  views::LabelButton* dismiss_button_;

  // All actions (link, button, esc) close the bubble, but we need to
  // make sure we don't send dismiss if the link was clicked.
  bool link_clicked_;
  bool action_taken_;

  base::WeakPtrFactory<ExtensionMessageBubbleView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleView);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
