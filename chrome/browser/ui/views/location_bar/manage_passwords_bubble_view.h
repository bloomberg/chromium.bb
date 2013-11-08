// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace content {
class WebContents;
}

namespace views {
class LabelButton;
}

class ManagePasswordsBubbleView : public views::BubbleDelegateView,
                                  public views::ButtonListener {
 public:
  // Shows the bubble.
  static void ShowBubble(content::WebContents* web_contents);

  // Closes any existing bubble.
  static void CloseBubble();

  // Whether the bubble is currently showing.
  static bool IsShowing();

 private:
  ManagePasswordsBubbleView(views::View* anchor_view,
                            content::WebContents* web_contents);
  virtual ~ManagePasswordsBubbleView();

  // If the bubble is not anchored to a view, places the bubble in the top
  // right (left in RTL) of the |screen_bounds| that contain |web_contents_|'s
  // browser window. Because the positioning is based on the size of the
  // bubble, this must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

  void Close();

  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Singleton instance of the Password bubble. The Password bubble can only be
  // shown on the active browser window, so there is no case in which it will be
  // shown twice at the same time.
  static ManagePasswordsBubbleView* manage_passwords_bubble_;

  // The WebContents for the page where the password bubble is displayed.
  content::WebContents* web_contents_;

  // The buttons that are shown in the bubble.
  views::LabelButton* save_button_;
  views::LabelButton* cancel_button_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
