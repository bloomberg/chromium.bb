// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/harmony_typography_provider.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
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
const int kAutofillPopupWidthMultiple = 12;
const int kAutofillPopupMinWidth = 64;
const int kAutofillPopupMaxWidth = 456;

// TODO(crbug.com/831603): Determine how colors should be shared with menus
// and/or omnibox, and how these should interact (if at all) with native
// theme colors.
const SkColor kAutofillPopupBackgroundColor = SK_ColorWHITE;
const SkColor kAutofillPopupSelectedBackgroundColor = gfx::kGoogleGrey200;
const SkColor kAutofillPopupFooterBackgroundColor = gfx::kGoogleGrey050;
const SkColor kAutofillPopupSeparatorColor = gfx::kGoogleGrey200;
const SkColor kAutofillPopupWarningColor = gfx::kGoogleRed600;

// A space between the input element and the dropdown, so that the dropdown's
// border doesn't look too close to the element.
constexpr int kElementBorderPadding = 1;

int GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MEDIUM);
}

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
}

int GetHorizontalMargin() {
  return views::MenuConfig::instance().item_horizontal_padding +
         GetCornerRadius();
}

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
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

 protected:
  AutofillPopupItemView(AutofillPopupController* controller,
                        int line_number,
                        int extra_height = 0)
      : AutofillPopupRowView(controller, line_number),
        extra_height_(extra_height) {}

  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;

 protected:
  virtual int GetPrimaryTextStyle() = 0;

 private:
  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout);

  const int extra_height_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupItemView);
};

// This represents a suggestion; i.e., a row containing data that will be filled
// into the page if selected.
class AutofillPopupSuggestionView : public AutofillPopupItemView {
 public:
  ~AutofillPopupSuggestionView() override = default;

  static AutofillPopupSuggestionView* Create(
      AutofillPopupController* controller,
      int line_number);

 protected:
  // AutofillPopupItemView:
  std::unique_ptr<views::Background> CreateBackground() override;
  int GetPrimaryTextStyle() override;

 private:
  AutofillPopupSuggestionView(AutofillPopupController* controller,
                              int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSuggestionView);
};

// This represents an option which appears in the footer of the dropdown, such
// as a row which will open the Autofill settings page when selected.
class AutofillPopupFooterView : public AutofillPopupItemView {
 public:
  ~AutofillPopupFooterView() override = default;

  static AutofillPopupFooterView* Create(AutofillPopupController* controller,
                                         int line_number);

 protected:
  // AutofillPopupItemView:
  void CreateContent() override;
  std::unique_ptr<views::Background> CreateBackground() override;
  int GetPrimaryTextStyle() override;

 private:
  AutofillPopupFooterView(AutofillPopupController* controller, int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupFooterView);
};

// Draws a separator between sections of the dropdown, namely between datalist
// and Autofill suggestions. Note that this is NOT the same as the border on top
// of the footer section or the border between footer items.
class AutofillPopupSeparatorView : public AutofillPopupRowView {
 public:
  ~AutofillPopupSeparatorView() override = default;

  static AutofillPopupSeparatorView* Create(AutofillPopupController* controller,
                                            int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupSeparatorView(AutofillPopupController* controller,
                             int line_number);

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupSeparatorView);
};

// Draws a row which contains a warning message.
class AutofillPopupWarningView : public AutofillPopupRowView {
 public:
  ~AutofillPopupWarningView() override = default;

  static AutofillPopupWarningView* Create(AutofillPopupController* controller,
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
  AutofillPopupWarningView(AutofillPopupController* controller, int line_number)
      : AutofillPopupRowView(controller, line_number) {}

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupWarningView);
};

void AutofillPopupItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  auto suggestion = controller_->GetSuggestionAt(line_number_);
  std::vector<base::string16> text;
  text.push_back(suggestion.value);
  text.push_back(suggestion.label);

  base::string16 icon_description;
  if (!suggestion.icon.empty()) {
    const int id = controller_->layout_model().GetIconAccessibleNameResourceId(
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

void AutofillPopupItemView::OnMouseEntered(const ui::MouseEvent& event) {
  controller_->SetSelectedLine(line_number_);
}

void AutofillPopupItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    controller_->AcceptSuggestion(line_number_);
}

void AutofillPopupItemView::CreateContent() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(0, GetHorizontalMargin())));

  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height + extra_height_);

  // TODO(crbug.com/831603): Remove elision responsibilities from controller.
  views::Label* text_label = new views::Label(
      controller_->GetElidedValueAt(line_number_),
      {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             GetPrimaryTextStyle())});
  text_label->SetEnabledColor(
      views::style::GetColor(*this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             GetPrimaryTextStyle()));

  AddChildView(text_label);

  AddSpacerWithSize(views::MenuConfig::instance().item_horizontal_padding,
                    /*resize=*/true, layout);

  const base::string16& description_text =
      controller_->GetElidedLabelAt(line_number_);
  if (!description_text.empty()) {
    views::Label* subtext_label = new views::Label(
        description_text,
        {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                               ChromeTextStyle::STYLE_SECONDARY)});
    subtext_label->SetEnabledColor(views::style::GetColor(
        *this, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
        ChromeTextStyle::STYLE_SECONDARY));

    AddChildView(subtext_label);
  }

  const gfx::ImageSkia icon =
      controller_->layout_model().GetIconImage(line_number_);
  if (!icon.isNull()) {
    AddSpacerWithSize(views::MenuConfig::instance().item_horizontal_padding,
                      /*resize=*/false, layout);

    auto* image_view = new views::ImageView();
    image_view->SetImage(icon);

    AddChildView(image_view);
  }
}

