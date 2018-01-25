// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_UPDATE_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_UPDATE_PENDING_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class StyledLabel;
}

class CredentialsSelectionView;

// A view offering the user the ability to update credentials. Contains a
// single credential row (in case of one credentials) or
// CredentialsSelectionView otherwise, along with a "Update Passwords" button
// and a rejection button.
class PasswordUpdatePendingView : public PasswordBubbleViewBase,
                                  public views::StyledLabelListener {
 public:
  PasswordUpdatePendingView(content::WebContents* web_contents,
                            views::View* anchor_view,
                            const gfx::Point& anchor_point,
                            DisplayReason reason);
  ~PasswordUpdatePendingView() override;

 private:
  // PasswordBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  void AddedToWidget() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  void UpdateTitleText(views::StyledLabel* title_view);

  CredentialsSelectionView* selection_view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordUpdatePendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_UPDATE_PENDING_VIEW_H_
