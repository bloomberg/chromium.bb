// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For WinDDK ATL compatibility, these ATL headers must come first.
#include "build/build_config.h"

#if defined(OS_WIN)
#include <atlbase.h>  // NOLINT
#include <atlwin.h>  // NOLINT
#endif

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>  // NOLINT

#include "base/feature_list.h"
#include "base/i18n/bidi_line_iterator.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/common/image_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"

namespace {

// The vertical padding to provide each RenderText in addition to the height of
// the font. Where possible, RenderText uses this additional space to vertically
// center the cap height of the font instead of centering the entire font.
static const int kVerticalPadding = 4;

// Creates a views::Background for the current result style.
std::unique_ptr<views::Background> CreateBackgroundWithColor(SkColor bg_color) {
  return ui::MaterialDesignController::IsNewerMaterialUi()
             ? views::CreateSolidBackground(bg_color)
             : std::make_unique<BackgroundWith1PxBorder>(bg_color, bg_color);
}

// Returns the horizontal offset that ensures icons align vertically with the
// Omnibox icon.
int GetIconAlignmentOffset() {
  // The horizontal bounds of a result is the width of the selection highlight
  // (i.e. the views::Background). The traditional popup is designed with its
  // selection shape mimicking the internal shape of the omnibox border. Inset
  // to be consistent with the border drawn in BackgroundWith1PxBorder.
  int offset = LocationBarView::GetBorderThicknessDip();

  // The touch-optimized popup selection always fills the results frame. So to
  // align icons, inset additionally by the frame alignment inset on the left.
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    offset += RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets.left();
  return offset;
}

// Returns the padding width between elements.
int HorizontalPadding() {
  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
}

class PlaceholderImageSource : public gfx::CanvasImageSource {
 public:
  PlaceholderImageSource(const gfx::Size& canvas_size, SkColor color);
  ~PlaceholderImageSource() override;

  // CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PlaceholderImageSource);
};

PlaceholderImageSource::PlaceholderImageSource(const gfx::Size& canvas_size,
                                               SkColor color)
    : gfx::CanvasImageSource(canvas_size, false),
      color_(color),
      size_(canvas_size) {}

PlaceholderImageSource::~PlaceholderImageSource() = default;

void PlaceholderImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  canvas->sk_canvas()->drawOval(gfx::RectToSkRect(gfx::Rect(size_)), flags);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

OmniboxResultView::OmniboxResultView(OmniboxPopupContentsView* model,
                                     int model_index,
                                     const gfx::FontList& font_list)
    : model_(model),
      model_index_(model_index),
      is_hovered_(false),
      font_height_(std::max(
          font_list.GetHeight(),
          font_list.DeriveWithWeight(gfx::Font::Weight::BOLD).GetHeight())),
      animation_(new gfx::SlideAnimation(this)) {
  CHECK_GE(model_index, 0);

  AddChildView(suggestion_view_ =
                   new OmniboxMatchCellView(this, font_list, GetTextHeight()));
  AddChildView(keyword_view_ =
                   new OmniboxMatchCellView(this, font_list, GetTextHeight()));

  keyword_view_->icon()->EnableCanvasFlippingForRTLUI(true);
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));
  keyword_view_->icon()->SizeToPreferredSize();
}

OmniboxResultView::~OmniboxResultView() {}

SkColor OmniboxResultView::GetColor(OmniboxPart part) const {
  return GetOmniboxColor(part, GetTint(), GetThemeState());
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match.GetMatchWithContentsAndDescriptionPossiblySwapped();
  animation_->Reset();
  is_hovered_ = false;
  suggestion_view_->OnMatchUpdate(match_);
  keyword_view_->OnMatchUpdate(match_);

  // Set up default (placeholder) image.
  SkColor color = GetColor(OmniboxPart::RESULTS_BACKGROUND);
  extensions::image_util::ParseHexColorString(match_.image_dominant_color,
                                              &color);
  color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).
  int image_edge_length = suggestion_view_->description()->GetLineHeight();
  suggestion_view_->image()->SetImage(
      gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(
          gfx::Size(image_edge_length, image_edge_length), color));

  keyword_view_->icon()->SetVisible(match_.associated_keyword.get());

  // Set up 'switch to tab' button.
  if (match.has_tab_match && !keyword_view_->icon()->visible()) {
    suggestion_tab_switch_button_ =
        std::make_unique<OmniboxTabSwitchButton>(model_, this, GetTextHeight());
    suggestion_tab_switch_button_->set_owned_by_client();
    AddChildView(suggestion_tab_switch_button_.get());
  } else {
    suggestion_tab_switch_button_.reset();
  }

  Invalidate();
  if (GetWidget())
    Layout();
}

