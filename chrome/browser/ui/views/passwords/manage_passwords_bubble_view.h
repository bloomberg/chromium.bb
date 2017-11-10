// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/ui_features.h"
#include "ui/views/controls/styled_label_listener.h"

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
class ManagePasswordsBubbleView : public LocationBarBubbleDelegateView,
                                  public views::StyledLabelListener {
 public:
  static constexpr int kDesiredBubbleWidth = 370;

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
  // Shows the bubble.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);
#endif

  // Closes the existing bubble.
  static void CloseCurrentBubble();

  // Makes the bubble the foreground window.
  static void ActivateBubble();

  // Returns a pointer to the bubble.
  static ManagePasswordsBubbleView* manage_password_bubble() {
    return manage_passwords_bubble_;
  }

  ManagePasswordsBubbleView(content::WebContents* web_contents,
                            views::View* anchor_view,
                            const gfx::Point& anchor_point,
                            DisplayReason reason);

  content::WebContents* web_contents() const;

#if defined(UNIT_TEST)
  const View* initially_focused_view() const {
    return initially_focused_view_;
  }

  static void set_auto_signin_toast_timeout(int seconds) {
    auto_signin_toast_timeout_ = seconds;
  }
#endif

  ManagePasswordsBubbleModel* model() { return &model_; }

 private:
  class AutoSigninView;
  class ManageView;
  class PendingView;
  class SaveConfirmationView;
  class SignInPromoView;
  class UpdatePendingView;

  ~ManagePasswordsBubbleView() override;

  // DialogDelegate:
  bool ShouldSnapFrameWidth() const override;

  // LocationBarBubbleDelegateView:
  int GetDialogButtons() const override;
  views::View* GetInitiallyFocusedView() override;
  void Init() override;
  void CloseBubble() override;
  void AddedToWidget() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Refreshes the bubble's state.
  void Refresh();

  // Updates |title_view|'s text and link styling from |model_|.
  void UpdateTitleText(views::StyledLabel* title_view);

  // Sets up a child view according to the model state.
  void CreateChild();

  void set_initially_focused_view(views::View* view) {
    DCHECK(!initially_focused_view_);
    initially_focused_view_ = view;
  }

  // Singleton instance of the Password bubble. The Password bubble can only be
  // shown on the active browser window, so there is no case in which it will be
  // shown twice at the same time. The instance is owned by the Bubble and will
  // be deleted when the bubble closes.
  static ManagePasswordsBubbleView* manage_passwords_bubble_;

  // The timeout in seconds for the auto sign-in toast.
  static int auto_signin_toast_timeout_;

  ManagePasswordsBubbleModel model_;

  views::View* initially_focused_view_;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
