// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For WinDDK ATL compatibility, these ATL headers must come first.
#include "build/build_config.h"

#if defined(OS_WIN)
#include <atlbase.h>  // NOLINT
#include <atlwin.h>  // NOLINT
#endif

#include "base/macros.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>  // NOLINT

#include "base/i18n/bidi_line_iterator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/native_theme.h"

using ui::NativeTheme;

namespace {

// A mapping from OmniboxResultView's ResultViewState/ColorKind types to
// NativeTheme colors.
struct TranslationTable {
  ui::NativeTheme::ColorId id;
  OmniboxResultView::ResultViewState state;
  OmniboxResultView::ColorKind kind;
} static const kTranslationTable[] = {
  { NativeTheme::kColorId_ResultsTableNormalBackground,
    OmniboxResultView::NORMAL, OmniboxResultView::BACKGROUND },
  { NativeTheme::kColorId_ResultsTableHoveredBackground,
    OmniboxResultView::HOVERED, OmniboxResultView::BACKGROUND },
  { NativeTheme::kColorId_ResultsTableSelectedBackground,
    OmniboxResultView::SELECTED, OmniboxResultView::BACKGROUND },
  { NativeTheme::kColorId_ResultsTableNormalText,
    OmniboxResultView::NORMAL, OmniboxResultView::TEXT },
  { NativeTheme::kColorId_ResultsTableHoveredText,
    OmniboxResultView::HOVERED, OmniboxResultView::TEXT },
  { NativeTheme::kColorId_ResultsTableSelectedText,
    OmniboxResultView::SELECTED, OmniboxResultView::TEXT },
  { NativeTheme::kColorId_ResultsTableNormalDimmedText,
    OmniboxResultView::NORMAL, OmniboxResultView::DIMMED_TEXT },
  { NativeTheme::kColorId_ResultsTableHoveredDimmedText,
    OmniboxResultView::HOVERED, OmniboxResultView::DIMMED_TEXT },
  { NativeTheme::kColorId_ResultsTableSelectedDimmedText,
    OmniboxResultView::SELECTED, OmniboxResultView::DIMMED_TEXT },
  { NativeTheme::kColorId_ResultsTableNormalUrl,
    OmniboxResultView::NORMAL, OmniboxResultView::URL },
  { NativeTheme::kColorId_ResultsTableHoveredUrl,
    OmniboxResultView::HOVERED, OmniboxResultView::URL },
  { NativeTheme::kColorId_ResultsTableSelectedUrl,
    OmniboxResultView::SELECTED, OmniboxResultView::URL },
};

struct TextStyle {
  ui::ResourceBundle::FontStyle font;
  ui::NativeTheme::ColorId colors[OmniboxResultView::NUM_STATES];
  gfx::BaselineStyle baseline;
};

TextStyle GetTextStyle(int type) {
  switch (type) {
    case SuggestionAnswer::TOP_ALIGNED:
      return {ui::ResourceBundle::LargeFont,
              {NativeTheme::kColorId_ResultsTableNormalDimmedText,
               NativeTheme::kColorId_ResultsTableHoveredDimmedText,
               NativeTheme::kColorId_ResultsTableSelectedDimmedText},
              gfx::SUPERIOR};
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      return {ui::ResourceBundle::LargeFont,
              {NativeTheme::kColorId_ResultsTableNegativeText,
               NativeTheme::kColorId_ResultsTableNegativeHoveredText,
               NativeTheme::kColorId_ResultsTableNegativeSelectedText},
              gfx::INFERIOR};
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      return {ui::ResourceBundle::LargeFont,
              {NativeTheme::kColorId_ResultsTablePositiveText,
               NativeTheme::kColorId_ResultsTablePositiveHoveredText,
               NativeTheme::kColorId_ResultsTablePositiveSelectedText},
              gfx::INFERIOR};
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      return {ui::ResourceBundle::BaseFont,
              {NativeTheme::kColorId_ResultsTableNormalText,
               NativeTheme::kColorId_ResultsTableHoveredText,
               NativeTheme::kColorId_ResultsTableSelectedText},
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      return {ui::ResourceBundle::BaseFont,
              {NativeTheme::kColorId_ResultsTableNormalText,
               NativeTheme::kColorId_ResultsTableHoveredText,
               NativeTheme::kColorId_ResultsTableSelectedText},
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      return {ui::ResourceBundle::LargeFont,
              {NativeTheme::kColorId_ResultsTableNormalText,
               NativeTheme::kColorId_ResultsTableHoveredText,
               NativeTheme::kColorId_ResultsTableSelectedText},
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:  // Fall through.
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      return {ui::ResourceBundle::BaseFont,
              {NativeTheme::kColorId_ResultsTableNormalDimmedText,
               NativeTheme::kColorId_ResultsTableHoveredDimmedText,
               NativeTheme::kColorId_ResultsTableSelectedDimmedText},
              gfx::NORMAL_BASELINE};
    case SuggestionAnswer::SUGGESTION:  // Fall through.
    default:
      return {ui::ResourceBundle::BaseFont,
              {NativeTheme::kColorId_ResultsTableNormalText,
               NativeTheme::kColorId_ResultsTableHoveredText,
               NativeTheme::kColorId_ResultsTableSelectedText},
              gfx::NORMAL_BASELINE};
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

// This class is a utility class for calculations affected by whether the result
// view is horizontally mirrored.  The drawing functions can be written as if
// all drawing occurs left-to-right, and then use this class to get the actual
// coordinates to begin drawing onscreen.
class OmniboxResultView::MirroringContext {
 public:
  MirroringContext() : center_(0), right_(0) {}

  // Tells the mirroring context to use the provided range as the physical
  // bounds of the drawing region.  When coordinate mirroring is needed, the
  // mirror point will be the center of this range.
  void Initialize(int x, int width) {
    center_ = x + width / 2;
    right_ = x + width;
  }

  // Given a logical range within the drawing region, returns the coordinate of
  // the possibly-mirrored "left" side.  (This functions exactly like
  // View::MirroredLeftPointForRect().)
  int mirrored_left_coord(int left, int right) const {
    return base::i18n::IsRTL() ? (center_ + (center_ - right)) : left;
  }

  // Given a logical coordinate within the drawing region, returns the remaining
  // width available.
  int remaining_width(int x) const {
    return right_ - x;
  }

 private:
  int center_;
  int right_;

  DISALLOW_COPY_AND_ASSIGN(MirroringContext);
};

OmniboxResultView::OmniboxResultView(OmniboxPopupContentsView* model,
                                     int model_index,
                                     const gfx::FontList& font_list)
    : model_(model),
      model_index_(model_index),
      font_list_(font_list),
      font_height_(std::max(
          font_list.GetHeight(),
          font_list.DeriveWithWeight(gfx::Font::Weight::BOLD).GetHeight())),
      mirroring_context_(new MirroringContext()),
      keyword_icon_(new views::ImageView()),
      animation_(new gfx::SlideAnimation(this)) {
  CHECK_GE(model_index, 0);
  keyword_icon_->set_owned_by_client();
  keyword_icon_->EnableCanvasFlippingForRTLUI(true);
  keyword_icon_->SetImage(
      GetVectorIcon(gfx::VectorIconId::OMNIBOX_KEYWORD_SEARCH));
  keyword_icon_->SizeToPreferredSize();
}

OmniboxResultView::~OmniboxResultView() {
}

SkColor OmniboxResultView::GetColor(
    ResultViewState state,
    ColorKind kind) const {
  for (size_t i = 0; i < arraysize(kTranslationTable); ++i) {
    if (kTranslationTable[i].state == state &&
        kTranslationTable[i].kind == kind) {
      return GetNativeTheme()->GetSystemColor(kTranslationTable[i].id);
    }
  }

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match;
  match_.PossiblySwapContentsAndDescriptionForDisplay();
  animation_->Reset();
  answer_image_ = gfx::ImageSkia();

  AutocompleteMatch* associated_keyword_match = match_.associated_keyword.get();
  if (associated_keyword_match) {
    if (!keyword_icon_->parent())
      AddChildView(keyword_icon_.get());
  } else if (keyword_icon_->parent()) {
    RemoveChildView(keyword_icon_.get());
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
  const ResultViewState state = GetState();
  if (state == NORMAL) {
    set_background(nullptr);
  } else {
    const SkColor bg_color = GetColor(state, BACKGROUND);
    set_background(new BackgroundWith1PxBorder(bg_color, bg_color));
  }

  // While the text in the RenderTexts may not have changed, the styling
  // (color/bold) may need to change. So we reset them to cause them to be
  // recomputed in OnPaint().
  contents_rendertext_.reset();
  description_rendertext_.reset();
  separator_rendertext_.reset();
  keyword_contents_rendertext_.reset();
  keyword_description_rendertext_.reset();
}

void OmniboxResultView::OnSelected() {
  DCHECK_EQ(SELECTED, GetState());

  // Notify assistive technology when results with answers attached are
  // selected. The non-answer text is already accessible as a consequence of
  // updating the text in the omnibox but this alert and GetAccessibleNodeData
  // below make the answer contents accessible.
  if (match_.answer)
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
}

gfx::Size OmniboxResultView::GetPreferredSize() const {
  if (!match_.answer)
    return gfx::Size(0, GetContentLineHeight());
  // An answer implies a match and a description in a large font.
  if (match_.answer->second_line().num_text_lines() == 1)
    return gfx::Size(0, GetContentLineHeight() + GetAnswerLineHeight());
  if (!description_rendertext_) {
    description_rendertext_ =
        CreateAnswerLine(match_.answer->second_line(), font_list_);
  }
  description_rendertext_->SetDisplayRect(
      gfx::Rect(text_bounds_.width(), 0));
  description_rendertext_->GetStringSize();
  return gfx::Size(
      0, GetContentLineHeight() +
             GetAnswerLineHeight() * description_rendertext_->GetNumLines());
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(match_.answer
                         ? l10n_util::GetStringFUTF16(
                               IDS_OMNIBOX_ACCESSIBLE_ANSWER, match_.contents,
                               match_.answer->second_line().AccessibleText())
                         : match_.contents);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, protected:

OmniboxResultView::ResultViewState OmniboxResultView::GetState() const {
  if (model_->IsSelectedIndex(model_index_))
    return SELECTED;
  return model_->IsHoveredIndex(model_index_) ? HOVERED : NORMAL;
}

int OmniboxResultView::GetTextHeight() const {
  return font_height_;
}

void OmniboxResultView::PaintMatch(const AutocompleteMatch& match,
                                   gfx::RenderText* contents,
                                   gfx::RenderText* description,
                                   gfx::Canvas* canvas,
                                   int x) const {
  int y = text_bounds_.y();

  if (!separator_rendertext_) {
    const base::string16& separator =
        l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
    separator_rendertext_ = CreateRenderText(separator);
    separator_rendertext_->SetColor(GetColor(GetState(), DIMMED_TEXT));
    separator_width_ = separator_rendertext_->GetContentWidth();
  }

  contents->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  if (description)
    description->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  int contents_max_width, description_max_width;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents->GetContentWidth(),
      separator_width_,
      description ? description->GetContentWidth() : 0,
      mirroring_context_->remaining_width(x),
      match.answer != nullptr,
      !AutocompleteMatch::IsSearchType(match.type),
      &contents_max_width,
      &description_max_width);

  int after_contents_x = DrawRenderText(match, contents, CONTENTS, canvas,
                                        x, y, contents_max_width);

  if (description_max_width != 0) {
    if (match.answer) {
      y += GetContentLineHeight();
      if (!answer_image_.isNull()) {
        int answer_icon_size = GetAnswerLineHeight();
        canvas->DrawImageInt(
            answer_image_,
            0, 0, answer_image_.width(), answer_image_.height(),
            GetMirroredXInView(x), y, answer_icon_size, answer_icon_size, true);
        // TODO(dschuyler): Perhaps this should be based on the font size
        // instead of hardcoded to 2 dp (e.g. by adding a space in an
        // appropriate font to the beginning of the description, then reducing
        // the additional padding here to zero).
        const int kAnswerIconToTextPadding = 2;
        x += answer_icon_size + kAnswerIconToTextPadding;
      }
    } else {
      x = DrawRenderText(match, separator_rendertext_.get(), SEPARATOR, canvas,
                         after_contents_x, y, separator_width_);
    }

    DrawRenderText(match, description, DESCRIPTION, canvas, x, y,
                   description_max_width);
  }
}

int OmniboxResultView::DrawRenderText(
    const AutocompleteMatch& match,
    gfx::RenderText* render_text,
    RenderTextType render_text_type,
    gfx::Canvas* canvas,
    int x,
    int y,
    int max_width) const {
  DCHECK(!render_text->text().empty());

  const int remaining_width = mirroring_context_->remaining_width(x);
  int right_x = x + max_width;

  // Infinite suggestions should appear with the leading ellipses vertically
  // stacked.
  if (render_text_type == CONTENTS &&
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    // When the directionality of suggestion doesn't match the UI, we try to
    // vertically stack the ellipsis by restricting the end edge (right_x).
    const bool is_ui_rtl = base::i18n::IsRTL();
    const bool is_match_contents_rtl =
        (render_text->GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT);
    const int offset =
        GetDisplayOffset(match, is_ui_rtl, is_match_contents_rtl);

    std::unique_ptr<gfx::RenderText> prefix_render_text(
        CreateRenderText(base::UTF8ToUTF16(
            match.GetAdditionalInfo(kACMatchPropertyContentsPrefix))));
    const int prefix_width = prefix_render_text->GetContentWidth();
    int prefix_x = x;

    const int max_match_contents_width = model_->max_match_contents_width();

    if (is_ui_rtl != is_match_contents_rtl) {
      // RTL infinite suggestions appear near the left edge in LTR UI, while LTR
      // infinite suggestions appear near the right edge in RTL UI. This is
      // against the natural horizontal alignment of the text. We reduce the
      // width of the box for suggestion display, so that the suggestions appear
      // in correct confines.  This reduced width allows us to modify the text
      // alignment (see below).
      right_x = x + std::min(remaining_width - prefix_width,
                             std::max(offset, max_match_contents_width));
      prefix_x = right_x;
      // We explicitly set the horizontal alignment so that when LTR suggestions
      // show in RTL UI (or vice versa), their ellipses appear stacked in a
      // single column.
      render_text->SetHorizontalAlignment(
          is_match_contents_rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
    } else {
      // If the dropdown is wide enough, place the ellipsis at the position
      // where the omitted text would have ended. Otherwise reduce the offset of
      // the ellipsis such that the widest suggestion reaches the end of the
      // dropdown.
      const int start_offset = std::max(prefix_width,
          std::min(remaining_width - max_match_contents_width, offset));
      right_x = x + std::min(remaining_width, start_offset + max_width);
      x += start_offset;
      prefix_x = x - prefix_width;
    }
    prefix_render_text->SetDirectionalityMode(is_match_contents_rtl ?
        gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR);
    prefix_render_text->SetHorizontalAlignment(
          is_match_contents_rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
    prefix_render_text->SetDisplayRect(
        gfx::Rect(mirroring_context_->mirrored_left_coord(
                      prefix_x, prefix_x + prefix_width),
                  y, prefix_width, GetContentLineHeight()));
    prefix_render_text->Draw(canvas);
  }

  // Set the display rect to trigger elision.
  const int final_width = right_x - x;
  int height = GetContentLineHeight();
  if (render_text_type == DESCRIPTION && match.answer) {
    render_text->SetDisplayRect(gfx::Rect(gfx::Size(final_width, 0)));
    render_text->GetStringSize();
    height = GetAnswerLineHeight() * render_text->GetNumLines();
  }
  render_text->SetDisplayRect(
      gfx::Rect(mirroring_context_->mirrored_left_coord(x, right_x), y,
                final_width, height));
  render_text->Draw(canvas);
  return right_x;
}

std::unique_ptr<gfx::RenderText> OmniboxResultView::CreateRenderText(
    const base::string16& text) const {
  std::unique_ptr<gfx::RenderText> render_text(
      gfx::RenderText::CreateInstance());
  render_text->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  render_text->SetFontList(font_list_);
  render_text->SetText(text);
  return render_text;
}

std::unique_ptr<gfx::RenderText> OmniboxResultView::CreateClassifiedRenderText(
    const base::string16& text,
    const ACMatchClassifications& classifications,
    bool force_dim) const {
  std::unique_ptr<gfx::RenderText> render_text(CreateRenderText(text));
  const size_t text_length = render_text->text().length();
  for (size_t i = 0; i < classifications.size(); ++i) {
    const size_t text_start = classifications[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end = (i < (classifications.size() - 1)) ?
        std::min(classifications[i + 1].offset, text_length) :
        text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if (classifications[i].style & ACMatchClassification::MATCH)
      render_text->ApplyWeight(gfx::Font::Weight::BOLD, current_range);

    ColorKind color_kind = TEXT;
    if (classifications[i].style & ACMatchClassification::URL) {
      color_kind = URL;
      // Consider logical string for domain "ABC.comי/hello" where ABC are
      // Hebrew (RTL) characters. This string should ideally show as
      // "CBA.com/hello". If we do not force LTR on URL, it will appear as
      // "com/hello.CBA".
      // With IDN and RTL TLDs, it might be okay to allow RTL rendering of URLs,
      // but it still has some pitfalls like :
      // ABC.COM/abc-pqr/xyz/FGH will appear as HGF/abc-pqr/xyz/MOC.CBA which
      // really confuses the path hierarchy of the URL.
      // Also, if the URL supports https, the appearance will change into LTR
      // directionality.
      // In conclusion, LTR rendering of URL is probably the safest bet.
      render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_FORCE_LTR);
    } else if (force_dim ||
        (classifications[i].style & ACMatchClassification::DIM)) {
      color_kind = DIMMED_TEXT;
    }
    render_text->ApplyColor(GetColor(GetState(), color_kind), current_range);
  }
  return render_text;
}

int OmniboxResultView::GetMatchContentsWidth() const {
  InitContentsRenderTextIfNecessary();
  contents_rendertext_->SetDisplayRect(gfx::Rect(gfx::Size(INT_MAX, 0)));
  return contents_rendertext_->GetContentWidth();
}

void OmniboxResultView::SetAnswerImage(const gfx::ImageSkia& image) {
  answer_image_ = image;
  SchedulePaint();
}

// TODO(skanuj): This is probably identical across all OmniboxResultView rows in
// the omnibox dropdown. Consider sharing the result.
int OmniboxResultView::GetDisplayOffset(
    const AutocompleteMatch& match,
    bool is_ui_rtl,
    bool is_match_contents_rtl) const {
  if (match.type != AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
    return 0;

  const base::string16& input_text =
      base::UTF8ToUTF16(match.GetAdditionalInfo(kACMatchPropertyInputText));
  int contents_start_index = 0;
  base::StringToInt(match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
                    &contents_start_index);

  std::unique_ptr<gfx::RenderText> input_render_text(
      CreateRenderText(input_text));
  const gfx::Range& glyph_bounds =
      input_render_text->GetGlyphBounds(contents_start_index);
  const int start_padding = is_match_contents_rtl ?
      std::max(glyph_bounds.start(), glyph_bounds.end()) :
      std::min(glyph_bounds.start(), glyph_bounds.end());

  return is_ui_rtl ?
      (input_render_text->GetContentWidth() - start_padding) : start_padding;
}

const char* OmniboxResultView::GetClassName() const {
  return "OmniboxResultView";
}

gfx::ImageSkia OmniboxResultView::GetIcon() const {
  const gfx::Image image = model_->GetIconIfExtensionMatch(model_index_);
  if (!image.IsEmpty())
    return image.AsImageSkia();

  return GetVectorIcon(
      model_->IsStarredMatch(match_)
          ? gfx::VectorIconId::OMNIBOX_STAR
          : AutocompleteMatch::TypeToVectorIcon(match_.type));
}

gfx::ImageSkia OmniboxResultView::GetVectorIcon(
    gfx::VectorIconId icon_id) const {
  // For selected rows, paint the icon the same color as the text.
  SkColor color = GetColor(GetState(), TEXT);
  if (GetState() != SELECTED)
    color = color_utils::DeriveDefaultIconColor(color);
  return gfx::CreateVectorIcon(icon_id, 16, color);
}

bool OmniboxResultView::ShowOnlyKeywordMatch() const {
  return match_.associated_keyword &&
      (keyword_icon_->x() <= icon_bounds_.right());
}

void OmniboxResultView::InitContentsRenderTextIfNecessary() const {
  if (!contents_rendertext_) {
    contents_rendertext_.reset(
        CreateClassifiedRenderText(
            match_.contents, match_.contents_class, false).release());
  }
}

void OmniboxResultView::Layout() {
  int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_HORIZONTAL_PADDING);
  // The horizontal bounds we're given are the outside bounds, so we can match
  // the omnibox border outline shape exactly in OnPaint().  We have to inset
  // here to keep the icons lined up.
  const int start_x =
      GetLayoutConstant(LOCATION_BAR_BORDER_THICKNESS) + horizontal_padding;
  const int end_x = width() - start_x;

  const gfx::ImageSkia icon = GetIcon();
  icon_bounds_.SetRect(start_x, (GetContentLineHeight() - icon.height()) / 2,
                       icon.width(), icon.height());

  const int text_x = start_x + LocationBarView::kIconWidth + horizontal_padding;
  int text_width = end_x - text_x;

  if (match_.associated_keyword.get()) {
    const int max_kw_x = end_x - keyword_icon_->width();
    const int kw_x = animation_->CurrentValueBetween(max_kw_x, start_x);
    const int kw_text_x = kw_x + keyword_icon_->width() + horizontal_padding;

    text_width = kw_x - text_x - horizontal_padding;
    keyword_text_bounds_.SetRect(
        kw_text_x, 0, std::max(end_x - kw_text_x, 0), height());
    keyword_icon_->SetPosition(
        gfx::Point(kw_x, (height() - keyword_icon_->height()) / 2));
  }

  text_bounds_.SetRect(text_x, 0, std::max(text_width, 0), height());
}

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  animation_->SetSlideDuration(width() / 4);
}

void OmniboxResultView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  // NOTE: While animating the keyword match, both matches may be visible.

  if (!ShowOnlyKeywordMatch()) {
    canvas->DrawImageInt(GetIcon(), GetMirroredXForRect(icon_bounds_),
                         icon_bounds_.y());
    int x = GetMirroredXForRect(text_bounds_);
    mirroring_context_->Initialize(x, text_bounds_.width());
    InitContentsRenderTextIfNecessary();

    if (!description_rendertext_) {
      if (match_.answer) {
        contents_rendertext_ =
            CreateAnswerLine(match_.answer->first_line(), font_list_);
        description_rendertext_ =
            CreateAnswerLine(match_.answer->second_line(), font_list_);
      } else if (!match_.description.empty()) {
        description_rendertext_ = CreateClassifiedRenderText(
            match_.description, match_.description_class, true);
      }
    }
    PaintMatch(match_, contents_rendertext_.get(),
               description_rendertext_.get(), canvas, x);
  }

  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  if (keyword_match) {
    int x = GetMirroredXForRect(keyword_text_bounds_);
    mirroring_context_->Initialize(x, keyword_text_bounds_.width());
    if (!keyword_contents_rendertext_) {
      keyword_contents_rendertext_.reset(
          CreateClassifiedRenderText(keyword_match->contents,
                                     keyword_match->contents_class,
                                     false).release());
    }
    if (!keyword_description_rendertext_ &&
        !keyword_match->description.empty()) {
      keyword_description_rendertext_.reset(
          CreateClassifiedRenderText(keyword_match->description,
                                     keyword_match->description_class,
                                     true).release());
    }
    PaintMatch(*keyword_match, keyword_contents_rendertext_.get(),
               keyword_description_rendertext_.get(), canvas, x);
  }
}

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  Layout();
  SchedulePaint();
}

int OmniboxResultView::GetAnswerLineHeight() const {
  // ANSWER_TEXT_LARGE is the largest font used and so defines the boundary that
  // all the other answer styles fit within.
  return ui::ResourceBundle::GetSharedInstance()
      .GetFontList(GetTextStyle(SuggestionAnswer::ANSWER_TEXT_LARGE).font)
      .GetHeight();
}

int OmniboxResultView::GetContentLineHeight() const {
  using Md = ui::MaterialDesignController;
  const int kIconVerticalPad = Md::GetMode() == Md::MATERIAL_HYBRID ? 8 : 4;
  const int kTextVerticalPad = 3;
  return std::max(
      LocationBarView::kIconWidth + 2 * kIconVerticalPad,
      GetTextHeight() + 2 * kTextVerticalPad);
}

std::unique_ptr<gfx::RenderText> OmniboxResultView::CreateAnswerLine(
    const SuggestionAnswer::ImageLine& line,
    gfx::FontList font_list) const {
  std::unique_ptr<gfx::RenderText> destination =
      CreateRenderText(base::string16());
  destination->SetFontList(font_list);

  for (const SuggestionAnswer::TextField& text_field : line.text_fields())
    AppendAnswerText(destination.get(), text_field.text(), text_field.type());
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
    AppendAnswerText(destination.get(), space + text_field->text(),
                     text_field->type());
  }
  text_field = line.status_text();
  if (text_field) {
    AppendAnswerText(destination.get(), space + text_field->text(),
                     text_field->type());
  }
  return destination;
}

void OmniboxResultView::AppendAnswerText(gfx::RenderText* destination,
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
      AppendAnswerTextHelper(destination, text.substr(begin), text_type, false);
      break;
    }
    AppendAnswerTextHelper(destination, text.substr(begin, end - begin),
                           text_type, false);
    begin = end + begin_tag.length();
    end = text.find(end_tag, begin);
    if (end == base::string16::npos)
      break;
    AppendAnswerTextHelper(destination, text.substr(begin, end - begin),
                           text_type, true);
    begin = end + end_tag.length();
  }
}

void OmniboxResultView::AppendAnswerTextHelper(gfx::RenderText* destination,
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
  destination->ApplyColor(
      GetNativeTheme()->GetSystemColor(text_style.colors[GetState()]), range);
  destination->ApplyBaselineStyle(text_style.baseline, range);
}
