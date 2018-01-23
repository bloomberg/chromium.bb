// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_delegate_view_base.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class GridLayout;
class ToggleImageButton;
}  // namespace views

// The ManagePasswordsBubbleView controls the contents of the bubble which
// pops up when Chrome offers to save a user's password, or when the user
// interacts with the Omnibox icon. It has two distinct states:
//
// 1. PendingView: Offers the user the possibility of saving credentials.
// 2. ManageView: Displays the current page's saved credentials.
// 3. BlacklistedView: Informs the user that the current page is blacklisted.
class ManagePasswordsBubbleView : public ManagePasswordsBubbleDelegateViewBase,
                                  public views::StyledLabelListener {
 public:
  static constexpr int kDesiredBubbleWidth = 370;

  ManagePasswordsBubbleView(content::WebContents* web_contents,
                            views::View* anchor_view,
                            const gfx::Point& anchor_point,
                            DisplayReason reason);
#if defined(UNIT_TEST)
  const View* initially_focused_view() const {
    return initially_focused_view_;
  }
#endif

 private:
  // TODO(pbos): Friend access here is only provided as an interrim while the
  // dialogs need to access their parent, as the dialogs become truly separate
  // this should go away on its own.
  friend class ManagePasswordPendingView;
  friend class ManagePasswordUpdatePendingView;

  // TODO(pbos): Define column sets within subdialogs or move to a common
  // helper class (maybe ManagePasswordsBubbleDelegateViewBase when all dialogs
  // are proper children).
  enum ColumnSetType {
    // | | (FILL, FILL) | |
    // Used for the bubble's header, the credentials list, and for simple
    // messages like "No passwords".
    SINGLE_VIEW_COLUMN_SET,

    // | | (LEADING, FILL) | | (FILL, FILL) | |
    // Used for the username/password line of the bubble, for the pending view.
    DOUBLE_VIEW_COLUMN_SET_USERNAME,
    DOUBLE_VIEW_COLUMN_SET_PASSWORD,

    // | | (LEADING, FILL) | | (FILL, FILL) | | (TRAILING, FILL) | |
    // Used for the password line of the bubble, for the pending view.
    // Views are label, password and the eye icon.
    TRIPLE_VIEW_COLUMN_SET,

    // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
    // Used for buttons at the bottom of the bubble which should nest at the
    // bottom-right corner.
    DOUBLE_BUTTON_COLUMN_SET,

    // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
    // Used for buttons at the bottom of the bubble which should occupy
    // the corners.
    LINK_BUTTON_COLUMN_SET,

    // | | (TRAILING, CENTER) | |
    // Used when there is only one button which should next at the bottom-right
    // corner.
    SINGLE_BUTTON_COLUMN_SET,

    // | | (LEADING, CENTER) | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
    // Used when there are three buttons.
    TRIPLE_BUTTON_COLUMN_SET,
  };

  ~ManagePasswordsBubbleView() override;

  // DialogDelegate:
  bool ShouldSnapFrameWidth() const override;

  // LocationBarBubbleDelegateView:
  int GetDialogButtons() const override;
  views::View* GetInitiallyFocusedView() override;
  void Init() override;
  void AddedToWidget() override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Updates |title_view|'s text and link styling from |model_|.
  void UpdateTitleText(views::StyledLabel* title_view);

  // Sets up a child view according to the model state.
  void CreateChild();

  void set_initially_focused_view(views::View* view) {
    DCHECK(!initially_focused_view_);
    initially_focused_view_ = view;
  }

  // TODO(pbos): Consider moving to shared base class when subdialogs are proper
  // dialogs instead of child views of this dialog.
  static void BuildColumnSet(views::GridLayout* layout, ColumnSetType type);
  static void BuildCredentialRows(
      views::GridLayout* layout,
      views::View* username_field,
      views::View* password_field,
      views::ToggleImageButton* password_view_button,
      bool show_password_label);

  views::View* initially_focused_view_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_VIEW_H_
