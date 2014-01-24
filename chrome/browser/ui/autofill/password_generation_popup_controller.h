// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_CONTROLLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"

namespace autofill {

class PasswordGenerationPopupController : public AutofillPopupViewDelegate {
 public:
  // Space above and below the password section.
  static const int kPasswordVerticalPadding = 14;

  // Space above and below help section.
  static const int kHelpVerticalPadding = 15;

  // Spacing between the border of the popup and any text.
  static const int kHorizontalPadding = 9;

  // Called by the view when the help link is clicked.
  virtual void OnHelpLinkClicked() = 0;

  // Accessors
  virtual const gfx::Rect& password_bounds() const = 0;
  virtual const gfx::Rect& divider_bounds() const = 0;
  virtual const gfx::Rect& help_bounds() const = 0;
  virtual bool password_selected() const = 0;
  virtual base::string16 password() const = 0;

  // Translated strings
  virtual base::string16 HelpText() = 0;
  virtual base::string16 LearnMoreLink() = 0;

 protected:
  virtual ~PasswordGenerationPopupController() {}
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
