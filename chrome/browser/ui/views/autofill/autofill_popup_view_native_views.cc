// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/harmony_typography_provider.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// By spec, dropdowns should have a min width of 64, and should always have
// a width which is a multiple of 12.
const int kDropdownWidthMultiple = 12;
const int kDropdownMinWidth = 64;

// TODO(crbug.com/768881): Determine how colors should be shared with menus
// and/or omnibox, and how these should interact (if at all) with native
// theme colors.
const SkColor kBackgroundColor = SK_ColorWHITE;
const SkColor kSelectedBackgroundColor = gfx::kGoogleGrey200;
const SkColor kFooterBackgroundColor = gfx::kGoogleGrey050;
const SkColor kSeparatorColor = gfx::kGoogleGrey200;

}  // namespace

namespace autofill {

namespace {

// This represents a single selectable item. Subclasses distinguish between
// footer and suggestion rows, which are structurally similar but have
// distinct styling.
class AutofillPopupItemView : public AutofillPopupRowView {
 public:
  ~AutofillPopupItemView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(controller_->GetSuggestionAt(line_number_).value);

    // Options are selectable.
    node_data->role = ax::mojom::Role::kMenuItem;
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                is_selected_);

    // Compute set size and position in set, which must not include separators.
    int set_size = 0;
    int pos_in_set = line_number_ + 1;
    for (int i = 0; i < controller_->GetLineCount(); ++i) {
      if (controller_->GetSuggestionAt(i).frontend_id ==
          autofill::POPUP_ITEM_ID_SEPARATOR) {
        if (i < line_number_)
          --pos_in_set;
      } else {
        ++set_size;
      }
    }
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    controller_->SetSelectedLine(line_number_);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
      controller_->AcceptSuggestion(line_number_);
  }

 protected:
  AutofillPopupItemView(AutofillPopupController* controller, int line_number)
      : AutofillPopupRowView(controller, line_number) {}

  // AutofillPopupRowView:
  void CreateContent() override {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kHorizontal,
        gfx::Insets(0, views::MenuConfig::instance().item_left_margin)));

    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);
    layout->set_minimum_cross_axis_size(
        views::MenuConfig::instance().touchable_menu_height);

    // TODO(crbug.com/831603): Remove elision responsibilities from controller.
    text_label_ = new views::Label(
        controller_->GetElidedValueAt(line_number_),
        {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                               views::style::TextStyle::STYLE_PRIMARY)});

    AddChildView(text_label_);

    auto* spacer = new views::View;
    spacer->SetPreferredSize(gfx::Size(
        views::MenuConfig::instance().label_to_minor_text_padding, 1));
    AddChildView(spacer);
    layout->SetFlexForView(spacer, /*flex*/ 1);

    const base::string16& description_text =
        controller_->GetElidedLabelAt(line_number_);
    if (!description_text.empty()) {
      subtext_label_ = new views::Label(
          description_text,
          {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                                 ChromeTextStyle::STYLE_HINT)});

      AddChildView(subtext_label_);
    }

    const gfx::ImageSkia icon =
        controller_->layout_model().GetIconImage(line_number_);
    if (!icon.isNull()) {
      auto* image_view = new views::ImageView();
      image_view->SetImage(icon);

      image_view->SetBorder(views::CreateEmptyBorder(
          0, views::MenuConfig::instance().icon_to_label_padding, 0, 0));
      AddChildView(image_view);
    }
  }

  void RefreshStyle() override {
    SetBackground(CreateBackground());

    if (text_label_) {
      int text_style = is_warning_ ? ChromeTextStyle::STYLE_RED
                                   : views::style::TextStyle::STYLE_PRIMARY;

      text_label_->SetEnabledColor(views::style::GetColor(
          *this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE, text_style));
    }

    if (subtext_label_) {
      SkColor secondary = views::style::GetColor(
          *this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
          ChromeTextStyle::STYLE_HINT);
      subtext_label_->SetEnabledColor(secondary);
    }

    SchedulePaint();
  }

 private:
  views::Label* text_label_ = nullptr;
  views::Label* subtext_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupItemView);
};

