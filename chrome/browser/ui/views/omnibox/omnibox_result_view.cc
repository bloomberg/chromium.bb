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
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view_model.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
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

const base::char16 kEllipsis[] = { 0x2026, 0x0 };

// The minimum distance between the top and bottom of the {icon|text} and the
// top or bottom of the row.
const int kMinimumIconVerticalPadding = 2;
const int kMinimumTextVerticalPadding = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

// Precalculated data used to draw a complete visual run within the match.
// This will include all or part of at least one, and possibly several,
// classifications.
struct OmniboxResultView::RunData {
  RunData() : run_start(0), visual_order(0), is_rtl(false), pixel_width(0) {}

  size_t run_start;  // Offset within the match text where this run begins.
  int visual_order;  // Where this run occurs in visual order.  The earliest
  // run drawn is run 0.
  bool is_rtl;
  int pixel_width;

  // Styled text classification pieces within this run, in logical order.
  Classifications classifications;
};

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

OmniboxResultView::OmniboxResultView(OmniboxResultViewModel* model,
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
      ellipsis_width_(gfx::GetStringWidth(base::string16(kEllipsis),
                                          font_list)),
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
  animation_->Reset();

  if (match.associated_keyword.get()) {
    keyword_icon_->SetImage(GetKeywordIcon());

    if (!keyword_icon_->parent())
      AddChildView(keyword_icon_.get());
  } else if (keyword_icon_->parent()) {
    RemoveChildView(keyword_icon_.get());
  }

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

void OmniboxResultView::PaintMatch(gfx::Canvas* canvas,
                                   const AutocompleteMatch& match,
                                   int x) {
  x = DrawString(canvas, match.contents, match.contents_class, false, x,
                 text_bounds_.y());

  // Paint the description.
  // TODO(pkasting): Because we paint in multiple separate pieces, we can wind
  // up with no space even for an ellipsis for one or both of these pieces.
  // Instead, we should paint the entire match as a single long string.  This
  // would also let us use a more properly-localizable string than we get with
  // just the IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR.
  if (!match.description.empty()) {
    base::string16 separator =
        l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
    ACMatchClassifications classifications;
    classifications.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    x = DrawString(canvas, separator, classifications, true, x,
                   text_bounds_.y());

    DrawString(canvas, match.description, match.description_class, true, x,
               text_bounds_.y());
  }
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
bool OmniboxResultView::SortRunsLogically(const RunData& lhs,
                                          const RunData& rhs) {
  return lhs.run_start < rhs.run_start;
}

// static
bool OmniboxResultView::SortRunsVisually(const RunData& lhs,
                                         const RunData& rhs) {
  return lhs.visual_order < rhs.visual_order;
}

// static
int OmniboxResultView::default_icon_size_ = 0;

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

int OmniboxResultView::DrawString(
    gfx::Canvas* canvas,
    const base::string16& text,
    const ACMatchClassifications& classifications,
    bool force_dim,
    int x,
    int y) {
  if (text.empty())
    return x;

  // Check whether or not this text is a URL.  URLs are always displayed LTR
  // regardless of locale.
  bool is_url = true;
  for (ACMatchClassifications::const_iterator i(classifications.begin());
       i != classifications.end(); ++i) {
    if (!(i->style & ACMatchClassification::URL)) {
      is_url = false;
      break;
    }
  }

  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateInstance());
  const size_t text_length = text.length();
  render_text->SetText(text);
  render_text->SetFontList(font_list_);
  render_text->SetMultiline(false);
  render_text->SetCursorEnabled(false);
  if (is_url)
    render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_FORCE_LTR);

  // Apply classifications.
  for (size_t i = 0; i < classifications.size(); ++i) {
    const size_t text_start = classifications[i].offset;
    if (text_start >= text_length)
      break;

    const size_t text_end = (i < (classifications.size() - 1)) ?
        std::min(classifications[i + 1].offset, text_length) : text_length;
    const gfx::Range current_range(text_start, text_end);

    // Calculate style-related data.
    if (classifications[i].style & ACMatchClassification::MATCH)
      render_text->ApplyStyle(gfx::BOLD, true, current_range);

    ColorKind color_kind = TEXT;
    if (classifications[i].style & ACMatchClassification::URL) {
      color_kind = URL;
    } else if (force_dim ||
        (classifications[i].style & ACMatchClassification::DIM)) {
      color_kind = DIMMED_TEXT;
    }
    render_text->ApplyColor(GetColor(GetState(), color_kind), current_range);
  }

  int remaining_width = mirroring_context_->remaining_width(x);

  // No need to try anything if we can't even show a solitary character.
  if ((text_length == 1) &&
      (remaining_width < render_text->GetContentWidth())) {
    return x;
  }

  if (render_text->GetContentWidth() > remaining_width)
    render_text->SetElideBehavior(gfx::ELIDE_AT_END);

  // Set the display rect to trigger eliding.
  render_text->SetDisplayRect(gfx::Rect(
      mirroring_context_->mirrored_left_coord(x, x + remaining_width), y,
      remaining_width, height()));
  render_text->set_clip_to_display_rect(true);
  render_text->Draw(canvas);

  // Need to call GetContentWidth again as the SetDisplayRect may modify it.
  return x + render_text->GetContentWidth();
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

  if (!match_.associated_keyword.get() ||
      keyword_icon_->x() > icon_bounds_.right()) {
    // Paint the icon.
    canvas->DrawImageInt(GetIcon(), GetMirroredXForRect(icon_bounds_),
                         icon_bounds_.y());

    // Paint the text.
    int x = GetMirroredXForRect(text_bounds_);
    mirroring_context_->Initialize(x, text_bounds_.width());
    PaintMatch(canvas, match_, x);
  }

  if (match_.associated_keyword.get()) {
    // Paint the keyword text.
    int x = GetMirroredXForRect(keyword_text_bounds_);
    mirroring_context_->Initialize(x, keyword_text_bounds_.width());
    PaintMatch(canvas, *match_.associated_keyword.get(), x);
  }
}

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  Layout();
  SchedulePaint();
}
