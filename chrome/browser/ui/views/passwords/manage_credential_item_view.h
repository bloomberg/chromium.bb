// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class CredentialsItemView;
class ManagePasswordsBubbleModel;

namespace views {
class ImageButton;
class Link;
}

// A custom view that represents one credential row. There are two states:
// * Present a credential to the user for management.
// * Offer the user the ability to undo a deletion action.
class ManageCredentialItemView : public views::View,
                                 public views::ButtonListener,
                                 public views::LinkListener {
 public:
  ManageCredentialItemView(ManagePasswordsBubbleModel* model,
                           const autofill::PasswordForm* password_form);
  ~ManageCredentialItemView() override;

 private:
  void Refresh();

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  autofill::PasswordForm form_;
  scoped_ptr<CredentialsItemView> credential_button_;
  views::ImageButton* delete_button_;
  views::Link* undo_link_;

  ManagePasswordsBubbleModel* const model_;
  bool form_deleted_;

  DISALLOW_COPY_AND_ASSIGN(ManageCredentialItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_CREDENTIAL_ITEM_VIEW_H_
