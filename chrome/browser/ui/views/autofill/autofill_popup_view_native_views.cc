// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// Child view only for triggering accessibility events. Rendering for every
// suggestion type is handled separately.
class AutofillPopupChildView : public views::View {
 public:
  AutofillPopupChildView(AutofillPopupController* controller, int line_number)
      : controller_(controller), line_number_(line_number) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  ~AutofillPopupChildView() override {}

  void ClearSelection() {}
  void AcceptSelection() { controller_->AcceptSuggestion(line_number_); }

  // views::Views implementation.
  // TODO(melandory): These actions are duplicates of what is implemented in
  // AutofillPopupBaseView. Once migration is finished, code in
  // AutofillPopupBaseView should be cleaned up.
  void OnMouseCaptureLost() override { ClearSelection(); }

  bool OnMouseDragged(const ui::MouseEvent& event) override { return true; }

  void OnMouseExited(const ui::MouseEvent& event) override {}

  void OnMouseMoved(const ui::MouseEvent& event) override {}

  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
      AcceptSelection();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {}

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    return true;
  }

 private:
  // views::Views implementation
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_MENU_ITEM;
    node_data->SetName(controller_->GetSuggestionAt(line_number_).value);
  }

  AutofillPopupController* controller_;
  int line_number_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupChildView);
};

}  // namespace

AutofillPopupViewNativeViews::AutofillPopupViewNativeViews(
    AutofillPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  SetLayoutManager(box_layout);

  CreateChildViews();
  SetBackground(views::CreateThemedSolidBackground(
      this, ui::NativeTheme::kColorId_ResultsTableNormalBackground));
}

AutofillPopupViewNativeViews::~AutofillPopupViewNativeViews() {}

void AutofillPopupViewNativeViews::Show() {
  DoShow();
}

void AutofillPopupViewNativeViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;

  DoHide();
}

void AutofillPopupViewNativeViews::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {}

void AutofillPopupViewNativeViews::OnSuggestionsChanged() {
  DoUpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewNativeViews::CreateChildViews() {
  RemoveAllChildViews(true /* delete_children */);
  for (int i = 0; i < controller_->GetLineCount(); ++i) {
    views::View* child_view = new AutofillPopupChildView(controller_, i);
    if (controller_->GetSuggestionAt(i).frontend_id !=
        autofill::POPUP_ITEM_ID_SEPARATOR) {
      // Ignore separators for now.
      // TODO(melandory): implement the drawing for the separator.
      views::Label* label = new views::Label(
          controller_->GetElidedValueAt(i),
          {controller_->layout_model().GetValueFontListForRow(i)});
      label->SetBackground(
          views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
              controller_->GetBackgroundColorIDForRow(i))));
      label->SetEnabledColor(GetNativeTheme()->GetSystemColor(
          controller_->layout_model().GetValueFontColorIDForRow(i)));
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      child_view->AddChildView(label);

      auto* box_layout =
          new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(4, 13));
      box_layout->set_main_axis_alignment(
          views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
      child_view->SetLayoutManager(box_layout);
    }
    AddChildView(child_view);
  }
}

}  // namespace autofill
