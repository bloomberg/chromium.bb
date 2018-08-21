// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/shadow_value.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// By spec, dropdowns should have a min width of 64, a max width of 456, and
// should always have a width which is a multiple of 12.
constexpr int kAutofillPopupWidthMultiple = 12;
constexpr int kAutofillPopupMinWidth = 64;
constexpr int kAutofillPopupMaxWidth = 456;

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

// A space between the input element and the dropdown, so that the dropdown's
// border doesn't look too close to the element.
constexpr int kElementBorderPadding = 1;

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
}

int GetHorizontalMargin() {
  return views::MenuConfig::instance().item_horizontal_padding +
         autofill::AutofillPopupBaseView::GetCornerRadius();
}

}  // namespace

namespace autofill {

namespace {

// Describes the possible layouts which can be applied to the rows in the popup.
enum class PopupItemLayoutType {
  kLeadingIcon,  // Icon (if any) shown on the leading (left in LTR) side.
  kTrailingIcon  // Icon (if any) shown on the trailing (right in LTR) side.
};

// By default, this returns kLeadingIcon for passwords and kTrailingIcon for all
// other contexts. When a study parameter is present for
// kAutofillDropdownLayoutExperiment, this will return the layout type which
// corresponds to that parameter.
PopupItemLayoutType GetLayoutType(int frontend_id) {
  switch (GetForcedPopupLayoutState()) {
    case ForcedPopupLayoutState::kLeadingIcon:
      return PopupItemLayoutType::kLeadingIcon;
    case ForcedPopupLayoutState::kTrailingIcon:
      return PopupItemLayoutType::kTrailingIcon;
    case ForcedPopupLayoutState::kDefault:
      switch (frontend_id) {
        case autofill::PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
        case autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
        case autofill::PopupItemId::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY:
          return PopupItemLayoutType::kLeadingIcon;
        default:
          return PopupItemLayoutType::kTrailingIcon;
      }
  }
}

// Container view that holds one child view and limits its width to the
// specified maximum.
class ConstrainedWidthView : public views::View {
 public:
  ConstrainedWidthView(views::View* child, int max_width);
  ~ConstrainedWidthView() override = default;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  int max_width_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWidthView);
};

ConstrainedWidthView::ConstrainedWidthView(views::View* child, int max_width)
    : max_width_(max_width) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(child);
}

gfx::Size ConstrainedWidthView::CalculatePreferredSize() const {
  gfx::Size size = View::CalculatePreferredSize();
  if (size.width() <= max_width_)
    return size;
  return gfx::Size(max_width_, GetHeightForWidth(max_width_));
}

// This represents a single selectable item. Subclasses distinguish between
// footer and suggestion rows, which are structurally similar but have
// distinct styling.
class AutofillPopupItemView : public AutofillPopupRowView {
 public:
  ~AutofillPopupItemView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

 protected:
  AutofillPopupItemView(AutofillPopupViewNativeViews* popup_view,
                        int line_number,
                        int extra_height = 0)
      : AutofillPopupRowView(popup_view, line_number),
        layout_type_(GetLayoutType(popup_view_->controller()
                                       ->GetSuggestionAt(line_number_)
                                       .frontend_id)),
        extra_height_(extra_height) {}

  AutofillPopupItemView(AutofillPopupViewNativeViews* popup_view,
                        int line_number,
                        PopupItemLayoutType override_layout_type,
                        int extra_height = 0)
      : AutofillPopupRowView(popup_view, line_number),
        layout_type_(override_layout_type),
        extra_height_(extra_height) {}

  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;

  virtual int GetPrimaryTextStyle() = 0;
  virtual views::View* CreateValueLabel();
  // The description view can be nullptr.
  virtual views::View* CreateDescriptionLabel();

  // Creates a description label iff the description text isn't empty.
  views::Label* CreateDescriptionLabelInternal() const;

 private:
  void AddIcon(gfx::ImageSkia icon);
  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout);

  const PopupItemLayoutType layout_type_;
  const int extra_height_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupItemView);
};

// This represents a suggestion; i.e., a row containing data that will be filled
// into the page if selected.
class AutofillPopupSuggestionView : public AutofillPopupItemView {
 public:
  ~AutofillPopupSuggestionView() override = default;

  static AutofillPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

 protected:
  // AutofillPopupItemView:
  std::unique_ptr<views::Background> CreateBackground() override;
  int GetPrimaryTextStyle() override;

  AutofillPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSuggestionView);
};

