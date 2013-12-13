// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_

#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class ToolbarView;

namespace views {
class Label;
class LabelButton;
class Link;
class Widget;
}

namespace extensions {

// This is a class that implements the UI for the bubble showing which
// extensions look suspicious and have therefore been automatically disabled.
class ExtensionMessageBubbleView : public ExtensionMessageBubble,
                                   public views::BubbleDelegateView,
                                   public views::ButtonListener,
                                   public views::LinkListener {
 public:
  // Show the Disabled Extension bubble, if needed.
  static void MaybeShow(
      Browser* browser, ToolbarView* toolbar_view, views::View* anchor_view);

  // ExtensionMessageBubble methods.
  virtual void OnActionButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnDismissButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnLinkClicked(const base::Closure& callback) OVERRIDE;
  virtual void Show() OVERRIDE;

  // WidgetObserver methods.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  ExtensionMessageBubbleView(
      views::View* anchor_view,
      ExtensionMessageBubbleController::Delegate* delegate);
  virtual ~ExtensionMessageBubbleView();

  // Shows the bubble and updates the counter for how often it has been shown.
  void ShowBubble();

  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View implementation.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  base::WeakPtrFactory<ExtensionMessageBubbleView> weak_factory_;

  // The controller for the bubble. Weak, not owned by us.
  ExtensionMessageBubbleController::Delegate* delegate_;

  // The headline, labels and buttons on the bubble.
  views::Label* headline_;
  views::Link* learn_more_;
  views::LabelButton* action_button_;
  views::LabelButton* dismiss_button_;

  // All actions (link, button, esc) close the bubble, but we need to
  // make sure we don't send dismiss if the link was clicked.
  bool link_clicked_;
  bool action_taken_;

  // Callbacks into the controller.
  base::Closure action_callback_;
  base::Closure dismiss_callback_;
  base::Closure link_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleView);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_VIEW_H_