class AutofillPopupSuggestionView : public AutofillPopupItemView {
 public:
  ~AutofillPopupSuggestionView() override = default;

  static AutofillPopupSuggestionView* Create(
      AutofillPopupController* controller,
      int line_number) {
    AutofillPopupSuggestionView* result =
        new AutofillPopupSuggestionView(controller, line_number);
    result->Init();
    return result;
  }

 protected:
  // AutofillPopupRowView:
  std::unique_ptr<views::Background> CreateBackground() override {
    return views::CreateSolidBackground(is_selected_ ? kSelectedBackgroundColor
                                                     : kBackgroundColor);
  }

 private:
  AutofillPopupSuggestionView(AutofillPopupController* controller,
                              int line_number)
      : AutofillPopupItemView(controller, line_number) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSuggestionView);
};

class AutofillPopupFooterView : public AutofillPopupItemView {
 public:
  ~AutofillPopupFooterView() override = default;

  static AutofillPopupFooterView* Create(AutofillPopupController* controller,
                                         int line_number) {
    AutofillPopupFooterView* result =
        new AutofillPopupFooterView(controller, line_number);
    result->Init();
    return result;
  }

 protected:
  // AutofillPopupRowView:
  std::unique_ptr<views::Background> CreateBackground() override {
    return views::CreateSolidBackground(is_selected_ ? kSelectedBackgroundColor
                                                     : kFooterBackgroundColor);
  }

 private:
  AutofillPopupFooterView(AutofillPopupController* controller, int line_number)
      : AutofillPopupItemView(controller, line_number) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupFooterView);
};

class AutofillPopupSeparatorView : public AutofillPopupRowView {
 public:
  ~AutofillPopupSeparatorView() override = default;

  static AutofillPopupSeparatorView* Create(AutofillPopupController* controller,
                                            int line_number) {
    AutofillPopupSeparatorView* result =
        new AutofillPopupSeparatorView(controller, line_number);
    result->Init();
    return result;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Separators are not selectable.
    node_data->role = ax::mojom::Role::kSplitter;
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    RefreshStyle();
  }

 protected:
  // AutofillPopupRowView:
  void CreateContent() override {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // In order to draw the horizontal line for the separator, create an empty
    // view which matches the width of the parent (by using full flex) and has a
    // height equal to the desired padding, then paint a border on the bottom.
    empty_ = new views::View();
    empty_->SetPreferredSize(
        gfx::Size(1, views::MenuConfig::instance().menu_vertical_border_size));
    AddChildView(empty_);
  }

  void RefreshStyle() override {
    empty_->SetBorder(views::CreateSolidSidedBorder(
        /*top=*/0,
        /*left=*/0,
        /*bottom=*/views::MenuConfig::instance().separator_thickness,
        /*right=*/0,
        /*color=*/kSeparatorColor));
    SchedulePaint();
  }

  std::unique_ptr<views::Background> CreateBackground() override {
    return views::CreateSolidBackground(SK_ColorWHITE);
  }

 private:
  AutofillPopupSeparatorView(AutofillPopupController* controller,
                             int line_number)
      : AutofillPopupRowView(controller, line_number) {
    SetFocusBehavior(FocusBehavior::NEVER);
  }

  views::View* empty_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSeparatorView);
};

}  // namespace

void AutofillPopupRowView::SetSelected(bool is_selected) {
  if (is_selected == is_selected_)
    return;

  is_selected_ = is_selected;
  NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  RefreshStyle();
}

bool AutofillPopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

bool AutofillPopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

AutofillPopupRowView::AutofillPopupRowView(AutofillPopupController* controller,
                                           int line_number)
    : controller_(controller), line_number_(line_number) {
  int frontend_id = controller_->GetSuggestionAt(line_number_).frontend_id;
  is_warning_ =
      frontend_id ==
      autofill::POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
}

void AutofillPopupRowView::Init() {
  CreateContent();
  RefreshStyle();
}

