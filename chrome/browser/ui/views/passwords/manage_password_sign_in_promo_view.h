// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class ManagePasswordsBubbleView;

// A view that offers user to sign in to Chrome.
class ManagePasswordSignInPromoView : public views::View,
                                      public views::ButtonListener {
 public:
  explicit ManagePasswordSignInPromoView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::Button* signin_button_;
  views::Button* no_button_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordSignInPromoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_
