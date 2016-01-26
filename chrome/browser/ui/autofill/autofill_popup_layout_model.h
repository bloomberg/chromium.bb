// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_

#include <stddef.h>

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Display;
class Point;
}

namespace ui {
class KeyEvent;
}

namespace autofill {

// Helper class which keeps tracks of popup bounds and related view information.
// TODO(mathp): investigate moving ownership of this class to the view.
class AutofillPopupLayoutModel {
 public:
  explicit AutofillPopupLayoutModel(AutofillPopupViewDelegate* delegate);

  // The minimum amount of padding between the Autofill name and subtext,
  // in pixels.
  static const int kNamePadding = 15;

  // The amount of padding between icons in pixels.
  static const int kIconPadding = 5;

  // The amount of padding at the end of the popup in pixels.
  static const int kEndPadding = 18;

#if !defined(OS_ANDROID)
  // Calculates the desired height of the popup based on its contents.
  int GetDesiredPopupHeight() const;

  // Calculates the desired width of the popup based on its contents.
  int GetDesiredPopupWidth() const;

  // Calculate the width of the row, excluding all the text. This provides
  // the size of the row that won't be reducible (since all the text can be
  // elided if there isn't enough space). |with_label| indicates whether a label
  // is expected to be present.
  int RowWidthWithoutText(int row, bool with_label) const;

  // Get the available space for the total text width. |with_label| indicates
  // whether a label is expected to be present.
  int GetAvailableWidthForRow(int row, bool with_label) const;

  // Calculates and sets the bounds of the popup, including placing it properly
  // to prevent it from going off the screen.
  void UpdatePopupBounds();
#endif

  // Convert a y-coordinate to the closest line.
  int LineFromY(int y) const;

  const gfx::Rect popup_bounds() const { return popup_bounds_; }

  // Returns the bounds of the item at |index| in the popup, relative to
  // the top left of the popup.
  gfx::Rect GetRowBounds(size_t index) const;

  // Gets the resource value for the given resource, returning -1 if the
  // resource isn't recognized.
  int GetIconResourceID(const base::string16& resource_name) const;

 private:
  // Returns the enclosing rectangle for the element_bounds.
  const gfx::Rect RoundedElementBounds() const;

  // The bounds of the Autofill popup.
  gfx::Rect popup_bounds_;

  PopupViewCommon view_common_;

  AutofillPopupViewDelegate* delegate_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupLayoutModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
