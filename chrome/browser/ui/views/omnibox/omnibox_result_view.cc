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
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/vector_icons.h"
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
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_utils.h"

namespace {

// The vertical margin that should be used above and below each suggestion.
static const int kVerticalMargin = 1;

// The vertical padding to provide each RenderText in addition to the height of
// the font. Where possible, RenderText uses this additional space to vertically
// center the cap height of the font instead of centering the entire font.
static const int kVerticalPadding = 4;

// TODO(dschuyler): Perhaps this should be based on the font size
// instead of hardcoded to 2 dp (e.g. by adding a space in an
// appropriate font to the beginning of the description, then reducing
// the additional padding here to zero).
static const int kAnswerIconToTextPadding = 2;

// Whether to use the two-line layout.
bool IsTwoLineLayout() {
  return base::FeatureList::IsEnabled(omnibox::kUIExperimentVerticalLayout);
}

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
  int offset = BackgroundWith1PxBorder::kLocationBarBorderThicknessDip;

  // The touch-optimized popup selection always fills the results frame. So to
  // align icons, inset additionally by the frame alignment inset on the left.
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    offset += RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets.left();
  return offset;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxImageView:

class OmniboxImageView : public views::ImageView {
 public:
  bool CanProcessEventsWithinSubtree() const override { return false; }
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxSeparatedLineView:

class OmniboxSeparatedLineView : public views::View {
 public:
  explicit OmniboxSeparatedLineView(OmniboxResultView* result_view,
                                    const gfx::FontList& font_list);
  ~OmniboxSeparatedLineView() override;

  views::ImageView* icon() { return icon_view_; }
  views::ImageView* image() { return image_view_; }
  OmniboxTextView* content() { return content_view_; }
  OmniboxTextView* description() { return description_view_; }
  OmniboxTextView* separator() { return separator_view_; }

  void OnMatchUpdate(const AutocompleteMatch& match);
  void OnHighlightUpdate(const AutocompleteMatch& match);

 protected:
  // views::View:
  void Layout() override;
  const char* GetClassName() const override;

