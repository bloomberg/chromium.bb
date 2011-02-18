// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_result_view.h"

#include "base/i18n/bidi_line_iterator.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_result_view_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_LINUX)
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/gfx/skia_utils_gtk.h"
#endif

namespace {

const char16 kEllipsis[] = { 0x2026 };

// The minimum distance between the top and bottom of the {icon|text} and the
// top or bottom of the row.
const int kMinimumIconVerticalPadding = 2;
const int kMinimumTextVerticalPadding = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AutocompleteResultView, public:

// This class is a utility class for calculations affected by whether the result
// view is horizontally mirrored.  The drawing functions can be written as if
// all drawing occurs left-to-right, and then use this class to get the actual
// coordinates to begin drawing onscreen.
class AutocompleteResultView::MirroringContext {
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

AutocompleteResultView::AutocompleteResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const gfx::Font& font,
    const gfx::Font& bold_font)
    : icon_vertical_padding_(kMinimumIconVerticalPadding),
      text_vertical_padding_(kMinimumTextVerticalPadding),
      model_(model),
      model_index_(model_index),
      normal_font_(font),
      bold_font_(bold_font),
      ellipsis_width_(font.GetStringWidth(string16(kEllipsis))),
      mirroring_context_(new MirroringContext()),
      match_(NULL, 0, false, AutocompleteMatch::URL_WHAT_YOU_TYPED) {
  CHECK_GE(model_index, 0);
  if (icon_size_ == 0) {
    icon_size_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        AutocompleteMatch::TypeToIcon(AutocompleteMatch::URL_WHAT_YOU_TYPED))->
        width();
  }
}

AutocompleteResultView::~AutocompleteResultView() {
}

void AutocompleteResultView::OnPaint(gfx::Canvas* canvas) {
  const ResultViewState state = GetState();
  if (state != NORMAL)
    canvas->AsCanvasSkia()->drawColor(GetColor(state, BACKGROUND));

  // Paint the icon.
  canvas->DrawBitmapInt(*GetIcon(), GetMirroredXForRect(icon_bounds_),
                        icon_bounds_.y());

  // Paint the text.
  int x = GetMirroredXForRect(text_bounds_);
  mirroring_context_->Initialize(x, text_bounds_.width());
  PaintMatch(canvas, match_, x);
}

void AutocompleteResultView::Layout() {
  icon_bounds_.SetRect(LocationBarView::kEdgeItemPadding,
                       (height() - icon_size_) / 2, icon_size_, icon_size_);
  int text_x = icon_bounds_.right() + LocationBarView::kItemPadding;
  int font_height = std::max(normal_font_.GetHeight(), bold_font_.GetHeight());
  text_bounds_.SetRect(text_x, std::max(0, (height() - font_height) / 2),
      std::max(bounds().width() - text_x - LocationBarView::kEdgeItemPadding,
      0), font_height);
}

gfx::Size AutocompleteResultView::GetPreferredSize() {
  return gfx::Size(0, GetPreferredHeight(normal_font_, bold_font_));
}

int AutocompleteResultView::GetPreferredHeight(
    const gfx::Font& font,
    const gfx::Font& bold_font) {
  int text_height = std::max(font.GetHeight(), bold_font.GetHeight()) +
      (text_vertical_padding_ * 2);
  int icon_height = icon_size_ + (icon_vertical_padding_ * 2);
  return std::max(icon_height, text_height);
}

