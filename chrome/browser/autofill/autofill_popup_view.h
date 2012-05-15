// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "ui/gfx/rect.h"

namespace content {
class WebContents;
}

class AutofillExternalDelegate;

class AutofillPopupView : public content::NotificationObserver {
 public:
  explicit AutofillPopupView(content::WebContents* web_contents,
                             AutofillExternalDelegate* external_delegate_);
  virtual ~AutofillPopupView();

  // Hide the popup from view. Platform-indepent work.
  virtual void Hide();

  // Display the autofill popup and fill it in with the values passed in.
  // Platform-independent work.
  void Show(const std::vector<string16>& autofill_values,
            const std::vector<string16>& autofill_labels,
            const std::vector<string16>& autofill_icons,
            const std::vector<int>& autofill_unique_ids);

  void set_element_bounds(const gfx::Rect& bounds) {
    element_bounds_ = bounds;
  }

  const gfx::Rect& element_bounds() { return element_bounds_; }

 protected:
  // Display the autofill popup and fill it in with the values passed in.
  // Platform-dependent work.
  virtual void ShowInternal() = 0;

  // Hide the popup from view. Platform-dependent work.
  virtual void HideInternal() = 0;

  // Invalide the given row and redraw it.
  virtual void InvalidateRow(size_t row) = 0;

  // Adjust the size of the popup to show the elements being held.
  virtual void ResizePopup() = 0;

  AutofillExternalDelegate* external_delegate() { return external_delegate_; }

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

  int selected_line() const { return selected_line_; }

  // Change which line is currently selected by the user.
  void SetSelectedLine(int selected_line);

  // Increase the selected line by 1, properly handling wrapping.
  void SelectNextLine();

  // Decrease the selected line by 1, properly handling wrapping.
  void SelectPreviousLine();

  // The user has choosen the selected line.
  bool AcceptSelectedLine();

  // The user has removed a suggestion.
  bool RemoveSelectedLine();

  // Return true if the index is the first element of a new section and should
  // have a separator above it.
  bool IsSeparatorIndex(int index);

  // Get the resource value for the given resource, returning -1 if the
  // resource isn't recognized.
  int GetIconResourceID(const string16& resource_name);

 private:
  // Returns true if the given id refers to an element that can be deleted.
  bool CanDelete(int id);

  // Returns true if the popup still has non-options entries to show the user.
  bool HasAutofillEntries();

  // content::NotificationObserver method override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  AutofillExternalDelegate* external_delegate_;

  // The bounds of the text element that is the focus of the Autofill.
  gfx::Rect element_bounds_;

  // The current Autofill query values.
  std::vector<string16> autofill_values_;
  std::vector<string16> autofill_labels_;
  std::vector<string16> autofill_icons_;
  std::vector<int> autofill_unique_ids_;

  // The line that is currently selected by the user.
  // |kNoSelection| indicates that no line is currently selected.
  int selected_line_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
