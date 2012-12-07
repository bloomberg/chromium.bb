// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

namespace content {
class WebContents;
}

class AutofillExternalDelegate;

class AutofillPopupView {
 public:
  explicit AutofillPopupView(content::WebContents* web_contents,
                             AutofillExternalDelegate* external_delegate_,
                             const gfx::Rect& element_bounds);
  virtual ~AutofillPopupView();

  // Hide the popup from view. Platform-indepent work.
  virtual void Hide() = 0;

  // Display the autofill popup and fill it in with the values passed in.
  // Platform-independent work.
  void Show(const std::vector<string16>& autofill_values,
            const std::vector<string16>& autofill_labels,
            const std::vector<string16>& autofill_icons,
            const std::vector<int>& autofill_unique_ids);

  // Update the bounds of the popup.
  void SetPopupBounds(const gfx::Rect& bounds);

  // Sets the current Autofill pointer to NULL, used when the popup can outlive
  // the delegate.
  void ClearExternalDelegate();

  const gfx::Rect& popup_bounds() { return popup_bounds_; }

 protected:
  // Display the autofill popup and fill it in with the values passed in.
  // Platform-dependent work.
  virtual void ShowInternal() = 0;

  // Invalidate the given row and redraw it.
  virtual void InvalidateRow(size_t row) = 0;

  // Ensure the popup is properly placed at |element_bounds_|.
  virtual void UpdateBoundsAndRedrawPopupInternal() = 0;

  AutofillExternalDelegate* external_delegate() { return external_delegate_; }

  const gfx::Rect& element_bounds() const { return element_bounds_; }

  const std::vector<string16>& autofill_values() const {
    return autofill_values_;
  }
  const std::vector<string16>& autofill_labels() const {
    return autofill_labels_;
  }
  const std::vector<string16>& autofill_icons() const {
    return autofill_icons_;
  }
  const std::vector<int>& autofill_unique_ids() const {
    return autofill_unique_ids_;
  }

#if !defined(OS_ANDROID)
  const gfx::Font& label_font() const { return label_font_; }
  const gfx::Font& value_font() const { return value_font_; }
#endif

  int selected_line() const { return selected_line_; }
  bool delete_icon_selected() const { return delete_icon_selected_; }

  // Recalculate the height and width of the popup and trigger a redraw.
  void UpdateBoundsAndRedrawPopup();

  // Change which line is selected by the user, based on coordinates.
  void SetSelectedPosition(int x, int y);

  // Select the value at the given position.
  void AcceptSelectedPosition(int x, int y);

  // Change which line is currently selected by the user.
  void SetSelectedLine(int selected_line);

  // Clear the currently selected line so that nothing is selected.
  void ClearSelectedLine();

  // Increase the selected line by 1, properly handling wrapping.
  void SelectNextLine();

  // Decrease the selected line by 1, properly handling wrapping.
  void SelectPreviousLine();

  // The user has choosen the selected line.
  bool AcceptSelectedLine();

  // The user has removed a suggestion.
  bool RemoveSelectedLine();

  // Get the resource value for the given resource, returning -1 if the
  // resource isn't recognized.
  int GetIconResourceID(const string16& resource_name);

  // Returns true if the given id refers to an element that can be deleted.
  bool CanDelete(int id);

#if !defined(OS_ANDROID)
  // Get width of popup needed by values.
  int GetPopupRequiredWidth();

  // Get height of popup needed by values.
  int GetPopupRequiredHeight();
#endif

  // Convert a y-coordinate to the closest line.
  int LineFromY(int y);

  // Get the height of the given row.
  int GetRowHeightFromId(int unique_id);

  // Returns the rectangle containing the item at position |index| in the popup.
  gfx::Rect GetRectForRow(size_t row, int width);

  // Returns true if the given |x| and |y| coordinates refer to a point that
  // hits the delete icon in the current selected line.
  bool DeleteIconIsSelected(int x, int y);

  // Constants that are needed by the subclasses.
  // The size of the boarder around the entire results popup, in pixels.
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

 private:
  // Returns true if the given id refers to an element that can be accepted.
  bool CanAccept(int id);

  // Returns true if the popup still has non-options entries to show the user.
  bool HasAutofillEntries();

  AutofillExternalDelegate* external_delegate_;

  // The bounds of the text element that is the focus of the Autofill.
  const gfx::Rect element_bounds_;

  // The bounds of the Autofill popup.
  gfx::Rect popup_bounds_;

  // The current Autofill query values.
  std::vector<string16> autofill_values_;
  std::vector<string16> autofill_labels_;
  std::vector<string16> autofill_icons_;
  std::vector<int> autofill_unique_ids_;

#if !defined(OS_ANDROID)
  // The fonts for the popup text.
  gfx::Font value_font_;
  gfx::Font label_font_;
#endif

  // The line that is currently selected by the user.
  // |kNoSelection| indicates that no line is currently selected.
  int selected_line_;

  // Used to indicate if the delete icon within a row is currently selected.
  bool delete_icon_selected_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
