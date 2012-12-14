// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Font;
class Point;
class Rect;
}

// This interface provides data to an AutofillPopupView.
class AutofillPopupController {
 public:
  // Called when the view is going down.
  virtual void ViewDestroyed() = 0;

  // Recalculate the height and width of the popup and trigger a redraw.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // Change which line is selected by the user, based on coordinates.
  virtual void SetSelectedPosition(int x, int y) = 0;

  // Accepts the described Autofill suggestion.
  virtual bool AcceptAutofillSuggestion(const string16& value,
                                        int unique_id,
                                        unsigned index) = 0;

  // Select the value at the given position.
  virtual void AcceptSelectedPosition(int x, int y) = 0;

  // Clear the currently selected line so that nothing is selected.
  virtual void ClearSelectedLine() = 0;

  // Get the resource value for the given resource, returning -1 if the
  // resource isn't recognized.
  virtual int GetIconResourceID(const string16& resource_name) = 0;

  // Returns true if the given id refers to an element that can be deleted.
  virtual bool CanDelete(int id) = 0;

#if !defined(OS_ANDROID)
  // Get width of popup needed by values.
  virtual int GetPopupRequiredWidth() = 0;

  // Get height of popup needed by values.
  virtual int GetPopupRequiredHeight() = 0;
#endif

  // Updates the bounds of the popup and initiates a redraw.
  virtual void SetPopupBounds(const gfx::Rect& bounds) = 0;

  // Get the height of the given row.
  virtual int GetRowHeightFromId(int unique_id) = 0;

  // Returns the rectangle containing the item at position |row| in the popup.
  // |row| is the index of the row, and |width| is its width.
  virtual gfx::Rect GetRectForRow(size_t row, int width) = 0;

  // The actual bounds of the popup.
  virtual const gfx::Rect& popup_bounds() const = 0;

  // The view that the form field element sits in.
  virtual gfx::NativeView container_view() const = 0;

  // The bounds of the form field element (relative to |container_origin|).
  virtual const gfx::Rect& element_bounds() const = 0;

  virtual const std::vector<string16>& autofill_values() const = 0;
  virtual const std::vector<string16>& autofill_labels() const = 0;
  virtual const std::vector<string16>& autofill_icons() const = 0;
  virtual const std::vector<int>& autofill_unique_ids() const = 0;

#if !defined(OS_ANDROID)
  virtual const gfx::Font& label_font() const = 0;
  virtual const gfx::Font& value_font() const = 0;
#endif

  virtual int selected_line() const = 0;
  virtual bool delete_icon_hovered() const = 0;

 protected:
  virtual ~AutofillPopupController() {}
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
