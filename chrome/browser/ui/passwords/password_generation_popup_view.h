// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_

#include "third_party/skia/include/core/SkColor.h"

class PasswordGenerationPopupController;

// Interface for creating and controlling a platform dependent view.
class PasswordGenerationPopupView {
 public:
  // Display the popup.
  virtual void Show() = 0;

  // This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // The state of the popup has changed from editing to offering a new password.
  // The layout should be recreated.
  virtual void UpdateState() = 0;

  // The password was edited, the popup should show the new value.
  virtual void UpdatePasswordValue() {}

  // Updates layout information from the controller and performs the layout.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // Called when the password selection state has changed.
  virtual void PasswordSelectionUpdated() = 0;

  // Note that PasswordGenerationPopupView owns itself, and will only be deleted
  // when Hide() is called.
  static PasswordGenerationPopupView* Create(
      PasswordGenerationPopupController* controller);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_H_
