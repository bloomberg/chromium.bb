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

#include <algorithm>  // NOLINT

#include "base/i18n/bidi_line_iterator.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(OS_WIN)
#include "ui/native_theme/native_theme_win.h"
#endif

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_aura.h"
#endif

namespace {

// The minimum distance between the top and bottom of the {icon|text} and the
// top or bottom of the row.
const int kMinimumIconVerticalPadding = 2;
const int kMinimumTextVerticalPadding = 3;

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
                                     LocationBarView* location_bar_view,
                                     const gfx::FontList& font_list)
    : edge_item_padding_(LocationBarView::GetItemPadding()),
      item_padding_(LocationBarView::GetItemPadding()),
      minimum_text_vertical_padding_(kMinimumTextVerticalPadding),
      model_(model),
      model_index_(model_index),
      location_bar_view_(location_bar_view),
      font_list_(font_list),
      font_height_(
          std::max(font_list.GetHeight(),
                   font_list.DeriveWithStyle(gfx::Font::BOLD).GetHeight())),
      mirroring_context_(new MirroringContext()),
      keyword_icon_(new views::ImageView()),
      animation_(new gfx::SlideAnimation(this)) {
  CHECK_GE(model_index, 0);
  if (default_icon_size_ == 0) {
    default_icon_size_ =
        location_bar_view_->GetThemeProvider()->GetImageSkiaNamed(
            AutocompleteMatch::TypeToIcon(
                AutocompleteMatchType::URL_WHAT_YOU_TYPED))->width();
  }
  if (ellipsis_width_ == 0)
    ellipsis_width_ = CreateRenderText(gfx::kEllipsisUTF16)->GetContentWidth();
  keyword_icon_->set_owned_by_client();
  keyword_icon_->EnableCanvasFlippingForRTLUI(true);
  keyword_icon_->SetImage(GetKeywordIcon());
  keyword_icon_->SizeToPreferredSize();
}

OmniboxResultView::~OmniboxResultView() {
}

SkColor OmniboxResultView::GetColor(
    ResultViewState state,
    ColorKind kind) const {
  const ui::NativeTheme* theme = GetNativeTheme();
#if defined(OS_WIN)
  if (theme == ui::NativeThemeWin::instance()) {
    static bool win_initialized = false;
    static SkColor win_colors[NUM_STATES][NUM_KINDS];
    if (!win_initialized) {
      win_colors[NORMAL][BACKGROUND] = color_utils::GetSysSkColor(COLOR_WINDOW);
      win_colors[SELECTED][BACKGROUND] =
          color_utils::GetSysSkColor(COLOR_HIGHLIGHT);
      win_colors[NORMAL][TEXT] = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
      win_colors[SELECTED][TEXT] =
          color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
      CommonInitColors(theme, win_colors);
      win_initialized = true;
    }
    return win_colors[state][kind];
  }
#endif
  static bool initialized = false;
  static SkColor colors[NUM_STATES][NUM_KINDS];
  if (!initialized) {
    colors[NORMAL][BACKGROUND] = theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldDefaultBackground);
    colors[NORMAL][TEXT] = theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldDefaultColor);
    colors[NORMAL][URL] = SkColorSetARGB(0xff, 0x00, 0x99, 0x33);
    colors[SELECTED][BACKGROUND] = theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);
    colors[SELECTED][TEXT] = theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldSelectionColor);
    colors[SELECTED][URL] = SkColorSetARGB(0xff, 0x00, 0x66, 0x22);
    colors[HOVERED][URL] = SkColorSetARGB(0xff, 0x00, 0x66, 0x22);
    CommonInitColors(theme, colors);
    initialized = true;
  }
  return colors[state][kind];
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match;
  match_contents_render_text_.reset();
  animation_->Reset();

  AutocompleteMatch* associated_keyword_match = match_.associated_keyword.get();
  if (associated_keyword_match) {
    keyword_icon_->SetImage(GetKeywordIcon());

    if (!keyword_icon_->parent())
      AddChildView(keyword_icon_.get());
  } else if (keyword_icon_->parent()) {
    RemoveChildView(keyword_icon_.get());
  }

  render_associated_keyword_match_ =
      associated_keyword_match && keyword_icon_->x() <= icon_bounds_.right();
  RenderMatchContents();
  Layout();
}