AutofillPopupViewNativeViews::AutofillPopupViewNativeViews(
    AutofillPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  // TODO(crbug.com/768881): kPopupTopBottomPadding is not needed on the bottom
  // when a footer is present.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical,
          gfx::Insets(views::MenuConfig::instance().menu_vertical_border_size,
                      0)));
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);

  CreateChildViews();
  SetBackground(views::CreateSolidBackground(kBackgroundColor));
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

gfx::Size AutofillPopupViewNativeViews::CalculatePreferredSize() const {
  // The border of the input element should be aligned with the border of the
  // dropdown when suggestions are not too wide.
  int contents_width =
      gfx::ToEnclosingRect(controller_->element_bounds()).width();

  // Compute the internal width needed for the contents by discounting the
  // width required for the border. This will ensure that, once added, the
  // dropdown border aligns with the element's border.
  contents_width -= GetWidget()->GetRootView()->GetInsets().width();

  // Allow the dropdown to grow beyond the element width if it requires more
  // horizontal space to render the suggestions.
  gfx::Size size = AutofillPopupBaseView::CalculatePreferredSize();
  if (contents_width < size.width()) {
    contents_width = size.width();
    // Use multiples of |kDropdownWidthMultiple| if the required width is larger
    // than the element width.
    if (contents_width % kDropdownWidthMultiple) {
      contents_width +=
          kDropdownWidthMultiple - (contents_width % kDropdownWidthMultiple);
    }
  }

  // Notwithstanding all the above rules, enforce a hard minimum so the dropdown
  // is not too small to interact with.
  size.set_width(std::max(kDropdownMinWidth, contents_width));
  return size;
}

void AutofillPopupViewNativeViews::VisibilityChanged(View* starting_from,
                                                     bool is_visible) {
  if (is_visible) {
    GetViewAccessibility().OnAutofillShown();
  } else {
    GetViewAccessibility().OnAutofillHidden();
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
  }
}

void AutofillPopupViewNativeViews::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {
  if (previous_row_selection) {
    rows_[*previous_row_selection]->SetSelected(false);
  } else {
    // Fire this the first time a row is selected. By firing this and the
    // matching kMenuEnd event, we are telling screen readers that the focus
    // is only changing temporarily, and the screen reader will restore the
    // focus back to the appropriate textfield when the menu closes.
    // This is deferred until the first focus so that the screen reader doesn't
    // treat the textfield as unfocused while the user edits, just because
    // autofill options are visible.
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
  }

  if (current_row_selection)
    rows_[*current_row_selection]->SetSelected(true);
}

void AutofillPopupViewNativeViews::OnSuggestionsChanged() {
  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewNativeViews::CreateChildViews() {
  RemoveAllChildViews(true /* delete_children */);
  rows_.clear();
  for (int i = 0; i < controller_->GetLineCount(); ++i) {
    switch (controller_->GetSuggestionAt(i).frontend_id) {
      case autofill::PopupItemId::POPUP_ITEM_ID_SEPARATOR:
        rows_.push_back(AutofillPopupSeparatorView::Create(controller_, i));
        break;
      case autofill::PopupItemId::POPUP_ITEM_ID_CLEAR_FORM:
      case autofill::PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS:
        rows_.push_back(AutofillPopupFooterView::Create(controller_, i));
        break;
      default:
        rows_.push_back(AutofillPopupSuggestionView::Create(controller_, i));
    }
    AddChildView(rows_.back());
  }
}

void AutofillPopupViewNativeViews::DoUpdateBoundsAndRedrawPopup() {
  SizeToPreferredSize();

  // The Widget's bounds need to account for the border.
  gfx::Insets insets = GetWidget()->GetRootView()->GetInsets();
  gfx::Rect popup_bounds = PopupViewCommon().CalculatePopupBounds(
      size().width() + insets.width(), size().height() + insets.height(),
      gfx::ToEnclosingRect(controller_->element_bounds()),
      controller_->container_view(), controller_->IsRTL());

  GetWidget()->SetBounds(popup_bounds);

  SchedulePaint();
}

}  // namespace autofill