  // Weak pointers for easy reference.
  OmniboxResultView* result_view_;
  views::ImageView* icon_view_;   // An icon resembling a '>'.
  views::ImageView* image_view_;  // For rich suggestions.
  OmniboxTextView* content_view_;
  OmniboxTextView* description_view_;
  OmniboxTextView* separator_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxSeparatedLineView);
};

OmniboxSeparatedLineView::OmniboxSeparatedLineView(
    OmniboxResultView* result_view,
    const gfx::FontList& font_list)
    : result_view_(result_view) {
  AddChildView(icon_view_ = new OmniboxImageView());
  AddChildView(image_view_ = new OmniboxImageView());
  AddChildView(content_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(description_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(separator_view_ = new OmniboxTextView(result_view, font_list));
}

OmniboxSeparatedLineView::~OmniboxSeparatedLineView() = default;

void OmniboxSeparatedLineView::OnMatchUpdate(const AutocompleteMatch& match) {
  // TODO(dschuyler): Fill this in.
}

void OmniboxSeparatedLineView::OnHighlightUpdate(
    const AutocompleteMatch& match) {
  // TODO(dschuyler): Fill this in.
}

const char* OmniboxSeparatedLineView::GetClassName() const {
  return "OmniboxSeparatedLineView";
}

void OmniboxSeparatedLineView::Layout() {
  // TODO(dschuyler): Fill this in.
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxSuggestionView:

class OmniboxSuggestionView : public OmniboxSeparatedLineView {
 public:
  explicit OmniboxSuggestionView(OmniboxResultView* result_view,
                                 const gfx::FontList& font_list);
  ~OmniboxSuggestionView() override;

 private:
  // views::View:
  void Layout() override;
  const char* GetClassName() const override;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSuggestionView);
};

OmniboxSuggestionView::OmniboxSuggestionView(OmniboxResultView* result_view,
                                             const gfx::FontList& font_list)
    : OmniboxSeparatedLineView::OmniboxSeparatedLineView(result_view,
                                                         font_list) {}

OmniboxSuggestionView::~OmniboxSuggestionView() = default;

const char* OmniboxSuggestionView::GetClassName() const {
  return "OmniboxSuggestionView";
}

void OmniboxSuggestionView::Layout() {
  // TODO(dschuyler): Fill this in.
}

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

  AddChildView(suggestion_view_ = new OmniboxSuggestionView(this, font_list));
  AddChildView(keyword_view_ = new OmniboxSeparatedLineView(this, font_list));

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

  suggestion_view_->image()->SetVisible(
      false);  // Until SetAnswerImage is called.
  keyword_view_->icon()->SetVisible(match_.associated_keyword.get());

  if (OmniboxFieldTrial::InTabSwitchSuggestionWithButtonTrial() &&
      match.type == AutocompleteMatchType::TAB_SEARCH &&
      !keyword_view_->icon()->visible()) {
    suggestion_tab_switch_button_ =
        std::make_unique<OmniboxTabSwitchButton>(this, GetTextHeight());
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

  suggestion_view_->OnHighlightUpdate(match_);
  keyword_view_->OnHighlightUpdate(match_);

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

  const base::string16& separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  suggestion_view_->separator()->SetText(separator);
  suggestion_view_->separator()->Dim();
  keyword_view_->separator()->SetText(separator);
  keyword_view_->separator()->Dim();

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

void OmniboxResultView::SetAnswerImage(const gfx::ImageSkia& image) {
  suggestion_view_->image()->SetImage(image);
  suggestion_view_->image()->SetVisible(true);
  Layout();
  SchedulePaint();
}

void OmniboxResultView::OpenMatch(WindowOpenDisposition disposition) {
  model_->OpenMatch(model_index_, disposition);
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

  node_data->AddState(ax::mojom::State::kSelectable);
  if (IsSelected())
    node_data->AddState(ax::mojom::State::kSelected);
  if (is_hovered_)
    node_data->AddState(ax::mojom::State::kHovered);
}

gfx::Size OmniboxResultView::CalculatePreferredSize() const {
  int height = GetTextHeight() + (2 * GetVerticalMargin());
  if (match_.answer)
    height += GetAnswerHeight();
  else if (IsTwoLineLayout())
    height += GetTextHeight();
  return gfx::Size(0, height);
}

void OmniboxResultView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  Invalidate();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

OmniboxImageView* OmniboxResultView::AddOmniboxImageView() {
  OmniboxImageView* view = new OmniboxImageView();
  AddChildView(view);
  return view;
}

OmniboxTextView* OmniboxResultView::AddOmniboxTextView(
    const gfx::FontList& font_list) {
  OmniboxTextView* view = new OmniboxTextView(this, font_list);
  AddChildView(view);
  return view;
}

int OmniboxResultView::GetTextHeight() const {
  return font_height_ + kVerticalPadding;
}

gfx::Image OmniboxResultView::GetIcon() const {
  return model_->GetMatchIcon(match_, GetColor(OmniboxPart::RESULTS_ICON));
}

int OmniboxResultView::GetAnswerHeight() const {
  const int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
      GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
  const gfx::Image icon = GetIcon();
  int icon_width = icon.Width();
  int answer_icon_size =
      suggestion_view_->image()->visible()
          ? suggestion_view_->image()->height() + kAnswerIconToTextPadding
          : 0;
  // TODO(dschuyler): The GetIconAlignmentOffset() is applied an extra time to
  // match the math in Layout(). This seems like a (minor) mistake.
  int deduction = (GetIconAlignmentOffset() * 2) + icon_width +
                  (horizontal_padding * 3) + answer_icon_size;
  int description_width = std::max(width() - deduction, 0);
  return suggestion_view_->description()->GetHeightForWidth(description_width) +
         kVerticalPadding;
}

int OmniboxResultView::GetVerticalMargin() const {
  // Regardless of the text size, we ensure a minimum size for the content line
  // here. This minimum is larger for hybrid mouse/touch devices to ensure an
  // adequately sized touch target.
  using Md = ui::MaterialDesignController;
  const int kIconVerticalPad = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentVerticalMargin,
      OmniboxFieldTrial::kUIVerticalMarginParam,
      Md::GetMode() == Md::MATERIAL_HYBRID ? 8 : 4);
  int min_height =
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE) + (kIconVerticalPad * 2);

  if (Md::IsTouchOptimizedUiEnabled()) {
    // The touchable spec specifies a height of 44 DIP. Ensure that's satisfied.
    // Answer rows can still exceed this size.
    constexpr int kTouchOptimizedResultHeight = 44;
    min_height = std::max(min_height, kTouchOptimizedResultHeight);
  }

  return std::max(kVerticalMargin, (min_height - GetTextHeight()) / 2);
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

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

void OmniboxResultView::Layout() {
  views::View::Layout();

  const int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
      GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
  // NOTE: While animating the keyword match, both matches may be visible.
  int suggestion_width = width();
  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  if (keyword_match) {
    const int icon_width = keyword_view_->icon()->width() +
                           GetIconAlignmentOffset() + (horizontal_padding * 2);
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
  keyword_view_->SetBounds(suggestion_width, 0, width() - suggestion_width,
                           height());
  suggestion_view_->SetBounds(0, 0, suggestion_width, height());

  const int start_x = GetIconAlignmentOffset() + horizontal_padding;
  int end_x = suggestion_view_->width();

  int text_height = GetTextHeight();
  int row_height = text_height;
  if (IsTwoLineLayout())
    row_height += match_.answer ? GetAnswerHeight() : GetTextHeight();
  suggestion_view_->separator()->SetVisible(false);
  keyword_view_->separator()->SetVisible(false);

  // TODO(dschuyler): Refactor these if/else's into separate pieces of code to
  // improve readability/maintainability.

  if (keyword_match) {
    int kw_x =
        start_x + BackgroundWith1PxBorder::kLocationBarBorderThicknessDip;
    keyword_view_->icon()->SetPosition(gfx::Point(
        kw_x, (keyword_view_->height() - keyword_view_->icon()->height()) / 2));
    kw_x += keyword_view_->icon()->width() + horizontal_padding;

    int content_width =
        keyword_view_->content()->CalculatePreferredSize().width();
    int description_width =
        keyword_view_->description()->CalculatePreferredSize().width();
    OmniboxPopupModel::ComputeMatchMaxWidths(
        content_width, keyword_view_->separator()->width(), description_width,
        width(),
        /*description_on_separate_line=*/false,
        !AutocompleteMatch::IsSearchType(match_.type), &content_width,
        &description_width);
    int y = GetVerticalMargin();
    keyword_view_->content()->SetBounds(kw_x, y, content_width, text_height);
    if (description_width != 0) {
      kw_x += keyword_view_->content()->width();
      keyword_view_->separator()->SetVisible(true);
      keyword_view_->separator()->SetBounds(
          kw_x, y, keyword_view_->separator()->width(), text_height);
      kw_x += keyword_view_->separator()->width();
      keyword_view_->description()->SetBounds(kw_x, y, description_width,
                                              text_height);
    } else if (IsTwoLineLayout()) {
      keyword_view_->content()->SetSize(
          gfx::Size(content_width, text_height * 2));
    }
  }

  const gfx::Image icon = GetIcon();
  const int icon_y = GetVerticalMargin() + (row_height - icon.Height()) / 2;
  suggestion_view_->icon()->SetBounds(
      start_x, icon_y, std::min(end_x, icon.Width()), icon.Height());

  // NOTE: While animating the keyword match, both matches may be visible.
  int x = start_x;
  int y = GetVerticalMargin();

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxRichEntitySuggestions) &&
      match_.answer) {
    suggestion_view_->icon()->SetVisible(false);
    int image_edge_length =
        text_height + suggestion_view_->description()->GetLineHeight();
    suggestion_view_->image()->SetImageSize(
        gfx::Size(image_edge_length, image_edge_length));
    suggestion_view_->image()->SetBounds(x, y, image_edge_length,
                                         image_edge_length);
    x += image_edge_length + horizontal_padding;
    suggestion_view_->content()->SetBounds(x, y, end_x - x, text_height);
    y += text_height;
    suggestion_view_->description()->SetBounds(x, y, end_x - x, text_height);
    return;
  }

  suggestion_view_->icon()->SetVisible(true);
  x += icon.Width() + horizontal_padding;
  if (match_.answer) {
    suggestion_view_->content()->SetBounds(x, y, end_x - x, text_height);
    y += text_height;
    if (suggestion_view_->image()->visible()) {
      // The description may be multi-line. Using the view height results in
      // an image that's too large, so we use the line height here instead.
      int image_edge_length = suggestion_view_->description()->GetLineHeight();
      suggestion_view_->image()->SetBounds(
          start_x + suggestion_view_->icon()->width() + horizontal_padding,
          y + (kVerticalPadding / 2), image_edge_length, image_edge_length);
      suggestion_view_->image()->SetImageSize(
          gfx::Size(image_edge_length, image_edge_length));
      x += suggestion_view_->image()->width() + kAnswerIconToTextPadding;
    }
    int description_width = end_x - x;
    suggestion_view_->description()->SetBounds(
        x, y, description_width,
        suggestion_view_->description()->GetHeightForWidth(description_width) +
            kVerticalPadding);
  } else if (IsTwoLineLayout()) {
    if (!!suggestion_view_->description()->GetContentsBounds().width()) {
      // A description is present.
      suggestion_view_->content()->SetBounds(x, y, end_x - x, GetTextHeight());
      y += GetTextHeight();
      int description_width = end_x - x;
      suggestion_view_->description()->SetBounds(
          x, y, description_width,
          suggestion_view_->description()->GetHeightForWidth(
              description_width) +
              kVerticalPadding);
    } else {
      // For no description, shift down halfway to draw contents in middle.
      y += GetTextHeight() / 2;
      suggestion_view_->content()->SetBounds(x, y, end_x - x, GetTextHeight());
    }
  } else {
    int content_width =
        suggestion_view_->content()->CalculatePreferredSize().width();
    int description_width =
        suggestion_view_->description()->CalculatePreferredSize().width();
    OmniboxPopupModel::ComputeMatchMaxWidths(
        content_width, suggestion_view_->separator()->width(),
        description_width, end_x - x,
        /*description_on_separate_line=*/false,
        !AutocompleteMatch::IsSearchType(match_.type), &content_width,
        &description_width);
    suggestion_view_->content()->SetBounds(x, y, content_width, text_height);
    x += content_width;
    if (description_width) {
      suggestion_view_->separator()->SetVisible(true);
      suggestion_view_->separator()->SetBounds(
          x, y, suggestion_view_->separator()->width(), text_height);
      x += suggestion_view_->separator()->width();
    }
    suggestion_view_->description()->SetBounds(x, y, description_width,
                                               text_height);
  }
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
