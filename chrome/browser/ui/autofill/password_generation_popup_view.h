// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_H_

namespace autofill {

class PasswordGenerationPopupController;

// Interface for creating and controlling a platform dependent view.
class PasswordGenerationPopupView {
 public:
  // This is the amount of vertical whitespace that is left above and below the
  // password when it is highlighted.
  static const int kPasswordVerticalInset = 7;

  // Display the popup.
  virtual void Show() = 0;

  // This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // Updates layout information from the controller.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // Called when the password selection state has changed.
  virtual void PasswordSelectionUpdated() = 0;

  // Note that PasswordGenerationPopupView owns itself, and will only be deleted
  // when Hide() is called.
  static PasswordGenerationPopupView* Create(
      PasswordGenerationPopupController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_H_
