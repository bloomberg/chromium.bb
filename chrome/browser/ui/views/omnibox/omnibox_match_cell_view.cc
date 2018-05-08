// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "extensions/common/image_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/controls/image_view.h"

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
// |is_old_style_answer| indicates whether the vertical margin is for a omnibox
// result displaying an answer to the query.
gfx::Insets GetVerticalInsets(int text_height, bool is_old_style_answer) {
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
    if (is_old_style_answer) {
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

////////////////////////////////////////////////////////////////////////////////
// PlaceholderImageSource:

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
// OmniboxImageView:

class OmniboxImageView : public views::ImageView {
 public:
  bool CanProcessEventsWithinSubtree() const override { return false; }
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

OmniboxMatchCellView::OmniboxMatchCellView(OmniboxResultView* result_view,
                                           const gfx::FontList& font_list,
                                           int text_height)
    : is_old_style_answer_(false),
      is_rich_suggestion_(false),
      is_search_type_(false),
      text_height_(text_height) {
  AddChildView(icon_view_ = new OmniboxImageView());
  AddChildView(image_view_ = new OmniboxImageView());
  AddChildView(content_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(description_view_ = new OmniboxTextView(result_view, font_list));
  AddChildView(separator_view_ = new OmniboxTextView(result_view, font_list));

  const base::string16& separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  separator_view_->SetText(separator);
}

OmniboxMatchCellView::~OmniboxMatchCellView() = default;

gfx::Size OmniboxMatchCellView::CalculatePreferredSize() const {
  int height = text_height_ +
               GetVerticalInsets(text_height_, is_old_style_answer_).height();
  if (is_rich_suggestion_) {
    height += text_height_;
  } else if (is_old_style_answer_) {
    height += GetOldStyleAnswerHeight();
  }
  return gfx::Size(0, height);
}

bool OmniboxMatchCellView::CanProcessEventsWithinSubtree() const {
  return false;
}

int OmniboxMatchCellView::GetOldStyleAnswerHeight() const {
  int icon_width = icon_view_->width();
  int answer_icon_size = image_view_->visible()
                             ? image_view_->height() + kAnswerIconToTextPadding
                             : 0;
  int deduction = GetIconAlignmentOffset() + icon_width +
                  (HorizontalPadding() * 3) + answer_icon_size;
  int description_width = std::max(width() - deduction, 0);
  return description_view_->GetHeightForWidth(description_width) +
         kVerticalPadding;
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  is_old_style_answer_ = !!match.answer;
  is_rich_suggestion_ =
      (base::FeatureList::IsEnabled(omnibox::kOmniboxNewAnswerLayout) &&
       !!match.answer) ||
      (base::FeatureList::IsEnabled(omnibox::kOmniboxRichEntitySuggestions) &&
       !match.image_url.empty());
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);
  if (is_old_style_answer_ || is_rich_suggestion_) {
    // Multi-line layout doesn't use the separator.
    separator_view_->SetSize(gfx::Size());

    // Set up default (placeholder) image.
    SkColor color = result_view->GetColor(OmniboxPart::RESULTS_BACKGROUND);
    extensions::image_util::ParseHexColorString(match.image_dominant_color,
                                                &color);
    color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).
    int image_edge_length = description_view_->GetLineHeight();
    image_view_->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(
            gfx::Size(image_edge_length, image_edge_length), color));
  } else {
    // Single-line layout doesn't use the image.
    image_view_->SetSize(gfx::Size());
  }
  if (is_rich_suggestion_) {
    // All rich suggestions don't use the old (small) icon.
    icon_view_->SetSize(gfx::Size());
  }
}

const char* OmniboxMatchCellView::GetClassName() const {
  return "OmniboxMatchCellView";
}

void OmniboxMatchCellView::Layout() {
  views::View::Layout();
  if (is_rich_suggestion_) {
    LayoutRichSuggestion();
  } else if (is_old_style_answer_) {
    LayoutOldStyleAnswer();
  } else {
    LayoutSplit();
  }
}

void OmniboxMatchCellView::LayoutOldStyleAnswer() {
  const int start_x = GetIconAlignmentOffset() + HorizontalPadding();
  int x = start_x;
  int y = GetVerticalInsets(text_height_, /*is_old_style_answer=*/true).top();
  icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  icon_view_->SetPosition(
      gfx::Point(x, y + (text_height_ - icon_view_->height()) / 2));
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

void OmniboxMatchCellView::LayoutRichSuggestion() {
  int x = GetIconAlignmentOffset() + HorizontalPadding();
  int y = GetVerticalInsets(text_height_, /*is_old_style_answer=*/false).top();
  int image_edge_length = text_height_ * 2;
  image_view_->SetImageSize(gfx::Size(image_edge_length, image_edge_length));
  image_view_->SetBounds(x, y, image_edge_length, image_edge_length);
  x += image_edge_length + HorizontalPadding();
  content_view_->SetBounds(x, y, width() - x, text_height_);
  y += text_height_;
  description_view_->SetBounds(x, y, width() - x, text_height_);
}

void OmniboxMatchCellView::LayoutSplit() {
  int x = GetIconAlignmentOffset() + HorizontalPadding();
  icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  int y = GetVerticalInsets(text_height_, /*is_old_style_answer=*/false).top();
  icon_view_->SetPosition(
      gfx::Point(x, y + (text_height_ - icon_view_->height()) / 2));
  x += icon_view_->width() + HorizontalPadding();
  int content_width = content_view_->CalculatePreferredSize().width();
  int description_width = description_view_->CalculatePreferredSize().width();
  gfx::Size separator_size = separator_view_->CalculatePreferredSize();
  OmniboxPopupModel::ComputeMatchMaxWidths(
      content_width, separator_size.width(), description_width, width() - x,
      /*description_on_separate_line=*/false, !is_search_type_, &content_width,
      &description_width);
  content_view_->SetBounds(x, y, content_width, text_height_);
  if (description_width != 0) {
    x += content_view_->width();
    separator_view_->SetSize(separator_size);
    separator_view_->SetBounds(x, y, separator_view_->width(), text_height_);
    x += separator_view_->width();
    description_view_->SetBounds(x, y, description_width, text_height_);
  } else {
    description_view_->SetSize(gfx::Size());
    separator_view_->SetSize(gfx::Size());
  }
}
