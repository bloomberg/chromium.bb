// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "chrome/browser/ui/autofill/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class Label;
}

namespace autofill {

class PasswordGenerationPopupController;

class PasswordGenerationPopupViewViews : public AutofillPopupBaseView,
                                         public PasswordGenerationPopupView,
                                         public views::StyledLabelListener {
 public:
  PasswordGenerationPopupViewViews(
      PasswordGenerationPopupController* controller,
      views::Widget* observing_widget);

  // PasswordGenerationPopupView implementation
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopup() OVERRIDE;

 private:
  virtual ~PasswordGenerationPopupViewViews();

  // views:Views implementation.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::StyledLabelListener implementation
  virtual void StyledLabelLinkClicked(const gfx::Range& range,
                                      int event_flags) OVERRIDE;

  // Label for the generated password. Used to change the background color when
  // the password is selected/deselected. Weak reference.
  views::Label* password_label_;

  // Controller for this view. Weak reference.
  PasswordGenerationPopupController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationPopupViewViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