// This represents a password suggestion row; i.e., a username and password.
class PasswordPopupSuggestionView : public AutofillPopupSuggestionView {
 public:
  ~PasswordPopupSuggestionView() override = default;

  static PasswordPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

 protected:
  // AutofillPopupItemView:
  views::View* CreateValueLabel() override;
  views::View* CreateDescriptionLabel() override;

 private:
  PasswordPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number);

  DISALLOW_COPY_AND_ASSIGN(PasswordPopupSuggestionView);
};

// This represents an option which appears in the footer of the dropdown, such
// as a row which will open the Autofill settings page when selected.
class AutofillPopupFooterView : public AutofillPopupItemView {
 public:
  ~AutofillPopupFooterView() override = default;

  static AutofillPopupFooterView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

 protected:
  // AutofillPopupItemView:
  void CreateContent() override;
  std::unique_ptr<views::Background> CreateBackground() override;
  int GetPrimaryTextStyle() override;

 private:
  AutofillPopupFooterView(AutofillPopupViewNativeViews* popup_view,
                          int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupFooterView);
};

// Draws a separator between sections of the dropdown, namely between datalist
// and Autofill suggestions. Note that this is NOT the same as the border on top
// of the footer section or the border between footer items.
class AutofillPopupSeparatorView : public AutofillPopupRowView {
 public:
  ~AutofillPopupSeparatorView() override = default;

  static AutofillPopupSeparatorView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseExited(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupSeparatorView(AutofillPopupViewNativeViews* popup_view,
                             int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSeparatorView);
};

// Draws a row which contains a warning message.
class AutofillPopupWarningView : public AutofillPopupRowView {
 public:
  ~AutofillPopupWarningView() override = default;

  static AutofillPopupWarningView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override {}
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupWarningView(AutofillPopupViewNativeViews* popup_view,
                           int line_number)
      : AutofillPopupRowView(popup_view, line_number) {}

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupWarningView);
};

/************** AutofillPopupItemView **************/

void AutofillPopupItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  AutofillPopupController* controller = popup_view_->controller();
  auto suggestion = controller->GetSuggestionAt(line_number_);
  std::vector<base::string16> text;
  text.push_back(suggestion.value);
  text.push_back(suggestion.label);

  base::string16 icon_description;
  if (!suggestion.icon.empty()) {
    const int id = controller->layout_model().GetIconAccessibleNameResourceId(
        suggestion.icon);
    if (id > 0)
      text.push_back(l10n_util::GetStringUTF16(id));
  }
  node_data->SetName(base::JoinString(text, base::ASCIIToUTF16(" ")));

  // Options are selectable.
  node_data->role = ax::mojom::Role::kMenuItem;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              is_selected_);

  // Compute set size and position in set, which must not include separators.
  int set_size = 0;
  int pos_in_set = line_number_ + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).frontend_id ==
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

void AutofillPopupItemView::OnMouseEntered(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view_->controller();
  if (controller)
    controller->SetSelectedLine(line_number_);
}

void AutofillPopupItemView::OnMouseExited(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view_->controller();
  if (controller)
    controller->SelectionCleared();
}

void AutofillPopupItemView::OnMouseReleased(const ui::MouseEvent& event) {
  AutofillPopupController* controller = popup_view_->controller();
  if (controller && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    controller->AcceptSuggestion(line_number_);
  }
}

void AutofillPopupItemView::CreateContent() {
  AutofillPopupController* controller = popup_view_->controller();
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(0, GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_STRETCH);
  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height + extra_height_);

  const gfx::ImageSkia icon =
      controller->layout_model().GetIconImage(line_number_);

  if (!icon.isNull() && layout_type_ == PopupItemLayoutType::kLeadingIcon) {
    AddIcon(icon);
    AddSpacerWithSize(views::MenuConfig::instance().item_horizontal_padding,
                      /*resize=*/false, layout_manager);
  }

  AddChildView(CreateValueLabel());

  AddSpacerWithSize(views::MenuConfig::instance().item_horizontal_padding,
                    /*resize=*/true, layout_manager);

  views::View* description_label = CreateDescriptionLabel();
  if (description_label)
    AddChildView(description_label);

  if (!icon.isNull() && layout_type_ == PopupItemLayoutType::kTrailingIcon) {
    AddSpacerWithSize(views::MenuConfig::instance().item_horizontal_padding,
                      /*resize=*/false, layout_manager);
    AddIcon(icon);
  }
}

