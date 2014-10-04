// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "chrome/browser/ui/autofill/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {
class StyledLabel;
class View;
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
  virtual void Show() override;
  virtual void Hide() override;
  virtual gfx::Size GetPreferredSizeOfPasswordView() override;
  virtual void UpdateBoundsAndRedrawPopup() override;
  virtual void PasswordSelectionUpdated() override;
  virtual bool IsPointInPasswordBounds(const gfx::Point& point) override;

 private:
  // Helper class to do layout of the password portion of the popup.
  class PasswordBox;

  virtual ~PasswordGenerationPopupViewViews();

  // Helper function to create |password_view_|.
  void CreatePasswordView();

  // views:Views implementation.
  virtual void Layout() override;
  virtual void OnPaint(gfx::Canvas* canvas) override;

  // views::StyledLabelListener implementation
  virtual void StyledLabelLinkClicked(const gfx::Range& range,
                                      int event_flags) override;

  // Sub views. Used to change bounds when updating. Weak references.
  PasswordBox* password_view_;
  views::StyledLabel* help_label_;

  // Fonts to use.
  gfx::FontList font_list_;

  // Size of the divider between the password and the help text.
  gfx::Rect divider_bounds_;

  // Controller for this view. Weak reference.
  PasswordGenerationPopupController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationPopupViewViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
