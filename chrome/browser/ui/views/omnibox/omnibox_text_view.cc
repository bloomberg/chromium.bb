// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <algorithm>
#include <memory>

#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"

namespace {

// Use the primary style for everything. TextStyle sometimes controls color, but
// we use OmniboxTheme for that.
constexpr int kTextStyle = views::style::STYLE_PRIMARY;

// Indicates to use CONTEXT_OMNIBOX_PRIMARY when picking a font size in legacy
// code paths.
constexpr int kInherit = INT_MIN;

// The vertical padding to provide each RenderText in addition to the height
// of the font. Where possible, RenderText uses this additional space to
// vertically center the cap height of the font instead of centering the
// entire font.
static constexpr int kVerticalPadding = 3;

struct TextStyle {
  OmniboxPart part;

  // The legacy size delta, relative to the ui::ResourceBundle BaseFont, or
  // kInherit to use CONTEXT_OMNIBOX_PRIMARY, to match the omnibox font.
  // Note: the actual font size may differ due to |baseline| altering the size.
  int legacy_size_delta = kInherit;

  // The size delta from the Touchable chrome spec. This is always relative to
  // CONTEXT_OMNIBOX_PRIMARY, which defaults to 15pt under touch. Only negative
  // deltas are supported correctly (the line height will not increase to fit).
  int touchable_size_delta = 0;

  // The baseline shift. Ignored under touch (text is always baseline-aligned).
  gfx::BaselineStyle baseline = gfx::NORMAL_BASELINE;
};

// The new answer layout has separate and different treatment of text styles,
// and as of writing both styling approaches need to be supported.  When old
// answer styles are deprecated, the above TextStyle structure and related
// logic can be removed, and this used exclusively.  This utility function
// applies new answer text styling for given text_type over range on render_text
// using result_view as a source for omnibox part colors.
void ApplyTextStyleForType(SuggestionAnswer::TextStyle text_style,
                           OmniboxResultView* result_view,
                           gfx::RenderText* render_text,
                           const gfx::Range& range) {
  struct TextStyleNewAnswers {
    void Apply(gfx::RenderText* render_text, const gfx::Range& range) {
      render_text->ApplyWeight(weight, range);
      render_text->ApplyBaselineStyle(baseline, range);
      render_text->ApplyColor(color, range);
    }
    SkColor color;
    gfx::BaselineStyle baseline = gfx::NORMAL_BASELINE;
    gfx::Font::Weight weight = gfx::Font::Weight::NORMAL;
  };

  TextStyleNewAnswers style;
  const SkColor part_color = result_view->GetColor(
      (text_style == SuggestionAnswer::TextStyle::NORMAL_DIM)
          ? OmniboxPart::RESULTS_TEXT_DIMMED
          : OmniboxPart::RESULTS_TEXT_DEFAULT);
  switch (text_style) {
    case SuggestionAnswer::TextStyle::SECONDARY:
      style = {gfx::kGoogleGrey600};
      break;
    case SuggestionAnswer::TextStyle::POSITIVE:
      style = {gfx::kGoogleGreen600};
      break;
    case SuggestionAnswer::TextStyle::NEGATIVE:
      style = {gfx::kGoogleRed600};
      break;
    case SuggestionAnswer::TextStyle::SUPERIOR:
      style = {.color = part_color, .baseline = gfx::SUPERIOR};
      break;
    case SuggestionAnswer::TextStyle::BOLD:
      style = {.color = part_color,
               .baseline = gfx::NORMAL_BASELINE,
               .weight = gfx::Font::Weight::BOLD};
      break;
    case SuggestionAnswer::TextStyle::NORMAL:
    case SuggestionAnswer::TextStyle::NORMAL_DIM:
    default:
      style = {part_color};
      break;
  }
  style.Apply(render_text, range);
}

}  // namespace

OmniboxTextView::OmniboxTextView(OmniboxResultView* result_view)
    : result_view_(result_view) {}

OmniboxTextView::~OmniboxTextView() = default;

gfx::Size OmniboxTextView::CalculatePreferredSize() const {
  return render_text_ ? render_text_->GetStringSize() : gfx::Size();
}

bool OmniboxTextView::CanProcessEventsWithinSubtree() const {
  return false;
}

const char* OmniboxTextView::GetClassName() const {
  return "OmniboxTextView";
}

int OmniboxTextView::GetHeightForWidth(int width) const {
  if (!render_text_)
    return 0;
  // If text wrapping is not called for we can simply return the font height.
  if (!wrap_text_lines_)
    return GetLineHeight();
  render_text_->SetDisplayRect(gfx::Rect(width, 0));
  gfx::Size string_size = render_text_->GetStringSize();
  return string_size.height() + kVerticalPadding;
}

void OmniboxTextView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (!render_text_)
    return;
  render_text_->SetDisplayRect(GetContentsBounds());
  render_text_->Draw(canvas);
}

void OmniboxTextView::ApplyTextColor(OmniboxPart part) {
  if (text().empty())
    return;
  render_text_->SetColor(result_view_->GetColor(part));
  SchedulePaint();
}

const base::string16& OmniboxTextView::text() const {
  return render_text_ ? render_text_->text() : base::EmptyString16();
}

