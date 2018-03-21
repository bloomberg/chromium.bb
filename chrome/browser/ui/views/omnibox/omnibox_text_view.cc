// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <algorithm>
#include <memory>

#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"

namespace {

struct TextStyle {
  ui::ResourceBundle::FontStyle font;
  OmniboxPart part;
  gfx::BaselineStyle baseline;
};

// Returns the styles that should be applied to the specified answer text type.
//
// Note that the font value is only consulted for the first text type that
// appears on an answer line, because RenderText does not yet support multiple
// font sizes. Subsequent text types on the same line will share the text size
// of the first type, while the color and baseline styles specified here will
// always apply. The gfx::INFERIOR baseline style is used as a workaround to
// produce smaller text on the same line. The way this is used in the current
// set of answers is that the small types (TOP_ALIGNED, DESCRIPTION_NEGATIVE,
// DESCRIPTION_POSITIVE and SUGGESTION_SECONDARY_TEXT_SMALL) only ever appear
// following LargeFont text, so for consistency they specify LargeFont for the
// first value even though this is not actually used (since they're not the
// first value).
TextStyle GetTextStyle(int type) {
  switch (type) {
    case SuggestionAnswer::TOP_ALIGNED:
      return {ui::ResourceBundle::LargeFont, OmniboxPart::RESULTS_TEXT_DIMMED,
              gfx::SUPERIOR};
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      return {ui::ResourceBundle::LargeFont, OmniboxPart::RESULTS_TEXT_NEGATIVE,
              gfx::INFERIOR};
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      return {ui::ResourceBundle::LargeFont, OmniboxPart::RESULTS_TEXT_POSITIVE,
              gfx::INFERIOR};
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      return {ui::ResourceBundle::BaseFont, OmniboxPart::RESULTS_TEXT_DEFAULT,
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      return {ui::ResourceBundle::BaseFont, OmniboxPart::RESULTS_TEXT_DIMMED,
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      return {ui::ResourceBundle::LargeFont, OmniboxPart::RESULTS_TEXT_DIMMED,
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      return {ui::ResourceBundle::LargeFont, OmniboxPart::RESULTS_TEXT_DIMMED,
              gfx::INFERIOR};
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      return {ui::ResourceBundle::BaseFont, OmniboxPart::RESULTS_TEXT_DIMMED,
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::SUGGESTION:  // Fall through.
    default:
      return {ui::ResourceBundle::BaseFont, OmniboxPart::RESULTS_TEXT_DEFAULT,
              gfx::NORMAL_BASELINE};
  }
}

}  // namespace

OmniboxTextView::OmniboxTextView(OmniboxResultView* result_view,
                                 const gfx::FontList& font_list)
    : result_view_(result_view),
      font_list_(font_list),
      font_height_(std::max(
          font_list.GetHeight(),
          font_list.DeriveWithWeight(gfx::Font::Weight::BOLD).GetHeight())) {}

OmniboxTextView::~OmniboxTextView() {}

gfx::Size OmniboxTextView::CalculatePreferredSize() const {
  if (!render_text_)
    return gfx::Size();
  return render_text_->GetStringSize();
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
  render_text_->SetDisplayRect(gfx::Rect(width, 0));
  gfx::Size string_size = render_text_->GetStringSize();
  return string_size.height();
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateRenderText(
    const base::string16& text) const {
  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  render_text->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  render_text->SetFontList(font_list_);
  render_text->SetText(text);
  return render_text;
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateClassifiedRenderText(
    const base::string16& text,
    const ACMatchClassifications& classifications) const {
  std::unique_ptr<gfx::RenderText> render_text(CreateRenderText(text));
  const size_t text_length = render_text->text().length();
  for (size_t i = 0; i < classifications.size(); ++i) {
    const size_t text_start = classifications[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end =
        (i < (classifications.size() - 1))
            ? std::min(classifications[i + 1].offset, text_length)
            : text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if (classifications[i].style & ACMatchClassification::MATCH)
      render_text->ApplyWeight(gfx::Font::Weight::BOLD, current_range);

    OmniboxPart part = OmniboxPart::RESULTS_TEXT_DEFAULT;
    if (classifications[i].style & ACMatchClassification::URL) {
      part = OmniboxPart::RESULTS_TEXT_URL;
      render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
    } else if (classifications[i].style & ACMatchClassification::DIM) {
      part = OmniboxPart::RESULTS_TEXT_DIMMED;
    } else if (classifications[i].style & ACMatchClassification::INVISIBLE) {
      part = OmniboxPart::RESULTS_TEXT_INVISIBLE;
    }
    render_text->ApplyColor(result_view_->GetColor(part), current_range);
  }
  return render_text;
}

void OmniboxTextView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  render_text_->SetDisplayRect(GetContentsBounds());
  render_text_->Draw(canvas);
}

void OmniboxTextView::Dim() {
  render_text_->SetColor(
      result_view_->GetColor(OmniboxPart::RESULTS_TEXT_DIMMED));
}

void OmniboxTextView::SetText(const base::string16& text,
                              const ACMatchClassifications& classifications) {
  render_text_.reset();
  render_text_ = CreateClassifiedRenderText(text, classifications);
  SizeToPreferredSize();
}

void OmniboxTextView::SetText(const base::string16& text) {
  render_text_.reset();
  render_text_ = CreateRenderText(text);
  SizeToPreferredSize();
}

void OmniboxTextView::SetText(const SuggestionAnswer::ImageLine& line) {
  // This assumes that the first text type in the line can be used to specify
  // the font for all the text fields in the line.  For now this works but
  // eventually it may be necessary to get RenderText to support multiple font
  // sizes or use multiple RenderTexts.
  render_text_.reset();
  render_text_ = CreateText(line, GetFontForType(line.text_fields()[0].type()));
  SizeToPreferredSize();
}

std::unique_ptr<gfx::RenderText> OmniboxTextView::CreateText(
    const SuggestionAnswer::ImageLine& line,
    const gfx::FontList& font_list) const {
  std::unique_ptr<gfx::RenderText> destination =
      CreateRenderText(base::string16());
  destination->SetFontList(font_list);

  for (const SuggestionAnswer::TextField& text_field : line.text_fields())
    AppendText(destination.get(), text_field.text(), text_field.type());
  if (!line.text_fields().empty()) {
    constexpr int kMaxDisplayLines = 3;
    const SuggestionAnswer::TextField& first_field = line.text_fields().front();
    if (first_field.has_num_lines() && first_field.num_lines() > 1 &&
        destination->MultilineSupported()) {
      destination->SetMultiline(true);
      destination->SetMaxLines(
          std::min(kMaxDisplayLines, first_field.num_lines()));
    }
  }
  const base::char16 space(' ');
  const auto* text_field = line.additional_text();
  if (text_field) {
    AppendText(destination.get(), space + text_field->text(),
               text_field->type());
  }
  text_field = line.status_text();
  if (text_field) {
    AppendText(destination.get(), space + text_field->text(),
               text_field->type());
  }
  return destination;
}

void OmniboxTextView::AppendText(gfx::RenderText* destination,
                                 const base::string16& text,
                                 int text_type) const {
  // TODO(dschuyler): make this better.  Right now this only supports unnested
  // bold tags.  In the future we'll need to flag unexpected tags while adding
  // support for b, i, u, sub, and sup.  We'll also need to support HTML
  // entities (&lt; for '<', etc.).
  const base::string16 begin_tag = base::ASCIIToUTF16("<b>");
  const base::string16 end_tag = base::ASCIIToUTF16("</b>");
  size_t begin = 0;
  while (true) {
    size_t end = text.find(begin_tag, begin);
    if (end == base::string16::npos) {
      AppendTextHelper(destination, text.substr(begin), text_type, false);
      break;
    }
    AppendTextHelper(destination, text.substr(begin, end - begin), text_type,
                     false);
    begin = end + begin_tag.length();
    end = text.find(end_tag, begin);
    if (end == base::string16::npos)
      break;
    AppendTextHelper(destination, text.substr(begin, end - begin), text_type,
                     true);
    begin = end + end_tag.length();
  }
}

void OmniboxTextView::AppendTextHelper(gfx::RenderText* destination,
                                       const base::string16& text,
                                       int text_type,
                                       bool is_bold) const {
  if (text.empty())
    return;
  int offset = destination->text().length();
  gfx::Range range(offset, offset + text.length());
  destination->AppendText(text);
  const TextStyle& text_style = GetTextStyle(text_type);
  // TODO(dschuyler): follow up on the problem of different font sizes within
  // one RenderText.  Maybe with destination->SetFontList(...).
  destination->ApplyWeight(
      is_bold ? gfx::Font::Weight::BOLD : gfx::Font::Weight::NORMAL, range);
  destination->ApplyColor(result_view_->GetColor(text_style.part), range);
  destination->ApplyBaselineStyle(text_style.baseline, range);
}

int OmniboxTextView::GetLineHeight() const {
  return font_height_;
}

const gfx::FontList& OmniboxTextView::GetFontForType(int text_type) const {
  // When BaseFont is specified, reuse font_list_, which may have had size
  // adjustments from BaseFont before it was provided to this class.  Otherwise,
  // get the standard font list for the specified style.
  ui::ResourceBundle::FontStyle font_style = GetTextStyle(text_type).font;
  const gfx::FontList& font_list =
      (font_style == ui::ResourceBundle::BaseFont)
          ? font_list_
          : ui::ResourceBundle::GetSharedInstance().GetFontList(font_style);
  font_height_ = font_list.GetHeight();
  return font_list;
}
