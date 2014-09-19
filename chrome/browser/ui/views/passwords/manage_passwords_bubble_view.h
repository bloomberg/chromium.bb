// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "ui/views/bubble/bubble_delegate.h"

class ManagePasswordsIconView;

namespace content {
class WebContents;
}

// The ManagePasswordsBubbleView controls the contents of the bubble which
// pops up when Chrome offers to save a user's password, or when the user
// interacts with the Omnibox icon. It has two distinct states:
//
// 1. PendingView: Offers the user the possibility of saving credentials.
// 2. ManageView: Displays the current page's saved credentials.
// 3. BlacklistedView: Informs the user that the current page is blacklisted.
//
class ManagePasswordsBubbleView : public ManagePasswordsBubble,
                                  public views::BubbleDelegateView {
 public:
  // Shows the bubble.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);

  // Closes the existing bubble.
  static void CloseBubble();

  // Makes the bubble the foreground window.
  static void ActivateBubble();

  // Whether the bubble is currently showing.
  static bool IsShowing();

  // Returns a pointer to the bubble.
  static const ManagePasswordsBubbleView* manage_password_bubble() {
    return manage_passwords_bubble_;
  }

  content::WebContents* web_contents() const;

  const View* initially_focused_view() const {
    return initially_focused_view_;
  }

 private:
  class BlacklistedView;
  class ConfirmNeverView;
  class ManageView;
  class PendingView;
  class SaveConfirmationView;

  ManagePasswordsBubbleView(content::WebContents* web_contents,
                            ManagePasswordsIconView* anchor_view,
                            DisplayReason reason);
  virtual ~ManagePasswordsBubbleView();

  // If the bubble is not anchored to a view, places the bubble in the top
  // right (left in RTL) of the |screen_bounds| that contain |web_contents_|'s
  // browser window. Because the positioning is based on the size of the
  // bubble, this must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

  // Close the bubble.
  void Close();

  // Refreshes the bubble's state: called to display a confirmation screen after
  // a user selects "Never for this site", for instance.
  void Refresh();

  // Called from PendingView if the user clicks on "Never for this site" in
  // order to display a confirmation screen.
  void NotifyNeverForThisSiteClicked();

  // Called from ConfirmNeverView if the user confirms her intention to never
  // save passwords, and remove existing passwords, for a site.
  void NotifyConfirmedNeverForThisSite();

  // Called from ConfirmNeverView if the user clicks on "Undo" in order to
  // undo the action and refresh to PendingView.
  void NotifyUndoNeverForThisSite();

  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::WidgetDelegate
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  void set_initially_focused_view(views::View* view) {
    DCHECK(!initially_focused_view_);
    initially_focused_view_ = view;
  }

  // Singleton instance of the Password bubble. The Password bubble can only be
  // shown on the active browser window, so there is no case in which it will be
  // shown twice at the same time. The instance is owned by the Bubble and will
  // be deleted when the bubble closes.
  static ManagePasswordsBubbleView* manage_passwords_bubble_;

  ManagePasswordsIconView* anchor_view_;

  // If true upon destruction, the user has confirmed that she never wants to
  // save passwords for a particular site.
  bool never_save_passwords_;

  views::View* initially_focused_view_;

  // A helper to intercept mouse click events on the web contents.
  class WebContentMouseHandler;
  scoped_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