// static
SkColor AutocompleteResultView::GetColor(ResultViewState state,
                                         ColorKind kind) {
  static bool initialized = false;
  static SkColor colors[NUM_STATES][NUM_KINDS];
  if (!initialized) {
#if defined(OS_WIN)
    colors[NORMAL][BACKGROUND] = color_utils::GetSysSkColor(COLOR_WINDOW);
    colors[SELECTED][BACKGROUND] = color_utils::GetSysSkColor(COLOR_HIGHLIGHT);
    colors[NORMAL][TEXT] = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    colors[SELECTED][TEXT] = color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
#elif defined(OS_LINUX)
    GdkColor bg_color, selected_bg_color, text_color, selected_text_color;
    gtk_util::GetTextColors(
        &bg_color, &selected_bg_color, &text_color, &selected_text_color);
    colors[NORMAL][BACKGROUND] = gfx::GdkColorToSkColor(bg_color);
    colors[SELECTED][BACKGROUND] = gfx::GdkColorToSkColor(selected_bg_color);
    colors[NORMAL][TEXT] = gfx::GdkColorToSkColor(text_color);
    colors[SELECTED][TEXT] = gfx::GdkColorToSkColor(selected_text_color);
#else
    // TODO(beng): source from theme provider.
    colors[NORMAL][BACKGROUND] = SK_ColorWHITE;
    colors[SELECTED][BACKGROUND] = SK_ColorBLUE;
    colors[NORMAL][TEXT] = SK_ColorBLACK;
    colors[SELECTED][TEXT] = SK_ColorWHITE;
#endif
    colors[HOVERED][BACKGROUND] =
        color_utils::AlphaBlend(colors[SELECTED][BACKGROUND],
                                colors[NORMAL][BACKGROUND], 64);
    colors[HOVERED][TEXT] = colors[NORMAL][TEXT];
    for (int i = 0; i < NUM_STATES; ++i) {
      colors[i][DIMMED_TEXT] =
          color_utils::AlphaBlend(colors[i][TEXT], colors[i][BACKGROUND], 128);
      colors[i][URL] = color_utils::GetReadableColor(SkColorSetRGB(0, 128, 0),
                                                     colors[i][BACKGROUND]);
    }
    initialized = true;
  }

  return colors[state][kind];
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteResultView, protected:

void AutocompleteResultView::PaintMatch(gfx::Canvas* canvas,
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
    string16 separator =
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
bool AutocompleteResultView::SortRunsLogically(const RunData& lhs,
                                               const RunData& rhs) {
  return lhs.run_start < rhs.run_start;
}

// static
bool AutocompleteResultView::SortRunsVisually(const RunData& lhs,
                                              const RunData& rhs) {
  return lhs.visual_order < rhs.visual_order;
}

// static
int AutocompleteResultView::icon_size_ = 0;

AutocompleteResultView::ResultViewState
    AutocompleteResultView::GetState() const {
  if (model_->IsSelectedIndex(model_index_))
    return SELECTED;
  return model_->IsHoveredIndex(model_index_) ? HOVERED : NORMAL;
}

const SkBitmap* AutocompleteResultView::GetIcon() const {
  const SkBitmap* bitmap = model_->GetSpecialIcon(model_index_);
  if (bitmap)
    return bitmap;

  int icon = match_.starred ?
      IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
  if (model_->IsSelectedIndex(model_index_)) {
    switch (icon) {
      case IDR_OMNIBOX_HTTP:    icon = IDR_OMNIBOX_HTTP_SELECTED; break;
      case IDR_OMNIBOX_HISTORY: icon = IDR_OMNIBOX_HISTORY_SELECTED; break;
      case IDR_OMNIBOX_SEARCH:  icon = IDR_OMNIBOX_SEARCH_SELECTED; break;
      case IDR_OMNIBOX_STAR:    icon = IDR_OMNIBOX_STAR_SELECTED; break;
      default:             NOTREACHED(); break;
    }
  }
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(icon);
}

int AutocompleteResultView::DrawString(
    gfx::Canvas* canvas,
    const string16& text,
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

  // Split the text into visual runs.  We do this first so that we don't need to
  // worry about whether our eliding might change the visual display in
  // unintended ways, e.g. by removing directional markings or by adding an
  // ellipsis that's not enclosed in appropriate markings.
  base::i18n::BiDiLineIterator bidi_line;
  if (!bidi_line.Open(text, base::i18n::IsRTL(), is_url))
    return x;
  const int num_runs = bidi_line.CountRuns();
  Runs runs;
  for (int run = 0; run < num_runs; ++run) {
    int run_start_int = 0, run_length_int = 0;
    // The index we pass to GetVisualRun corresponds to the position of the run
    // in the displayed text. For example, the string "Google in HEBREW" (where
    // HEBREW is text in the Hebrew language) has two runs: "Google in " which
    // is an LTR run, and "HEBREW" which is an RTL run. In an LTR context, the
    // run "Google in " has the index 0 (since it is the leftmost run
    // displayed). In an RTL context, the same run has the index 1 because it
    // is the rightmost run. This is why the order in which we traverse the
    // runs is different depending on the locale direction.
    const UBiDiDirection run_direction = bidi_line.GetVisualRun(
        (base::i18n::IsRTL() && !is_url) ? (num_runs - run - 1) : run,
        &run_start_int, &run_length_int);
    DCHECK_GT(run_length_int, 0);
    runs.push_back(RunData());
    RunData* current_run = &runs.back();
    current_run->run_start = run_start_int;
    const size_t run_end = current_run->run_start + run_length_int;
    current_run->visual_order = run;
    current_run->is_rtl = !is_url && (run_direction == UBIDI_RTL);
    current_run->pixel_width = 0;

    // Compute classifications for this run.
    for (size_t i = 0; i < classifications.size(); ++i) {
      const size_t text_start =
          std::max(classifications[i].offset, current_run->run_start);
      if (text_start >= run_end)
        break;  // We're past the last classification in the run.

      const size_t text_end = (i < (classifications.size() - 1)) ?
          std::min(classifications[i + 1].offset, run_end) : run_end;
      if (text_end <= current_run->run_start)
        continue;  // We haven't reached the first classification in the run.

      current_run->classifications.push_back(ClassificationData());
      ClassificationData* current_data =
          &current_run->classifications.back();
      current_data->text = text.substr(text_start, text_end - text_start);

      // Calculate style-related data.
      const int style = classifications[i].style;
      const bool use_bold_font = !!(style & ACMatchClassification::MATCH);
      current_data->font = &(use_bold_font ? bold_font_ : normal_font_);
      const ResultViewState state = GetState();
      if (style & ACMatchClassification::URL)
        current_data->color = GetColor(state, URL);
      else if (style & ACMatchClassification::DIM)
        current_data->color = GetColor(state, DIMMED_TEXT);
      else
        current_data->color = GetColor(state, force_dim ? DIMMED_TEXT : TEXT);
      current_data->pixel_width =
          current_data->font->GetStringWidth(current_data->text);
      current_run->pixel_width += current_data->pixel_width;
    }
    DCHECK(!current_run->classifications.empty());
  }
  DCHECK(!runs.empty());

  // Sort into logical order so we can elide logically.
  std::sort(runs.begin(), runs.end(), &SortRunsLogically);

  // Now determine what to elide, if anything.  Several subtle points:
  //   * Because we have the run data, we can get edge cases correct, like
  //     whether to place an ellipsis before or after the end of a run when the
  //     text needs to be elided at the run boundary.
  //   * The "or one before it" comments below refer to cases where an earlier
  //     classification fits completely, but leaves too little space for an
  //     ellipsis that turns out to be needed later.  These cases are commented
  //     more completely in Elide().
  int remaining_width = mirroring_context_->remaining_width(x);
  for (Runs::iterator i(runs.begin()); i != runs.end(); ++i) {
    if (i->pixel_width > remaining_width) {
      // This run or one before it needs to be elided.
      for (Classifications::iterator j(i->classifications.begin());
           j != i->classifications.end(); ++j) {
        if (j->pixel_width > remaining_width) {
          // This classification or one before it needs to be elided.  Erase all
          // further classifications and runs so Elide() can simply reverse-
          // iterate over everything to find the specific classification to
          // elide.
          i->classifications.erase(++j, i->classifications.end());
          runs.erase(++i, runs.end());
          Elide(&runs, remaining_width);
          break;
        }
        remaining_width -= j->pixel_width;
      }
      break;
    }
    remaining_width -= i->pixel_width;
  }

  // Sort back into visual order so we can display the runs correctly.
  std::sort(runs.begin(), runs.end(), &SortRunsVisually);

  // Draw the runs.
  for (Runs::iterator i(runs.begin()); i != runs.end(); ++i) {
    const bool reverse_visible_order = (i->is_rtl != base::i18n::IsRTL());
    int flags = gfx::Canvas::NO_ELLIPSIS;  // We've already elided.
    if (reverse_visible_order) {
      std::reverse(i->classifications.begin(), i->classifications.end());
      if (i->is_rtl)
        flags |= gfx::Canvas::FORCE_RTL_DIRECTIONALITY;
    }
    for (Classifications::const_iterator j(i->classifications.begin());
         j != i->classifications.end(); ++j) {
      int left = mirroring_context_->mirrored_left_coord(x, x + j->pixel_width);
      canvas->DrawStringInt(j->text, *j->font, j->color, left,
                            y, j->pixel_width, j->font->GetHeight(), flags);
      x += j->pixel_width;
    }
  }

  return x;
}

void AutocompleteResultView::Elide(Runs* runs, int remaining_width) const {
  // The complexity of this function is due to edge cases like the following:
  // We have 100 px of available space, an initial classification that takes 86
  // px, and a font that has a 15 px wide ellipsis character.  Now if the first
  // classification is followed by several very narrow classifications (e.g. 3
  // px wide each), we don't know whether we need to elide or not at the time we
  // see the first classification -- it depends on how many subsequent
  // classifications follow, and some of those may be in the next run (or
  // several runs!).  This is why instead we let our caller move forward until
  // we know we definitely need to elide, and then in this function we move
  // backward again until we find a string that we can successfully do the
  // eliding on.
  bool first_classification = true;
  for (Runs::reverse_iterator i(runs->rbegin()); i != runs->rend(); ++i) {
    for (Classifications::reverse_iterator j(i->classifications.rbegin());
         j != i->classifications.rend(); ++j) {
      if (!first_classification) {
        // For all but the first classification we consider, we need to append
        // an ellipsis, since there isn't enough room to draw it after this
        // classification.
        j->text += kEllipsis;

        // We also add this classification's width (sans ellipsis) back to the
        // available width since we want to consider the available space we'll
        // have when we draw this classification.
        remaining_width += j->pixel_width;
      }
      first_classification = false;

      // Can we fit at least an ellipsis?
      string16 elided_text =
          ui::ElideText(j->text, *j->font, remaining_width, false);
      Classifications::reverse_iterator prior_classification(j);
      ++prior_classification;
      const bool on_first_classification =
        (prior_classification == i->classifications.rend());
      if (elided_text.empty() && (remaining_width >= ellipsis_width_) &&
          on_first_classification) {
        // Edge case: This classification is bold, we can't fit a bold ellipsis
        // but we can fit a normal one, and this is the first classification in
        // the run.  We should display a lone normal ellipsis, because appending
        // one to the end of the previous run might put it in the wrong visual
        // location (if the previous run is reversed from the normal visual
        // order).
        // NOTE: If this isn't the first classification in the run, we don't
        // need to bother with this; see note below.
        elided_text = kEllipsis;
      }
      if (!elided_text.empty()) {
        // Success.  Elide this classification and stop.
        j->text = elided_text;

        // If we could only fit an ellipsis, then only make it bold if there was
        // an immediate prior classification in this run that was also bold, or
        // it will look orphaned.
        if ((elided_text.length() == 1) &&
            (on_first_classification ||
             (prior_classification->font == &normal_font_)))
          j->font = &normal_font_;

        j->pixel_width = j->font->GetStringWidth(elided_text);

        // Erase any other classifications that come after the elided one.
        i->classifications.erase(j.base(), i->classifications.end());
        runs->erase(i.base(), runs->end());
        return;
      }

      // We couldn't fit an ellipsis.  Move back one classification,
      // append an ellipsis, and try again.
      // NOTE: In the edge case that a bold ellipsis doesn't fit but a
      // normal one would, and we reach here, then there is a previous
      // classification in this run, and so either:
      //   * It's normal, and will be able to draw successfully with the
      //     ellipsis we'll append to it, or
      //   * It is also bold, in which case we don't want to fall back
      //     to a normal ellipsis anyway (see comment above).
    }
  }

  // We couldn't draw anything.
  runs->clear();
}
