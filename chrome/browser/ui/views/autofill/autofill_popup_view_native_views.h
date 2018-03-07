// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

#include <vector>

namespace views {
class Label;
}

namespace autofill {

class AutofillPopupController;

// Child view representing one row (i.e., one suggestion) in the Autofill
// Popup.
class AutofillPopupRowView : public views::View {
 public:
  AutofillPopupRowView(AutofillPopupController* controller, int line_number);

  ~AutofillPopupRowView() override {}

  void AcceptSelection();
  void SetSelected(bool is_selected);
  void RefreshStyle();

  // views::View:
  // TODO(tmartino): Consolidate and deprecate code in AutofillPopupBaseView
  // where overlap exists with these events.
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void CreateContent();

  AutofillPopupController* controller_;
  const int line_number_;
  bool is_separator_ = false;  // overwritten in ctor
  bool is_warning_ = false;    // overwritten in ctor
  bool is_selected_ = false;

  views::Label* text_label_ = nullptr;
  views::Label* subtext_label_ = nullptr;

  SkColor text_color_ = gfx::kPlaceholderColor;
  SkColor text_selected_color_ = gfx::kPlaceholderColor;
  SkColor subtext_color_ = gfx::kPlaceholderColor;
  SkColor subtext_selected_color_ = gfx::kPlaceholderColor;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupRowView);
};

// Views implementation for the autofill and password suggestion.
// TODO(https://crbug.com/768881): Once this implementation is complete, this
// class should be renamed to AutofillPopupViewViews and old
// AutofillPopupViewViews should be removed. The main difference of
// AutofillPopupViewNativeViews from AutofillPopupViewViews is that child views
// are drawn using toolkit-views framework, in contrast to
// AutofillPopupViewViews, where individuals rows are drawn directly on canvas.
class AutofillPopupViewNativeViews : public AutofillPopupBaseView,
                                     public AutofillPopupView {
 public:
  AutofillPopupViewNativeViews(AutofillPopupController* controller,
                               views::Widget* parent_widget);
  ~AutofillPopupViewNativeViews() override;

  const std::vector<AutofillPopupRowView*>& GetRowsForTesting() {
    return rows_;
  }

  // AutofillPopupView:
  void Show() override;
  void Hide() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // AutofillPopupBaseView:
  // TODO(tmartino): Remove these overrides and the corresponding methods in
  // AutofillPopupBaseView once deprecation of AutofillPopupViewViews is
  // complete.
  void OnMouseExited(const ui::MouseEvent& event) override {}
  void OnMouseMoved(const ui::MouseEvent& event) override {}

 private:
  void OnSelectedRowChanged(base::Optional<int> previous_row_selection,
                            base::Optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;

  // Creates child views based on the suggestions given by |controller_|.
  void CreateChildViews();

  // AutofillPopupBaseView:
  void DoUpdateBoundsAndRedrawPopup() override;

  // Controller for this view.
  AutofillPopupController* controller_;

  std::vector<AutofillPopupRowView*> rows_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewNativeViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_NATIVE_VIEWS_H_
