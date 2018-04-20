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

// The minimum vertical margin that should be used above and below each
// suggestion.
static const int kMinVerticalMargin = 1;

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
  int offset = LocationBarView::GetBorderThicknessDip();

  // The touch-optimized popup selection always fills the results frame. So to
  // align icons, inset additionally by the frame alignment inset on the left.
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    offset += RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets.left();
  return offset;
}

// Returns the margins that should appear at the top and bottom of the result.
// |match_is_answer| indicates whether the vertical margin is for a omnibox
// result displaying an answer to the query.
gfx::Insets GetVerticalInsets(int text_height, bool match_is_answer) {
  // Regardless of the text size, we ensure a minimum size for the content line
  // here. This minimum is larger for hybrid mouse/touch devices to ensure an
  // adequately sized touch target.
  using Md = ui::MaterialDesignController;
  const int kIconVerticalPad = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentVerticalMargin,
      OmniboxFieldTrial::kUIVerticalMarginParam,
      Md::GetMode() == Md::MATERIAL_HYBRID ? 8 : 4);
  const int min_height_for_icon =
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE) + (kIconVerticalPad * 2);
  const int min_height_for_text = text_height + 2 * kMinVerticalMargin;
  int min_height = std::max(min_height_for_icon, min_height_for_text);

  // Make sure the minimum height of an omnibox result matches the height of the
  // location bar view / non-results section of the omnibox popup in touch.
  if (Md::IsTouchOptimizedUiEnabled()) {
    min_height = std::max(
        min_height, RoundedOmniboxResultsFrame::GetNonResultSectionHeight());
    if (match_is_answer) {
      // Answer matches apply the normal margin at the top and the minimum
      // allowable margin at the bottom.
      const int top_margin = gfx::ToCeiledInt((min_height - text_height) / 2.f);
      return gfx::Insets(top_margin, 0, kMinVerticalMargin, 0);
    }
  }

  const int total_margin = min_height - text_height;
  // Ceiling the top margin to account for |total_margin| being an odd number.
  const int top_margin = gfx::ToCeiledInt(total_margin / 2.f);
  const int bottom_margin = total_margin - top_margin;
  return gfx::Insets(top_margin, 0, bottom_margin, 0);
}