void AutofillPopupItemView::RefreshStyle() {
  SetBackground(CreateBackground());
  SchedulePaint();
}

views::View* AutofillPopupItemView::CreateValueLabel() {
  // TODO(crbug.com/831603): Remove elision responsibilities from controller.
  views::Label* text_label = new views::Label(
      popup_view_->controller()->GetElidedValueAt(line_number_),
      {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             GetPrimaryTextStyle())});
  text_label->SetEnabledColor(
      views::style::GetColor(*this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             GetPrimaryTextStyle()));
  return text_label;
}

views::View* AutofillPopupItemView::CreateDescriptionLabel() {
  return CreateDescriptionLabelInternal();
}

views::Label* AutofillPopupItemView::CreateDescriptionLabelInternal() const {
  const base::string16& description_text =
      popup_view_->controller()->GetElidedLabelAt(line_number_);
  if (!description_text.empty()) {
    views::Label* subtext_label = new views::Label(
        description_text,
        {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                               ChromeTextStyle::STYLE_SECONDARY)});
    subtext_label->SetEnabledColor(views::style::GetColor(
        *this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
        ChromeTextStyle::STYLE_SECONDARY));

    return subtext_label;
  }
  return nullptr;
}

void AutofillPopupItemView::AddIcon(gfx::ImageSkia icon) {
  auto* image_view = new views::ImageView();
  image_view->SetImage(icon);
  AddChildView(image_view);
}

void AutofillPopupItemView::AddSpacerWithSize(int spacer_width,
                                              bool resize,
                                              views::BoxLayout* layout) {
  auto* spacer = new views::View;
  spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
  AddChildView(spacer);
  layout->SetFlexForView(spacer,
                         /*flex=*/resize ? 1 : 0,
                         /*use_min_size=*/true);
}

/************** AutofillPopupSuggestionView **************/

// static
AutofillPopupSuggestionView* AutofillPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupSuggestionView* result =
      new AutofillPopupSuggestionView(popup_view, line_number);
  result->Init();
  return result;
}

std::unique_ptr<views::Background>
AutofillPopupSuggestionView::CreateBackground() {
  return views::CreateSolidBackground(
      is_selected_ ? AutofillPopupBaseView::kSelectedBackgroundColor
                   : AutofillPopupBaseView::kBackgroundColor);
}

int AutofillPopupSuggestionView::GetPrimaryTextStyle() {
  return views::style::TextStyle::STYLE_PRIMARY;
}

AutofillPopupSuggestionView::AutofillPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupItemView(popup_view, line_number) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

/************** PasswordPopupSuggestionView **************/

PasswordPopupSuggestionView* PasswordPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  PasswordPopupSuggestionView* result =
      new PasswordPopupSuggestionView(popup_view, line_number);
  result->Init();
  return result;
}

views::View* PasswordPopupSuggestionView::CreateValueLabel() {
  views::View* label = AutofillPopupSuggestionView::CreateValueLabel();
  return new ConstrainedWidthView(label, kAutofillPopupUsernameMaxWidth);
}

views::View* PasswordPopupSuggestionView::CreateDescriptionLabel() {
  views::Label* label = CreateDescriptionLabelInternal();
  if (!label)
    return nullptr;

  label->SetElideBehavior(gfx::TRUNCATE);
  return new ConstrainedWidthView(label, kAutofillPopupPasswordMaxWidth);
}

PasswordPopupSuggestionView::PasswordPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupSuggestionView(popup_view, line_number) {}

/************** AutofillPopupFooterView **************/

// static
AutofillPopupFooterView* AutofillPopupFooterView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupFooterView* result =
      new AutofillPopupFooterView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupFooterView::CreateContent() {
  SetBorder(views::CreateSolidSidedBorder(
      /*top=*/views::MenuConfig::instance().separator_thickness,
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0,
      /*color=*/AutofillPopupBaseView::kSeparatorColor));
  AutofillPopupItemView::CreateContent();
}

std::unique_ptr<views::Background> AutofillPopupFooterView::CreateBackground() {
  return views::CreateSolidBackground(
      is_selected_ ? AutofillPopupBaseView::kSelectedBackgroundColor
                   : AutofillPopupBaseView::kFooterBackgroundColor);
}

int AutofillPopupFooterView::GetPrimaryTextStyle() {
  return ChromeTextStyle::STYLE_SECONDARY;
}

AutofillPopupFooterView::AutofillPopupFooterView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupItemView(popup_view,
                            line_number,
                            PopupItemLayoutType::kTrailingIcon,
                            AutofillPopupBaseView::GetCornerRadius()) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