void OmniboxResultView::ShowKeyword(bool show_keyword) {
  if (show_keyword)
    animation_->Show();
  else
    animation_->Hide();
}

void OmniboxResultView::Invalidate() {
  keyword_icon_->SetImage(GetKeywordIcon());
  match_contents_render_text_.reset();
  SchedulePaint();
}

gfx::Size OmniboxResultView::GetPreferredSize() {
  return gfx::Size(0, std::max(
      default_icon_size_ + (kMinimumIconVerticalPadding * 2),
      GetTextHeight() + (minimum_text_vertical_padding_ * 2)));
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

void OmniboxResultView::PaintMatch(gfx::Canvas* canvas, int x) {
  const AutocompleteMatch& match = display_match();
  int y = text_bounds_.y();
  x = DrawRenderText(canvas, RenderMatchContents(), true, x, y);
  if (match.description.empty())
    return;

  const base::string16& separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  scoped_ptr<gfx::RenderText> separator_render_text(
      CreateRenderText(separator));
  separator_render_text->SetColor(GetColor(GetState(), DIMMED_TEXT));

  scoped_ptr<gfx::RenderText> description_render_text(
      CreateRenderText(match.description));
  ApplyClassifications(description_render_text.get(), match.description_class,
                       true);

  const int min_desc_width = separator_render_text->GetContentWidth() +
      std::min(description_render_text->GetContentWidth(), ellipsis_width_);
  if (mirroring_context_->remaining_width(x) < min_desc_width)
    return;

  x = DrawRenderText(canvas, separator_render_text.get(), false, x, y);

  DrawRenderText(canvas, description_render_text.get(), false, x, y);
}

int OmniboxResultView::DrawRenderText(
    gfx::Canvas* canvas,
    gfx::RenderText* render_text,
    bool contents,
    int x,
    int y) const {
  DCHECK(!render_text->text().empty());

  const int remaining_width = mirroring_context_->remaining_width(x);
  const int content_width = render_text->GetContentWidth();
  int right_x = x + std::min(remaining_width, content_width);

  const AutocompleteMatch& match = display_match();

  // Infinite suggestions should appear with the leading ellipses vertically
  // stacked.
  if (contents &&
      (match.type == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE)) {
    // When the directionality of suggestion doesn't match the UI, we try to
    // vertically stack the ellipsis by restricting the end edge (right_x).
    const bool is_ui_rtl = base::i18n::IsRTL();
    const bool is_match_contents_rtl =
        (render_text->GetTextDirection() == base::i18n::RIGHT_TO_LEFT);
    const int offset = GetDisplayOffset(is_ui_rtl, is_match_contents_rtl);

    scoped_ptr<gfx::RenderText> prefix_render_text(
        CreateRenderText(base::UTF8ToUTF16(
            match.GetAdditionalInfo("match contents prefix"))));
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
          std::min(remaining_width - (prefix_width + max_match_contents_width),
                   offset));
      right_x = x + std::min(remaining_width, start_offset + content_width);
      x += start_offset;
      prefix_x = x - prefix_width;
    }
    prefix_render_text->SetDirectionalityMode(is_match_contents_rtl ?
        gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR);
    prefix_render_text->SetHorizontalAlignment(
          is_match_contents_rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
    prefix_render_text->SetDisplayRect(gfx::Rect(
          mirroring_context_->mirrored_left_coord(
              prefix_x, prefix_x + prefix_width), y,
          prefix_width, height()));
    prefix_render_text->Draw(canvas);
  }

  // Set the display rect to trigger eliding.
  render_text->SetDisplayRect(gfx::Rect(
      mirroring_context_->mirrored_left_coord(x, right_x), y,
      right_x - x, height()));
  render_text->Draw(canvas);
  return right_x;
}

