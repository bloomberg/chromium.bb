// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}

namespace autofill {

class AutofillPopupDelegate;
class AutofillPopupView;

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl : public AutofillPopupController {
 public:
  // Creates a new |AutofillPopupControllerImpl|, or reuses |previous| if the
  // construction arguments are the same. |previous| may be invalidated by this
  // call. The controller will listen for keyboard input routed to
  // |web_contents| while the popup is showing, unless |web_contents| is NULL.
  static base::WeakPtr<AutofillPopupControllerImpl> GetOrCreate(
      base::WeakPtr<AutofillPopupControllerImpl> previous,
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      gfx::NativeView container_view,
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction);

  // Shows the popup, or updates the existing popup with the given values.
  void Show(const std::vector<autofill::Suggestion>& suggestions);

  // Updates the data list values currently shown with the popup.
  void UpdateDataListValues(const std::vector<base::string16>& values,
                            const std::vector<base::string16>& labels);

  // Hides the popup and destroys the controller. This also invalidates
  // |delegate_|.
  void Hide() override;

  // Invoked when the view was destroyed by by someone other than this class.
  void ViewDestroyed() override;

  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // Tells the view to capture mouse events. Must be called before |Show()|.
  void set_hide_on_outside_click(bool hide_on_outside_click);

 protected:
  FRIEND_TEST_ALL_PREFIXES(AutofillPopupControllerUnitTest,
                           ProperlyResetController);

  AutofillPopupControllerImpl(base::WeakPtr<AutofillPopupDelegate> delegate,
                              content::WebContents* web_contents,
                              gfx::NativeView container_view,
                              const gfx::RectF& element_bounds,
                              base::i18n::TextDirection text_direction);
  ~AutofillPopupControllerImpl() override;

  // AutofillPopupViewDelegate implementation.
  void SetSelectionAtPoint(const gfx::Point& point) override;
  bool AcceptSelectedLine() override;
  void SelectionCleared() override;
  gfx::Rect popup_bounds() const override;
  gfx::NativeView container_view() override;
  const gfx::RectF& element_bounds() const override;
  void SetElementBounds(const gfx::RectF& bounds);
  bool IsRTL() const override;
  const std::vector<autofill::Suggestion> GetSuggestions() override;
#if !defined(OS_ANDROID)
  int GetElidedValueWidthForRow(int row) override;
  int GetElidedLabelWidthForRow(int row) override;
#endif

  // AutofillPopupController implementation.
  void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index) override;
  int GetLineCount() const override;
  const autofill::Suggestion& GetSuggestionAt(int row) const override;
  const base::string16& GetElidedValueAt(int row) const override;
  const base::string16& GetElidedLabelAt(int row) const override;
  bool GetRemovalConfirmationText(int list_index,
                                  base::string16* title,
                                  base::string16* body) override;
  bool RemoveSuggestion(int list_index) override;
  ui::NativeTheme::ColorId GetBackgroundColorIDForRow(int index) const override;
  base::Optional<int> selected_line() const override;
  const AutofillPopupLayoutModel& layout_model() const override;

  // Change which line is currently selected by the user.
  void SetSelectedLine(base::Optional<int> selected_line);

  // Increase the selected line by 1, properly handling wrapping.
  void SelectNextLine();

  // Decrease the selected line by 1, properly handling wrapping.
  void SelectPreviousLine();

  // The user has removed a suggestion.
  bool RemoveSelectedLine();

  // Returns true if the given id refers to an element that can be accepted.
  bool CanAccept(int id);

  // Returns true if the popup still has non-options entries to show the user.
  bool HasSuggestions();

  // Set the Autofill entry values. Exposed to allow tests to set these values
  // without showing the popup.
  void SetValues(const std::vector<autofill::Suggestion>& suggestions);

  AutofillPopupView* view() { return view_; }

  base::WeakPtr<AutofillPopupControllerImpl> GetWeakPtr();

  // Contains common popup functionality such as popup layout. Protected for
  // testing.
  PopupControllerCommon controller_common_;

 private:
#if !defined(OS_ANDROID)
  FRIEND_TEST_ALL_PREFIXES(AutofillPopupControllerUnitTest, ElideText);
  // Helper method which elides the value and label for the suggestion at |row|
  // given the |available_width|. Puts the results in |elided_values_| and
  // |elided_labels_|.
  void ElideValueAndLabelForRow(int row, int available_width);
#endif

  // Clear the internal state of the controller. This is needed to ensure that
  // when the popup is reused it doesn't leak values between uses.
  void ClearState();

  friend class AutofillPopupControllerUnitTest;
  void SetViewForTesting(AutofillPopupView* view) { view_ = view; }

  AutofillPopupView* view_;  // Weak reference.
  AutofillPopupLayoutModel layout_model_;
  base::WeakPtr<AutofillPopupDelegate> delegate_;

  // The text direction of the popup.
  base::i18n::TextDirection text_direction_;

  // The current Autofill query values.
  std::vector<autofill::Suggestion> suggestions_;

  // Elided values and labels corresponding to the suggestions_ vector to
  // ensure that it fits on the screen.
  std::vector<base::string16> elided_values_;
  std::vector<base::string16> elided_labels_;

  // The line that is currently selected by the user, null indicates that no
  // line is currently selected.
  base::Optional<int> selected_line_;

  base::WeakPtrFactory<AutofillPopupControllerImpl> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