void OmniboxResultView::ShowKeyword(bool show_keyword) {
  if (show_keyword)
    animation_->Show();
  else
    animation_->Hide();
}

void OmniboxResultView::Invalidate() {
  // TODO(tapted): Consider using background()->SetNativeControlColor() and
  // always have a background.
  if (GetThemeState() == OmniboxPartState::NORMAL) {
    SetBackground(nullptr);
  } else {
    SkColor color = GetColor(OmniboxPart::RESULTS_BACKGROUND);
    SetBackground(CreateBackgroundWithColor(color));
  }

  // Reapply the dim color to account for the highlight state.
  suggestion_view_->separator()->Dim();
  keyword_view_->separator()->Dim();
  if (suggestion_tab_switch_button_)
    suggestion_tab_switch_button_->UpdateBackground();

  // Recreate the icons in case the color needs to change.
  // Note: if this is an extension icon or favicon then this can be done in
  //       SetMatch() once (rather than repeatedly, as happens here). There may
  //       be an optimization opportunity here.
  // TODO(dschuyler): determine whether to optimize the color changes.
  suggestion_view_->icon()->SetImage(GetIcon().ToImageSkia());
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));

  if (match_.answer) {
    suggestion_view_->content()->SetText(match_.answer->first_line());
    suggestion_view_->description()->SetText(match_.answer->second_line());
  } else {
    suggestion_view_->content()->SetText(match_.contents,
                                         match_.contents_class);
    suggestion_view_->description()->SetText(match_.description,
                                             match_.description_class);
  }

  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  keyword_view_->content()->SetVisible(keyword_match);
  keyword_view_->description()->SetVisible(keyword_match);
  if (keyword_match) {
    keyword_view_->content()->SetText(keyword_match->contents,
                                      keyword_match->contents_class);
    keyword_view_->description()->SetText(keyword_match->description,
                                          keyword_match->description_class);
    keyword_view_->description()->Dim();
  }

  // TODO(dschuyler): without this Layout call the text will shift slightly when
  // hovered. Look into removing this call (without the text shifting).
  Layout();
}

void OmniboxResultView::OnSelected() {
  DCHECK(IsSelected());

  // The text is also accessible via text/value change events in the omnibox but
  // this selection event allows the screen reader to get more details about the
  // list and the user's position within it.
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

OmniboxPartState OmniboxResultView::GetThemeState() const {
  if (IsSelected()) {
    return is_hovered_ ? OmniboxPartState::HOVERED_AND_SELECTED
                       : OmniboxPartState::SELECTED;
  }
  return is_hovered_ ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;
}

OmniboxTint OmniboxResultView::GetTint() const {
  return model_->GetTint();
}

void OmniboxResultView::OnMatchIconUpdated() {
  // The new icon will be fetched during repaint.
  SchedulePaint();
}

void OmniboxResultView::SetRichSuggestionImage(const gfx::ImageSkia& image) {
  suggestion_view_->image()->SetImage(image);
  suggestion_view_->image()->SetVisible(true);
  Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

// |button| is the tab switch button.
void OmniboxResultView::ButtonPressed(views::Button* button,
                                      const ui::Event& event) {
  OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides:

bool OmniboxResultView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    model_->SetSelectedLine(model_index_);
  return true;
}

bool OmniboxResultView::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    // When the drag enters or remains within the bounds of this view, either
    // set the state to be selected or hovered, depending on the mouse button.
    if (event.IsOnlyLeftMouseButton()) {
      if (!IsSelected())
        model_->SetSelectedLine(model_index_);
      if (suggestion_tab_switch_button_) {
        gfx::Point point_in_child_coords(event.location());
        View::ConvertPointToTarget(this, suggestion_tab_switch_button_.get(),
                                   &point_in_child_coords);
        if (suggestion_tab_switch_button_->HitTestPoint(
                point_in_child_coords)) {
          SetMouseHandler(suggestion_tab_switch_button_.get());
          return false;
        }
      }
    } else {
      SetHovered(true);
    }
    return true;
  }

  // When the drag leaves the bounds of this view, cancel the hover state and
  // pass control to the popup view.
  SetHovered(false);
  SetMouseHandler(model_);
  return false;
}

