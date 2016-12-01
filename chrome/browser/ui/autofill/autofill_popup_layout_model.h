// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_

#include <stddef.h>

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class ImageSkia;
}

namespace autofill {

// Helper class which keeps tracks of popup bounds and related view information.
// TODO(mathp): investigate moving ownership of this class to the view.
class AutofillPopupLayoutModel {
 public:
  explicit AutofillPopupLayoutModel(AutofillPopupViewDelegate* delegate);
  ~AutofillPopupLayoutModel();

  // The minimum amount of padding between the Autofill name and subtext,
  // in pixels.
  static const int kNamePadding = 15;

  // The minimum amount of padding between the Autofill http warning message
  // name and subtext, in pixels.
  static const int kHttpWarningNamePadding = 8;

  // The amount of padding around icons in pixels.
  static const int kIconPadding = 5;

  // The amount of horizontal padding around icons in pixels for HTTP warning
  // message.
  static const int kHttpWarningIconPadding = 8;

  // The amount of padding at the end of the popup in pixels.
  static const int kEndPadding = 8;

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

  // The same font can vary based on the type of data it is showing at the row
  // |index|.
  const gfx::FontList& GetValueFontListForRow(size_t index) const;
  const gfx::FontList& GetLabelFontListForRow(size_t index) const;

  // Returns the value font color of the row item according to its |index|.
  SkColor GetValueFontColorForRow(size_t index) const;

  // Returns the icon image of the item at |index| in the popup.
  gfx::ImageSkia GetIconImage(size_t index) const;
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

#if !defined(OS_ANDROID)
  // The fonts for the popup text.
  // Normal font (readable size, non bold).
  gfx::FontList normal_font_list_;
  // Slightly smaller than the normal font.
  gfx::FontList smaller_font_list_;
  // Bold version of the normal font.
  gfx::FontList bold_font_list_;
  // Font used for the warning dialog, which may be italic or not depending on
  // the platform.
  gfx::FontList warning_font_list_;
#endif

  // The bounds of the Autofill popup.
  gfx::Rect popup_bounds_;

  PopupViewCommon view_common_;

  AutofillPopupViewDelegate* delegate_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupLayoutModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
