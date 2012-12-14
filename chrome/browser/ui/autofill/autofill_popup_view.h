// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_

#include "ui/gfx/native_widget_types.h"

class AutofillPopupController;

namespace gfx {
class Rect;
}

namespace ui {
class KeyEvent;
}

// The interface for creating and controlling a platform-dependent
// AutofillPopupView.
class AutofillPopupView {
 public:
  // The size of the border around the entire results popup, in pixels.
  static const size_t kBorderThickness;

  // The amount of padding between icons in pixels.
  static const size_t kIconPadding;

  // The amount of padding at the end of the popup in pixels.
  static const size_t kEndPadding;

  // Height of the delete icon in pixels.
  static const size_t kDeleteIconHeight;

  // Width of the delete icon in pixels.
  static const size_t kDeleteIconWidth;

  // Height of the Autofill icons in pixels.
  static const size_t kAutofillIconHeight;

  // Width of the Autofill icons in pixels.
  static const size_t kAutofillIconWidth;

  // Displays the Autofill popup and fills it in with data from the controller.
  virtual void Show() = 0;

  // Hides the popup from view. This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // Invalidates the given row and redraw it.
  virtual void InvalidateRow(size_t row) = 0;

  // Refreshes the position of the popup.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // Factory function for creating the view.
  static AutofillPopupView* Create(AutofillPopupController* controller);

 protected:
  virtual ~AutofillPopupView() {}
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
