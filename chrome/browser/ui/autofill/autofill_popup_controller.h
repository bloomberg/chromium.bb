// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Font;
class Point;
class Rect;
class RectF;
}

// This interface provides data to an AutofillPopupView.
class AutofillPopupController {
 public:
  // Called when the popup should get hidden.
  virtual void Hide() = 0;

  // Called whent the popup view was destroyed.
  virtual void ViewDestroyed() = 0;

  // Recalculates the height and width of the popup and triggers a redraw.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // The user has moved the mouse within the popup.
  virtual void MouseHovered(int x, int y) = 0;

  // The user clicked the mouse within the popup.
  virtual void MouseClicked(int x, int y) = 0;

  // The user has moved the mouse outside of the popup.
  virtual void MouseExitedPopup() = 0;

  // Accepts the suggestion at |index|.
  virtual void AcceptSuggestion(size_t index) = 0;

  // Gets the resource value for the given resource, returning -1 if the
  // resource isn't recognized.
  virtual int GetIconResourceID(const string16& resource_name) = 0;

  // Returns true if the given index refers to an element that can be deleted.
  virtual bool CanDelete(size_t index) const = 0;

  // Updates the bounds of the popup and initiates a redraw.
  virtual void SetPopupBounds(const gfx::Rect& bounds) = 0;

  // Returns the bounds of the item at |index| in the popup, relative to
  // the top left of the popup.
  virtual gfx::Rect GetRowBounds(size_t index) = 0;

  // The actual bounds of the popup.
  virtual const gfx::Rect& popup_bounds() const = 0;

  // The view that the form field element sits in.
  virtual gfx::NativeView container_view() const = 0;

  // The bounds of the form field element (screen coordinates).
  virtual const gfx::RectF& element_bounds() const = 0;

  // TODO(csharp): The names, subtexts and icon getters can probably be adjusted
  // to take in the row index and return a single element, instead of the
  // whole vector.
  // The main labels for each autofill item.
  virtual const std::vector<string16>& names() const = 0;

  // Smaller labels for each autofill item.
  virtual const std::vector<string16>& subtexts() const = 0;

  // A string which identifies the icon to be shown for each autofill item.
  virtual const std::vector<string16>& icons() const = 0;

  // Identifier for the row.
  virtual const std::vector<int>& identifiers() const = 0;

#if !defined(OS_ANDROID)
  // The same font can vary based on the type of data it is showing,
  // so we need to know the row.
  virtual const gfx::Font& GetNameFontForRow(size_t index) const = 0;
  virtual const gfx::Font& subtext_font() const = 0;
#endif

  // Returns the index of the selected line. A line is "selected" when it is
  // hovered or has keyboard focus.
  virtual int selected_line() const = 0;

 protected:
  virtual ~AutofillPopupController() {}
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