// Returns the padding width between elements.
int HorizontalPadding() {
  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxImageView:

class OmniboxImageView : public views::ImageView {
 public:
  bool CanProcessEventsWithinSubtree() const override { return false; }
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxSuggestionView:

// TODO(dschuyler): Move this class to its own file. The reason it started in
// the omnibox_result_view file was to ease the refactor of this code. I intend
// to resolve this in mid-2018.
class OmniboxSuggestionView : public views::View {
 public:
  explicit OmniboxSuggestionView(OmniboxResultView* result_view,
                                 const gfx::FontList& font_list,
                                 int text_height);
  ~OmniboxSuggestionView() override;

  views::ImageView* icon() { return icon_view_; }
  views::ImageView* image() { return image_view_; }
  OmniboxTextView* content() { return content_view_; }
  OmniboxTextView* description() { return description_view_; }
  OmniboxTextView* separator() { return separator_view_; }

  void OnMatchUpdate(const AutocompleteMatch& match);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool CanProcessEventsWithinSubtree() const override { return false; }

 protected:
  // views::View:
  void Layout() override;
  const char* GetClassName() const override;

  // Returns the height of the the description section of answer suggestions.
  int GetAnswerHeight() const;

  void LayoutAnswer();
  void LayoutEntity();
  void LayoutSplit();
  void LayoutTwoLine();

  bool is_answer_;
  bool is_search_type_;
  int text_height_;

  // Weak pointers for easy reference.
  views::ImageView* icon_view_;   // An icon resembling a '>'.
  views::ImageView* image_view_;  // For rich suggestions.
  OmniboxTextView* content_view_;
  OmniboxTextView* description_view_;
  OmniboxTextView* separator_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxSuggestionView);
};

OmniboxSuggestionView::OmniboxSuggestionView(OmniboxResultView* result_view,
                                             const gfx::FontList& font_list,
                                             int text_height)
    : is_answer_(false), is_search_type_(false), text_height_(text_height) {
  AddChildView(icon_view_ = new OmniboxImageView());
  AddChildView(image_view_ = new OmniboxImageView());
  AddChildView(content_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(description_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(separator_view_ = new OmniboxTextView(result_view, font_list));

  const base::string16& separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  separator_view_->SetText(separator);
}

OmniboxSuggestionView::~OmniboxSuggestionView() = default;

gfx::Size OmniboxSuggestionView::CalculatePreferredSize() const {
  int height =
      text_height_ + GetVerticalInsets(text_height_, is_answer_).height();
  if (is_answer_)
    height += GetAnswerHeight();
  else if (IsTwoLineLayout())
    height += text_height_;
  return gfx::Size(0, height);
}

int OmniboxSuggestionView::GetAnswerHeight() const {
  int icon_width = icon_view_->width();
  int answer_icon_size = image_view_->visible()
                             ? image_view_->height() + kAnswerIconToTextPadding
                             : 0;
  // TODO(dschuyler): The GetIconAlignmentOffset() is applied an extra time to
  // match the math in Layout(). This seems like a (minor) mistake.
  int deduction = (GetIconAlignmentOffset() * 2) + icon_width +
                  (HorizontalPadding() * 3) + answer_icon_size;
  int description_width = std::max(width() - deduction, 0);
  return description_view_->GetHeightForWidth(description_width) +
         kVerticalPadding;
}

void OmniboxSuggestionView::OnMatchUpdate(const AutocompleteMatch& match) {
  is_answer_ = !!match.answer;
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);
  if (is_answer_ || IsTwoLineLayout()) {
    separator_view_->SetSize(gfx::Size());
  }
  if (is_answer_ &&
      base::FeatureList::IsEnabled(omnibox::kOmniboxRichEntitySuggestions)) {
    icon_view_->SetSize(gfx::Size());
  }
}

const char* OmniboxSuggestionView::GetClassName() const {
  return "OmniboxSuggestionView";
}

void OmniboxSuggestionView::Layout() {
  views::View::Layout();
  if (is_answer_) {
    if (base::FeatureList::IsEnabled(omnibox::kOmniboxRichEntitySuggestions)) {
      LayoutEntity();
    } else {
      LayoutAnswer();
    }
  } else if (IsTwoLineLayout()) {
    LayoutTwoLine();
  } else {
    LayoutSplit();
  }
}

void OmniboxSuggestionView::LayoutAnswer() {
  const int start_x = GetIconAlignmentOffset() + HorizontalPadding();
  int x = start_x + LocationBarView::GetBorderThicknessDip();
  int y = GetVerticalInsets(text_height_, is_answer_).top();
  icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  int center_icon_within = IsTwoLineLayout() ? height() : text_height_;
  icon_view_->SetPosition(
      gfx::Point(x, y + (center_icon_within - icon_view_->height()) / 2));
  x += icon_view_->width() + HorizontalPadding();
  content_view_->SetBounds(x, y, width() - x, text_height_);
  y += text_height_;
  if (image_view_->visible()) {
    // The description may be multi-line. Using the view height results in
    // an image that's too large, so we use the line height here instead.
    int image_edge_length = description_view_->GetLineHeight();
    image_view_->SetBounds(start_x + icon_view_->width() + HorizontalPadding(),
                           y + (kVerticalPadding / 2), image_edge_length,
                           image_edge_length);
    image_view_->SetImageSize(gfx::Size(image_edge_length, image_edge_length));
    x += image_view_->width() + kAnswerIconToTextPadding;
  }
  int description_width = width() - x;
  description_view_->SetBounds(
      x, y, description_width,
      description_view_->GetHeightForWidth(description_width) +
          kVerticalPadding);
}

void OmniboxSuggestionView::LayoutEntity() {
  int x = GetIconAlignmentOffset() + HorizontalPadding() +
          LocationBarView::GetBorderThicknessDip();
  int y = GetVerticalInsets(text_height_, is_answer_).top();
  int image_edge_length = text_height_ + description_view_->GetLineHeight();
  image_view_->SetImageSize(gfx::Size(image_edge_length, image_edge_length));
  image_view_->SetBounds(x, y, image_edge_length, image_edge_length);
  x += image_edge_length + HorizontalPadding();
  content_view_->SetBounds(x, y, width() - x, text_height_);
  y += text_height_;
  description_view_->SetBounds(x, y, width() - x, text_height_);
}

void OmniboxSuggestionView::LayoutSplit() {
  int x = GetIconAlignmentOffset() + HorizontalPadding() +
          LocationBarView::GetBorderThicknessDip();
  icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  int y = GetVerticalInsets(text_height_, is_answer_).top();
  icon_view_->SetPosition(
      gfx::Point(x, y + (text_height_ - icon_view_->height()) / 2));
  x += icon_view_->width() + HorizontalPadding();
  int content_width = content_view_->CalculatePreferredSize().width();
  int description_width = description_view_->CalculatePreferredSize().width();
  OmniboxPopupModel::ComputeMatchMaxWidths(
      content_width, separator_view_->width(), description_width, width(),
      /*description_on_separate_line=*/false, !is_search_type_, &content_width,
      &description_width);
  content_view_->SetBounds(x, y, content_width, text_height_);
  if (description_width != 0) {
    x += content_view_->width();
    separator_view_->SetSize(separator_view_->CalculatePreferredSize());
    separator_view_->SetBounds(x, y, separator_view_->width(), text_height_);
    x += separator_view_->width();
    description_view_->SetBounds(x, y, description_width, text_height_);
  } else {
    separator_view_->SetSize(gfx::Size());
  }
}

void OmniboxSuggestionView::LayoutTwoLine() {
  int x = GetIconAlignmentOffset() + HorizontalPadding() +
          LocationBarView::GetBorderThicknessDip();
  int y = GetVerticalInsets(text_height_, is_answer_).top();
  icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  icon_view_->SetPosition(gfx::Point(x, (height() - icon_view_->height()) / 2));
  x += icon_view_->width() + HorizontalPadding();
  if (!!description_view_->GetContentsBounds().width()) {
    // A description is present.
    content_view_->SetBounds(x, y, width() - x, text_height_);
    y += text_height_;
    int description_width = width() - x;
    description_view_->SetBounds(
        x, y, description_width,
        description_view_->GetHeightForWidth(description_width) +
            kVerticalPadding);
  } else {
    // For no description, shift down halfway to draw contents in middle.
    y += text_height_ / 2;
    content_view_->SetBounds(x, y, width() - x, text_height_);
  }
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

  AddChildView(suggestion_view_ =
                   new OmniboxSuggestionView(this, font_list, GetTextHeight()));
  AddChildView(keyword_view_ =
                   new OmniboxSuggestionView(this, font_list, GetTextHeight()));

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
      false);  // Until SetRichSuggestionImage is called.
  keyword_view_->icon()->SetVisible(match_.associated_keyword.get());

  if (match.has_tab_match && !keyword_view_->icon()->visible()) {
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

  // Reapply the dim color to account for the highlight state.
  suggestion_view_->separator()->Dim();
  keyword_view_->separator()->Dim();

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
  model_->OpenMatch(model_index_, disposition);
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
