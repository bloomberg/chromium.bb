// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace views {
class LabelButton;
}

namespace content {
class PageNavigator;
}

// OutdatedUpgradeBubbleView warns the user that an upgrade is long overdue.
// It is intended to be used as the content of a bubble anchored off of the
// Chrome toolbar. Don't create an OutdatedUpgradeBubbleView directly,
// instead use the static ShowBubble method.
class OutdatedUpgradeBubbleView : public views::BubbleDelegateView,
                                  public views::ButtonListener {
 public:
  static void ShowBubble(views::View* anchor_view,
                         content::PageNavigator* navigator);

  // Identifies if we are running a build that supports the
  // outdated upgrade bubble view.
  static bool IsAvailable();

  // views::BubbleDelegateView method.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

 private:
  OutdatedUpgradeBubbleView(views::View* anchor_view,
                            content::PageNavigator* navigator);
  virtual ~OutdatedUpgradeBubbleView();

  static bool IsShowing() { return upgrade_bubble_ != NULL; }

  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;

  // views::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // The upgrade bubble, if we're showing one.
  static OutdatedUpgradeBubbleView* upgrade_bubble_;

  // The numer of times the user ignored the bubble before finally choosing to
  // reinstall.
  static int num_ignored_bubbles_;

  // Identifies if the reinstall button was hit before closing the bubble.
  bool chose_to_reinstall_;

  // Button that takes the user to the Chrome download page.
  views::LabelButton* reinstall_button_;

  // Button for the user to be reminded later about the outdated upgrade.
  views::LabelButton* later_button_;

  // The PageNavigator to use for opening the Download Chrome URL.
  content::PageNavigator* navigator_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedUpgradeBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