/************** AutofillPopupSeparatorView **************/

// static
AutofillPopupSeparatorView* AutofillPopupSeparatorView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupSeparatorView* result =
      new AutofillPopupSeparatorView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupSeparatorView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // Separators are not selectable.
  node_data->role = ax::mojom::Role::kSplitter;
}

void AutofillPopupSeparatorView::CreateContent() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Separator* separator = new views::Separator();
  separator->SetColor(AutofillPopupBaseView::kSeparatorColor);
  // Add some spacing between the the previous item and the separator.
  separator->SetPreferredHeight(
      views::MenuConfig::instance().separator_thickness);
  separator->SetBorder(views::CreateEmptyBorder(
      /*top=*/GetContentsVerticalPadding(),
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0));
  AddChildView(separator);

  SetBackground(CreateBackground());
}

void AutofillPopupSeparatorView::RefreshStyle() {
  SchedulePaint();
}

std::unique_ptr<views::Background>
AutofillPopupSeparatorView::CreateBackground() {
  return views::CreateSolidBackground(SK_ColorWHITE);
}

AutofillPopupSeparatorView::AutofillPopupSeparatorView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupRowView(popup_view, line_number) {
  SetFocusBehavior(FocusBehavior::NEVER);
}

/************** AutofillPopupWarningView **************/

// static
AutofillPopupWarningView* AutofillPopupWarningView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupWarningView* result =
      new AutofillPopupWarningView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupWarningView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  AutofillPopupController* controller = popup_view_->controller();
  if (controller)
    return;

  node_data->SetName(controller->GetSuggestionAt(line_number_).value);
  node_data->role = ax::mojom::Role::kStaticText;
}

void AutofillPopupWarningView::CreateContent() {
  AutofillPopupController* controller = popup_view_->controller();

  int horizontal_margin = GetHorizontalMargin();
  int vertical_margin = AutofillPopupBaseView::GetCornerRadius();

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_margin, horizontal_margin)));

  views::Label* text_label = new views::Label(
      controller->GetElidedValueAt(line_number_),
      {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             ChromeTextStyle::STYLE_RED)});
  text_label->SetEnabledColor(AutofillPopupBaseView::kWarningColor);
  text_label->SetMultiLine(true);
  int max_width =
      std::min(kAutofillPopupMaxWidth,
               PopupViewCommon().CalculateMaxWidth(
                   gfx::ToEnclosingRect(controller->element_bounds()),
                   controller->container_view()));
  max_width -= 2 * horizontal_margin;
  text_label->SetMaximumWidth(max_width);
  text_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  AddChildView(text_label);
}

std::unique_ptr<views::Background>
AutofillPopupWarningView::CreateBackground() {
  return views::CreateSolidBackground(SK_ColorWHITE);
}

}  // namespace

/************** AutofillPopupRowView **************/

void AutofillPopupRowView::SetSelected(bool is_selected) {
  if (is_selected == is_selected_)
    return;

  is_selected_ = is_selected;
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  RefreshStyle();
}

bool AutofillPopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

bool AutofillPopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

AutofillPopupRowView::AutofillPopupRowView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : popup_view_(popup_view), line_number_(line_number) {
  set_notify_enter_exit_on_child(true);
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
  layout_ = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  layout_->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);

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

