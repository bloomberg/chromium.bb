// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

namespace autofill {

class AutofillPopupLayoutModel;
struct Suggestion;

// This interface provides data to an AutofillPopupView.
class AutofillPopupController : public AutofillPopupViewDelegate {
 public:
  // Recalculates the height and width of the popup and triggers a redraw.
  virtual void UpdateBoundsAndRedrawPopup() = 0;

  // Accepts the suggestion at |index|.
  virtual void AcceptSuggestion(size_t index) = 0;

  // Returns true if the given index refers to an element that is a warning
  // rather than an Autofill suggestion.
  virtual bool IsWarning(size_t index) const = 0;

  // Returns the number of lines of data that there are.
  virtual size_t GetLineCount() const = 0;

  // Returns the suggestion or pre-elided string at the given row index.
  virtual const autofill::Suggestion& GetSuggestionAt(size_t row) const = 0;
  virtual const base::string16& GetElidedValueAt(size_t row) const = 0;
  virtual const base::string16& GetElidedLabelAt(size_t row) const = 0;

  // Returns whether the item at |list_index| can be removed. If so, fills
  // out |title| and |body| (when non-null) with relevant user-facing text.
  virtual bool GetRemovalConfirmationText(int index,
                                          base::string16* title,
                                          base::string16* body) = 0;

  // Removes the suggestion at the given index.
  virtual bool RemoveSuggestion(int index) = 0;

  // Returns the background color of the row item according to its |index|, or
  // transparent if the default popup background should be used.
  virtual SkColor GetBackgroundColorForRow(int index) const = 0;

  // Returns the index of the selected line. A line is "selected" when it is
  // hovered or has keyboard focus.
  virtual int selected_line() const = 0;

  virtual const AutofillPopupLayoutModel& layout_model() const = 0;

 protected:
  ~AutofillPopupController() override {}
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
