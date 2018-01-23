// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class ManagePasswordsBubbleModel;

// A view that can show up after saving a password without being signed in to
// offer signing users in so they can access their credentials across devices.
class ManagePasswordSignInPromoView : public views::View {
 public:
  explicit ManagePasswordSignInPromoView(ManagePasswordsBubbleModel* model);

  bool Accept();
  bool Cancel();
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const;

 private:
  ManagePasswordsBubbleModel* const model_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordSignInPromoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SIGN_IN_PROMO_VIEW_H_
