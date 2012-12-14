// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_popup_delegate.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "content/public/browser/keyboard_listener.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

class AutofillPopupView;

namespace ui {
class KeyEvent;
}

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl : public AutofillPopupController,
                                    public content::KeyboardListener {
 public:
  AutofillPopupControllerImpl(AutofillPopupDelegate* delegate,
                              gfx::NativeView container_view,
                              const gfx::Rect& element_bounds);

  // Shows the popup, or updates the existing popup with the given values.
  void Show(const std::vector<string16>& autofill_values,
            const std::vector<string16>& autofill_labels,
            const std::vector<string16>& autofill_icons,
            const std::vector<int>& autofill_unique_ids);

  // Hides the popup and destroys the controller.
  void Hide();

  // Called when |delegate_| is no longer valid.
  void DelegateDestroyed();

 protected:
  FRIEND_TEST_ALL_PREFIXES(AutofillExternalDelegateBrowserTest,
                           CloseWidgetAndNoLeaking);
  virtual ~AutofillPopupControllerImpl();

  // AutofillPopupController implementation.
  virtual void ViewDestroyed() OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopup() OVERRIDE;
  virtual void SetSelectedPosition(int x, int y) OVERRIDE;
  virtual bool AcceptAutofillSuggestion(const string16& value,
                                        int unique_id,
                                        unsigned index) OVERRIDE;
  virtual void AcceptSelectedPosition(int x, int y) OVERRIDE;
  virtual void ClearSelectedLine() OVERRIDE;
  virtual int GetIconResourceID(const string16& resource_name) OVERRIDE;
  virtual bool CanDelete(int id) OVERRIDE;
#if !defined(OS_ANDROID)
  virtual int GetPopupRequiredWidth() OVERRIDE;
  virtual int GetPopupRequiredHeight() OVERRIDE;
#endif
  virtual int GetRowHeightFromId(int unique_id) OVERRIDE;
  virtual gfx::Rect GetRectForRow(size_t row, int width) OVERRIDE;
  virtual void SetPopupBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual const gfx::Rect& popup_bounds() const OVERRIDE;
  virtual gfx::NativeView container_view() const OVERRIDE;
  virtual const gfx::Rect& element_bounds() const OVERRIDE;
  virtual const std::vector<string16>& autofill_values() const OVERRIDE;
  virtual const std::vector<string16>& autofill_labels() const OVERRIDE;
  virtual const std::vector<string16>& autofill_icons() const OVERRIDE;
  virtual const std::vector<int>& autofill_unique_ids() const OVERRIDE;
#if !defined(OS_ANDROID)
  virtual const gfx::Font& label_font() const OVERRIDE;
  virtual const gfx::Font& value_font() const OVERRIDE;
#endif
  virtual int selected_line() const OVERRIDE;
  virtual bool delete_icon_hovered() const OVERRIDE;

  // KeyboardListener implementation.
  virtual bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

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

  // Convert a y-coordinate to the closest line.
  int LineFromY(int y);

  // Returns true if the given |x| and |y| coordinates refer to a point that
  // hits the delete icon in the current selected line.
  bool DeleteIconIsUnder(int x, int y);

  // Returns true if the given id refers to an element that can be accepted.
  bool CanAccept(int id);

  // Returns true if the popup still has non-options entries to show the user.
  bool HasAutofillEntries();

  AutofillPopupView* view() { return view_; }

  // |view_| pass throughs (virtual for testing).
  virtual void ShowView();
  virtual void InvalidateRow(size_t row);

 private:
  AutofillPopupView* view_;  // Weak reference.
  AutofillPopupDelegate* delegate_;  // Weak reference.
  gfx::NativeView container_view_;  // Weak reference.

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
  bool delete_icon_hovered_;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
