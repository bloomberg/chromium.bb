// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/label_button.h"

// CredentialsItemView represents a credential view in the account chooser
// bubble.
class CredentialsItemView : public views::LabelButton {
 public:
  CredentialsItemView(views::ButtonListener* button_listener,
                      const base::string16& text);
  ~CredentialsItemView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialsItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