void OmniboxResultView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    OpenMatch(event.IsOnlyLeftMouseButton()
                  ? WindowOpenDisposition::CURRENT_TAB
                  : WindowOpenDisposition::NEW_BACKGROUND_TAB);
  }
}

void OmniboxResultView::OnMouseMoved(const ui::MouseEvent& event) {
  SetHovered(true);
}

void OmniboxResultView::OnMouseExited(const ui::MouseEvent& event) {
  SetHovered(false);
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Get the label without the ", n of m" positional text appended.
  // The positional info is provided via
  // ax::mojom::IntAttribute::kPosInSet/SET_SIZE and providing it via text as
  // well would result in duplicate announcements.
  node_data->SetName(
      AutocompleteMatchType::ToAccessibilityLabel(match_, match_.contents));

  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             model_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             model_->child_count());

  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());
  if (is_hovered_)
    node_data->AddState(ax::mojom::State::kHovered);
}

gfx::Size OmniboxResultView::CalculatePreferredSize() const {
  // The keyword_view_ is not added because keyword_view_ uses the same space as
  // suggestion_view_. So the 'preferred' size is just the suggestion_view_
  // size.
  return suggestion_view_->CalculatePreferredSize();
}

void OmniboxResultView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  Invalidate();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

int OmniboxResultView::GetTextHeight() const {
  return font_height_ + kVerticalPadding;
}

gfx::Image OmniboxResultView::GetIcon() const {
  return model_->GetMatchIcon(match_, GetColor(OmniboxPart::RESULTS_ICON));
}

void OmniboxResultView::SetHovered(bool hovered) {
  if (is_hovered_ != hovered) {
    is_hovered_ = hovered;
    Invalidate();
    SchedulePaint();
  }
}

bool OmniboxResultView::IsSelected() const {
  return model_->IsSelectedIndex(model_index_);
}

void OmniboxResultView::OpenMatch(WindowOpenDisposition disposition) {
  model_->OpenMatch(disposition);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

void OmniboxResultView::Layout() {
  views::View::Layout();
  // NOTE: While animating the keyword match, both matches may be visible.
  int suggestion_width = width();
  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  if (keyword_match) {
    const int icon_width = keyword_view_->icon()->width() +
                           GetIconAlignmentOffset() + (HorizontalPadding() * 2);
    const int max_kw_x = width() - icon_width;
    suggestion_width = animation_->CurrentValueBetween(max_kw_x, 0);
  }
  if (suggestion_tab_switch_button_) {
    const gfx::Size ts_button_size =
        suggestion_tab_switch_button_->GetPreferredSize();
    suggestion_tab_switch_button_->SetSize(ts_button_size);

    // It looks nice to have the same margin on top, bottom and right side.
    const int margin =
        (suggestion_view_->height() - ts_button_size.height()) / 2;
    suggestion_width -= ts_button_size.width() + margin;
    suggestion_tab_switch_button_->SetPosition(
        gfx::Point(suggestion_width, margin));
  }
  keyword_view_->SetBounds(suggestion_width, 0, width(), height());
  suggestion_view_->SetBounds(0, 0, suggestion_width, height());
}

const char* OmniboxResultView::GetClassName() const {
  return "OmniboxResultView";
}

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  animation_->SetSlideDuration(width() / 4);
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, gfx::AnimationProgressed overrides, private:

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  Layout();
  SchedulePaint();
}