void AutofillPopupItemView::RefreshStyle() {
  SetBackground(CreateBackground());
  SchedulePaint();
}

void AutofillPopupItemView::AddSpacerWithSize(int spacer_width,
                                              bool resize,
                                              views::BoxLayout* layout) {
  auto* spacer = new views::View;
  spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
  AddChildView(spacer);
  layout->SetFlexForView(spacer, /*flex=*/resize ? 1 : 0);
}

// static
AutofillPopupSuggestionView* AutofillPopupSuggestionView::Create(
    AutofillPopupController* controller,
    int line_number) {
  AutofillPopupSuggestionView* result =
      new AutofillPopupSuggestionView(controller, line_number);
  result->Init();
  return result;
}

std::unique_ptr<views::Background>
AutofillPopupSuggestionView::CreateBackground() {
  return views::CreateSolidBackground(
      is_selected_ ? kAutofillPopupSelectedBackgroundColor
                   : kAutofillPopupBackgroundColor);
}

int AutofillPopupSuggestionView::GetPrimaryTextStyle() {
  return views::style::TextStyle::STYLE_PRIMARY;
}

AutofillPopupSuggestionView::AutofillPopupSuggestionView(
    AutofillPopupController* controller,
    int line_number)
    : AutofillPopupItemView(controller, line_number) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

// static
AutofillPopupFooterView* AutofillPopupFooterView::Create(
    AutofillPopupController* controller,
    int line_number) {
  AutofillPopupFooterView* result =
      new AutofillPopupFooterView(controller, line_number);
  result->Init();
  return result;
}

void AutofillPopupFooterView::CreateContent() {
  SetBorder(views::CreateSolidSidedBorder(
      /*top=*/views::MenuConfig::instance().separator_thickness,
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0,
      /*color=*/kAutofillPopupSeparatorColor));
  AutofillPopupItemView::CreateContent();
}

std::unique_ptr<views::Background> AutofillPopupFooterView::CreateBackground() {
  return views::CreateSolidBackground(
      is_selected_ ? kAutofillPopupSelectedBackgroundColor
                   : kAutofillPopupFooterBackgroundColor);
}

int AutofillPopupFooterView::GetPrimaryTextStyle() {
  return ChromeTextStyle::STYLE_SECONDARY;
}

AutofillPopupFooterView::AutofillPopupFooterView(
    AutofillPopupController* controller,
    int line_number)
    : AutofillPopupItemView(controller, line_number, GetCornerRadius()) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

// static
AutofillPopupSeparatorView* AutofillPopupSeparatorView::Create(
    AutofillPopupController* controller,
    int line_number) {
  AutofillPopupSeparatorView* result =
      new AutofillPopupSeparatorView(controller, line_number);
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
  separator->SetColor(kAutofillPopupSeparatorColor);
  // Add some spacing between the the previous item and the separator.
  separator->SetPreferredHeight(
      views::MenuConfig::instance().separator_thickness);
  separator->SetBorder(views::CreateEmptyBorder(
      /*top=*/GetContentsVerticalPadding(),
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0));
  AddChildView(separator);
}

void AutofillPopupSeparatorView::RefreshStyle() {
  SchedulePaint();
}

std::unique_ptr<views::Background>
AutofillPopupSeparatorView::CreateBackground() {
  return views::CreateSolidBackground(SK_ColorWHITE);
}

AutofillPopupSeparatorView::AutofillPopupSeparatorView(
    AutofillPopupController* controller,
    int line_number)
    : AutofillPopupRowView(controller, line_number) {
  SetFocusBehavior(FocusBehavior::NEVER);
}

// static
AutofillPopupWarningView* AutofillPopupWarningView::Create(
    AutofillPopupController* controller,
    int line_number) {
  AutofillPopupWarningView* result =
      new AutofillPopupWarningView(controller, line_number);
  result->Init();
  return result;
}