void AutofillPopupViewNativeViews::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {
  if (previous_row_selection) {
    rows_[*previous_row_selection]->SetSelected(false);
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

  // Create one container to wrap the "regular" (non-footer) rows.
  views::View* body_container = new views::View();
  views::BoxLayout* body_layout = body_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  body_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);

  int line_number = 0;
  bool has_footer = false;

  // Process and add all the suggestions which are in the primary container.
  // Stop once the first footer item is found, or there are no more items.
  while (line_number < controller_->GetLineCount()) {
    switch (controller_->GetSuggestionAt(line_number).frontend_id) {
      case autofill::PopupItemId::POPUP_ITEM_ID_CLEAR_FORM:
      case autofill::PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS:
      case autofill::PopupItemId::POPUP_ITEM_ID_SCAN_CREDIT_CARD:
      case autofill::PopupItemId::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
      case autofill::PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
        // This is a footer, so this suggestion will be processed later. Don't
        // increment |line_number|, or else it will be skipped when adding
        // footer rows below.
        has_footer = true;
        break;

      case autofill::PopupItemId::POPUP_ITEM_ID_SEPARATOR:
        rows_.push_back(AutofillPopupSeparatorView::Create(this, line_number));
        break;

      case autofill::PopupItemId::
          POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
        rows_.push_back(AutofillPopupWarningView::Create(this, line_number));
        break;

      case autofill::PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
      case autofill::PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
        rows_.push_back(PasswordPopupSuggestionView::Create(this, line_number));
        break;

      default:
        rows_.push_back(AutofillPopupSuggestionView::Create(this, line_number));
    }

    if (has_footer)
      break;
    body_container->AddChildView(rows_.back());
    line_number++;
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  scroll_view_->SetContents(body_container);
  scroll_view_->set_draw_overflow_indicator(false);
  scroll_view_->ClipHeightTo(0, body_container->GetPreferredSize().height());

  // Use an additional container to apply padding outside the scroll view, so
  // that the padding area is stationary. This ensures that the rounded corners
  // appear properly; on Mac, the clipping path will not apply properly to a
  // scrollable area.
  // NOTE: GetContentsVerticalPadding is guaranteed to return a size which
  // accommodates the rounded corners.
  views::View* padding_wrapper = new views::View();
  padding_wrapper->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(GetContentsVerticalPadding(), 0)));
  padding_wrapper->SetLayoutManager(std::make_unique<views::FillLayout>());
  padding_wrapper->AddChildView(scroll_view_);
  AddChildView(padding_wrapper);
  layout_->SetFlexForView(padding_wrapper, 1);

  // All the remaining rows (where index >= |line_number|) are part of the
  // footer. This needs to be in its own container because it should not be
  // affected by scrolling behavior (it's "sticky") and because it has a
  // special background color.
  if (has_footer) {
    views::View* footer_container = new views::View();
    footer_container->SetBackground(
        views::CreateSolidBackground(kFooterBackgroundColor));

    views::BoxLayout* footer_layout = footer_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    footer_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);

    while (line_number < controller_->GetLineCount()) {
      rows_.push_back(AutofillPopupFooterView::Create(this, line_number));
      footer_container->AddChildView(rows_.back());
      line_number++;
    }

    AddChildView(footer_container);
    layout_->SetFlexForView(footer_container, 0);
  }
}

int AutofillPopupViewNativeViews::AdjustWidth(int width) const {
  if (width >= kAutofillPopupMaxWidth)
    return kAutofillPopupMaxWidth;

  int elem_width = gfx::ToEnclosingRect(controller_->element_bounds()).width();

  // If the element width is within the range of legal sizes for the popup, use
  // it as the min width, so that the popup will align with its edges when
  // possible.
  int min_width = (kAutofillPopupMinWidth <= elem_width &&
                   elem_width < kAutofillPopupMaxWidth)
                      ? elem_width
                      : kAutofillPopupMinWidth;

  if (width <= min_width)
    return min_width;

  // The popup size is being determined by the contents, rather than the min/max
  // or the element bounds. Round up to a multiple of
  // |kAutofillPopupWidthMultiple|.
  if (width % kAutofillPopupWidthMultiple) {
    width +=
        (kAutofillPopupWidthMultiple - (width % kAutofillPopupWidthMultiple));
  }

  return width;
}

void AutofillPopupViewNativeViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size size = CalculatePreferredSize();
  gfx::Rect popup_bounds;

  // When a bubble border is shown, the contents area (inside the shadow) is
  // supposed to be aligned with input element boundaries.
  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());
  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(/*horizontal=*/0, /*vertical=*/-kElementBorderPadding);

  PopupViewCommon().CalculatePopupVerticalBounds(size.height(), element_bounds,
                                                 controller_->container_view(),
                                                 &popup_bounds);

  // Adjust the width to compensate for a scroll bar, if necessary, and for
  // other rules.
  int scroll_width = 0;
  if (size.height() > popup_bounds.height()) {
    size.set_height(popup_bounds.height());

    // Because the preferred size is greater than the bounds available, the
    // contents will have to scroll. The scroll bar will steal width from the
    // content and smoosh everything together. Instead, add to the width to
    // compensate.
    scroll_width = scroll_view_->GetScrollBarLayoutWidth();
  }
  size.set_width(AdjustWidth(size.width() + scroll_width));

  PopupViewCommon().CalculatePopupHorizontalBounds(
      size.width(), element_bounds, controller_->container_view(),
      controller_->IsRTL(), &popup_bounds);

  SetSize(size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  SetClipPath();

  SchedulePaint();
}

}  // namespace autofill
