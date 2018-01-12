// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_UPDATE_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_UPDATE_PENDING_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class CredentialsSelectionView;
class ManagePasswordsBubbleView;

// A view offering the user the ability to update credentials. Contains a
// single credential row (in case of one credentials) or
// CredentialsSelectionView otherwise, along with a "Update Passwords" button
// and a rejection button.
class ManagePasswordUpdatePendingView : public views::View,
                                        public views::ButtonListener {
 public:
  explicit ManagePasswordUpdatePendingView(ManagePasswordsBubbleView* parent);
  ~ManagePasswordUpdatePendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  ManagePasswordsBubbleView* parent_;

  CredentialsSelectionView* selection_view_;

  views::Button* update_button_;
  views::Button* nope_button_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordUpdatePendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_UPDATE_PENDING_VIEW_H_