void AutofillPopupWarningView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetName(controller_->GetSuggestionAt(line_number_).value);
  node_data->role = ax::mojom::Role::kStaticText;
}

void AutofillPopupWarningView::CreateContent() {
  int horizontal_margin = GetHorizontalMargin();
  int vertical_margin = GetCornerRadius();

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_margin, horizontal_margin)));

  views::Label* text_label = new views::Label(
      controller_->GetElidedValueAt(line_number_),
      {views::style::GetFont(ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
                             ChromeTextStyle::STYLE_RED)});
  text_label->SetEnabledColor(kAutofillPopupWarningColor);
  text_label->SetMultiLine(true);
  int max_width =
      std::min(kAutofillPopupMaxWidth,
               PopupViewCommon().CalculateMaxWidth(
                   gfx::ToEnclosingRect(controller_->element_bounds()),
                   controller_->container_view()));
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

AutofillPopupRowView::AutofillPopupRowView(AutofillPopupController* controller,
                                           int line_number)
    : controller_(controller), line_number_(line_number) {}

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
  SetBackground(views::CreateSolidBackground(kAutofillPopupBackgroundColor));
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

void AutofillPopupViewNativeViews::VisibilityChanged(View* starting_from,
                                                     bool is_visible) {
  if (is_visible) {
    // TODO(https://crbug.com/848427) Call this when suggestions become
    // available at all, even if it not currently visible.
    ui::AXPlatformNode::OnInputSuggestionsAvailable();
    // Fire these the first time a menu is visible. By firing these and the
    // matching end events, we are telling screen readers that the focus
    // is only changing temporarily, and the screen reader will restore the
    // focus back to the appropriate textfield when the menu closes.
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
  } else {
    // TODO(https://crbug.com/848427) Only call if suggestions are actually no
    // longer available. The suggestions could be hidden but still available, as
    // is the case when the Escape key is pressed.
    ui::AXPlatformNode::OnInputSuggestionsUnavailable();
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
  }
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
  views::BoxLayout* body_layout =
      body_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical,
          gfx::Insets(GetContentsVerticalPadding(), 0)));
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
        rows_.push_back(
            AutofillPopupSeparatorView::Create(controller_, line_number));
        break;

      case autofill::PopupItemId::
          POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
        rows_.push_back(
            AutofillPopupWarningView::Create(controller_, line_number));
        break;

      default:
        rows_.push_back(
            AutofillPopupSuggestionView::Create(controller_, line_number));
    }

    if (has_footer)
      break;
    body_container->AddChildView(rows_.back());
    line_number++;
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->set_hide_horizontal_scrollbar(true);
  scroll_view_->SetContents(body_container);
  AddChildView(scroll_view_);
  layout_->SetFlexForView(scroll_view_, 1);
  scroll_view_->ClipHeightTo(0, body_container->GetPreferredSize().height());

  // All the remaining rows (where index >= |line_number|) are part of the
  // footer. This needs to be in its own container because it should not be
  // affected by scrolling behavior (it's "sticky") and because it has a
  // special background color.
  if (has_footer) {
    views::View* footer_container = new views::View();
    footer_container->SetBackground(
        views::CreateSolidBackground(kAutofillPopupFooterBackgroundColor));

    views::BoxLayout* footer_layout = footer_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    footer_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);

    while (line_number < controller_->GetLineCount()) {
      rows_.push_back(
          AutofillPopupFooterView::Create(controller_, line_number));
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

void AutofillPopupViewNativeViews::AddExtraInitParams(
    views::Widget::InitParams* params) {
  // Ensure the bubble border is not painted on an opaque background.
  params->opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
}

std::unique_ptr<views::View> AutofillPopupViewNativeViews::CreateWrapperView() {
  // Create a wrapper view that contains the current view and will receive the
  // bubble border. This is needed so that a clipping path can be later applied
  // on the contents only and not affect the border.
  auto wrapper_view = std::make_unique<views::View>();
  wrapper_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  wrapper_view->AddChildView(this);
  return wrapper_view;
}

std::unique_ptr<views::Border> AutofillPopupViewNativeViews::CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW,
      SK_ColorWHITE);
  border->SetCornerRadius(GetCornerRadius());
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          views::EMPHASIS_MEDIUM));
  bubble_border_ = border.get();
  return border;
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

  popup_bounds.Inset(-bubble_border_->GetInsets());

  GetWidget()->SetBounds(popup_bounds);

  // Ensure the child views are not rendered beyond the bubble border
  // boundaries.
  SkRect local_bounds = gfx::RectToSkRect(GetLocalBounds());
  SkScalar radius = SkIntToScalar(bubble_border_->GetBorderCornerRadius());
  gfx::Path clip_path;
  clip_path.addRoundRect(local_bounds, radius, radius);
  set_clip_path(clip_path);

  SchedulePaint();
}

}  // namespace autofill