scoped_ptr<gfx::RenderText> OmniboxResultView::CreateRenderText(
    const base::string16& text) const {
  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateInstance());
  render_text->SetFontList(font_list_);
  render_text->SetText(text);
  render_text->SetElideBehavior(gfx::ELIDE_AT_END);
  return render_text.Pass();
}

void OmniboxResultView::ApplyClassifications(
    gfx::RenderText* render_text,
    const ACMatchClassifications& classifications,
    bool force_dim) const {
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
      render_text->ApplyStyle(gfx::BOLD, true, current_range);

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
}

gfx::RenderText* OmniboxResultView::RenderMatchContents() {
  if (!match_contents_render_text_) {
    const AutocompleteMatch& match = display_match();
    match_contents_render_text_.reset(
        CreateRenderText(match.contents).release());
    ApplyClassifications(match_contents_render_text_.get(),
                         match.contents_class, false);
  }
  return match_contents_render_text_.get();
}

int OmniboxResultView::GetMatchContentsWidth() const {
  return match_contents_render_text_->GetContentWidth();
}

// TODO(skanuj): This is probably identical across all OmniboxResultView rows in
// the omnibox dropdown. Consider sharing the result.
int OmniboxResultView::GetDisplayOffset(
    bool is_ui_rtl,
    bool is_match_contents_rtl) const {
  const AutocompleteMatch& match = display_match();
  if (match.type != AutocompleteMatchType::SEARCH_SUGGEST_INFINITE)
    return 0;

  const base::string16& input_text =
      base::UTF8ToUTF16(match.GetAdditionalInfo("input text"));
  int contents_start_index = 0;
  base::StringToInt(match.GetAdditionalInfo("match contents start index"),
                    &contents_start_index);

  scoped_ptr<gfx::RenderText> input_render_text(CreateRenderText(input_text));
  const gfx::Range& glyph_bounds =
      input_render_text->GetGlyphBounds(contents_start_index);
  const int start_padding = is_match_contents_rtl ?
      std::max(glyph_bounds.start(), glyph_bounds.end()) :
      std::min(glyph_bounds.start(), glyph_bounds.end());

  return is_ui_rtl ?
      (input_render_text->GetContentWidth() - start_padding) : start_padding;
}

// static
void OmniboxResultView::CommonInitColors(const ui::NativeTheme* theme,
                                         SkColor colors[][NUM_KINDS]) {
  colors[HOVERED][BACKGROUND] =
      color_utils::AlphaBlend(colors[SELECTED][BACKGROUND],
                              colors[NORMAL][BACKGROUND], 64);
  colors[HOVERED][TEXT] = colors[NORMAL][TEXT];
#if defined(USE_AURA)
  const bool is_aura = theme == ui::NativeThemeAura::instance();
#else
  const bool is_aura = false;
#endif
  for (int i = 0; i < NUM_STATES; ++i) {
    if (is_aura) {
      colors[i][TEXT] =
          color_utils::AlphaBlend(SK_ColorBLACK, colors[i][BACKGROUND], 0xdd);
      colors[i][DIMMED_TEXT] =
          color_utils::AlphaBlend(SK_ColorBLACK, colors[i][BACKGROUND], 0xbb);
    } else {
      colors[i][DIMMED_TEXT] =
          color_utils::AlphaBlend(colors[i][TEXT], colors[i][BACKGROUND], 128);
      colors[i][URL] = color_utils::GetReadableColor(SkColorSetRGB(0, 128, 0),
                                                     colors[i][BACKGROUND]);
    }

    // TODO(joi): Programmatically draw the dropdown border using
    // this color as well. (Right now it's drawn as black with 25%
    // alpha.)
    colors[i][DIVIDER] =
        color_utils::AlphaBlend(colors[i][TEXT], colors[i][BACKGROUND], 0x34);
  }
}