void OmniboxTextView::SetText(const base::string16& new_text,
                              bool deemphasize) {
  if (cached_classifications_) {
    cached_classifications_.reset();
  } else if (text() == new_text && deemphasize == use_deemphasized_font_) {
    // Only exit early if |cached_classifications_| was empty,
    // i.e. the last time text was set was through this method.
    return;
  }

  use_deemphasized_font_ = deemphasize;
  render_text_ = CreateRenderText(new_text);

  OnStyleChanged();
}

void OmniboxTextView::SetText(const base::string16& new_text,
                              const ACMatchClassifications& classifications,
                              bool deemphasize) {
  if (text() == new_text && cached_classifications_ &&
      classifications == *cached_classifications_ &&
      deemphasize == use_deemphasized_font_)
    return;

  use_deemphasized_font_ = deemphasize;

  cached_classifications_ =
      std::make_unique<ACMatchClassifications>(classifications);
  render_text_ = CreateRenderText(new_text);

  // ReapplyStyling will update the preferred size and request a repaint.
  ReapplyStyling();
}

void OmniboxTextView::SetText(const SuggestionAnswer::ImageLine& line,
                              bool deemphasize) {
  use_deemphasized_font_ = deemphasize;
  cached_classifications_.reset();
  wrap_text_lines_ = line.num_text_lines() > 1;
  render_text_ = CreateRenderText(base::string16());

  for (const SuggestionAnswer::TextField& text_field : line.text_fields())
    AppendText(text_field, base::string16());
  if (!line.text_fields().empty()) {
    constexpr int kMaxDisplayLines = 3;
    const SuggestionAnswer::TextField& first_field = line.text_fields().front();
    if (first_field.has_num_lines() && first_field.num_lines() > 1) {
      render_text_->SetMultiline(true);
      render_text_->SetMaxLines(
          std::min(kMaxDisplayLines, first_field.num_lines()));
    }
  }

  // Add the "additional" and "status" text from |line|, if any.
  AppendExtraText(line);

  OnStyleChanged();
}

void OmniboxTextView::AppendExtraText(const SuggestionAnswer::ImageLine& line) {
  const base::string16 space(1, base::char16(' '));
  const auto* text_field = line.additional_text();
  if (text_field) {
    AppendText(*text_field, space);
  }
  text_field = line.status_text();
  if (text_field) {
    AppendText(*text_field, space);
  }
  SetPreferredSize(CalculatePreferredSize());
}

int OmniboxTextView::GetLineHeight() const {
  return font_height_;
}

void OmniboxTextView::ReapplyStyling() {
  // No work required if there are no preexisting styles.
  if (!cached_classifications_)
    return;

  const size_t text_length = text().length();
  for (size_t i = 0; i < cached_classifications_->size(); ++i) {
    const size_t text_start = (*cached_classifications_)[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end =
        (i < (cached_classifications_->size() - 1))
            ? std::min((*cached_classifications_)[i + 1].offset, text_length)
            : text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if ((*cached_classifications_)[i].style & ACMatchClassification::MATCH)
      render_text_->ApplyWeight(gfx::Font::Weight::BOLD, current_range);

    OmniboxPart part = OmniboxPart::RESULTS_TEXT_DEFAULT;
    if ((*cached_classifications_)[i].style & ACMatchClassification::URL) {
      part = OmniboxPart::RESULTS_TEXT_URL;
      render_text_->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
    } else if ((*cached_classifications_)[i].style &
               ACMatchClassification::DIM) {
      part = OmniboxPart::RESULTS_TEXT_DIMMED;
    }
    render_text_->ApplyColor(result_view_->GetColor(part), current_range);
  }

  OnStyleChanged();
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateRenderText(
    const base::string16& text) const {
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  const gfx::FontList& font = views::style::GetFont(
      (use_deemphasized_font_ ? CONTEXT_OMNIBOX_DEEMPHASIZED
                              : CONTEXT_OMNIBOX_PRIMARY),
      kTextStyle);
  render_text->SetFontList(font);
  render_text->SetText(text);
  return render_text;
}

void OmniboxTextView::AppendText(const SuggestionAnswer::TextField& field,
                                 const base::string16& prefix) {
  const base::string16& append_text =
      prefix.empty() ? field.text() : (prefix + field.text());
  if (append_text.empty())
    return;
  int offset = text().length();
  gfx::Range range(offset, offset + append_text.length());
  render_text_->AppendText(append_text);
  ApplyTextStyleForType(field.style(), result_view_, render_text_.get(), range);
}

void OmniboxTextView::OnStyleChanged() {
  const int height_normal = render_text_->font_list().GetHeight();
  const int height_bold =
      ui::ResourceBundle::GetSharedInstance()
          .GetFontListWithDelta(render_text_->font_list().GetFontSize() -
                                    gfx::FontList().GetFontSize(),
                                gfx::Font::NORMAL, gfx::Font::Weight::BOLD)
          .GetHeight();
  font_height_ = std::max(height_normal, height_bold);
  font_height_ += kVerticalPadding;

  SetPreferredSize(CalculatePreferredSize());
  SchedulePaint();
}