// static
int OmniboxResultView::default_icon_size_ = 0;

// static
int OmniboxResultView::ellipsis_width_ = 0;

gfx::ImageSkia OmniboxResultView::GetIcon() const {
  const gfx::Image image = model_->GetIconIfExtensionMatch(model_index_);
  if (!image.IsEmpty())
    return image.AsImageSkia();

  int icon = match_.starred ?
      IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
  if (GetState() == SELECTED) {
    switch (icon) {
      case IDR_OMNIBOX_EXTENSION_APP:
        icon = IDR_OMNIBOX_EXTENSION_APP_SELECTED;
        break;
      case IDR_OMNIBOX_HTTP:
        icon = IDR_OMNIBOX_HTTP_SELECTED;
        break;
      case IDR_OMNIBOX_SEARCH:
        icon = IDR_OMNIBOX_SEARCH_SELECTED;
        break;
      case IDR_OMNIBOX_STAR:
        icon = IDR_OMNIBOX_STAR_SELECTED;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return *(location_bar_view_->GetThemeProvider()->GetImageSkiaNamed(icon));
}

const gfx::ImageSkia* OmniboxResultView::GetKeywordIcon() const {
  // NOTE: If we ever begin returning icons of varying size, then callers need
  // to ensure that |keyword_icon_| is resized each time its image is reset.
  return location_bar_view_->GetThemeProvider()->GetImageSkiaNamed(
      (GetState() == SELECTED) ? IDR_OMNIBOX_TTS_SELECTED : IDR_OMNIBOX_TTS);
}


void OmniboxResultView::Layout() {
  const gfx::ImageSkia icon = GetIcon();

  icon_bounds_.SetRect(edge_item_padding_ +
      ((icon.width() == default_icon_size_) ?
          0 : LocationBarView::kIconInternalPadding),
      (height() - icon.height()) / 2, icon.width(), icon.height());

  int text_x = edge_item_padding_ + default_icon_size_ + item_padding_;
  int text_width = width() - text_x - edge_item_padding_;

  if (match_.associated_keyword.get()) {
    const int kw_collapsed_size =
        keyword_icon_->width() + edge_item_padding_;
    const int max_kw_x = width() - kw_collapsed_size;
    const int kw_x =
        animation_->CurrentValueBetween(max_kw_x, edge_item_padding_);
    const int kw_text_x = kw_x + keyword_icon_->width() + item_padding_;

    text_width = kw_x - text_x - item_padding_;
    keyword_text_bounds_.SetRect(
        kw_text_x, 0,
        std::max(width() - kw_text_x - edge_item_padding_, 0), height());
    keyword_icon_->SetPosition(
        gfx::Point(kw_x, (height() - keyword_icon_->height()) / 2));
  }

  text_bounds_.SetRect(text_x, 0, std::max(text_width, 0), height());
}

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  animation_->SetSlideDuration(width() / 4);
}

void OmniboxResultView::OnPaint(gfx::Canvas* canvas) {
  const ResultViewState state = GetState();
  if (state != NORMAL)
    canvas->DrawColor(GetColor(state, BACKGROUND));

  if (!render_associated_keyword_match_) {
    // Paint the icon.
    canvas->DrawImageInt(GetIcon(), GetMirroredXForRect(icon_bounds_),
                         icon_bounds_.y());
  }
  const gfx::Rect& text_bounds = render_associated_keyword_match_ ?
      keyword_text_bounds_ : text_bounds_;
  int x = GetMirroredXForRect(text_bounds);
  mirroring_context_->Initialize(x, text_bounds.width());
  PaintMatch(canvas, x);
}

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  Layout();
  SchedulePaint();
}
