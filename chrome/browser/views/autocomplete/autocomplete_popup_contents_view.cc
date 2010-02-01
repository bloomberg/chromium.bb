// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"

#include "app/bidi_line_iterator.h"
#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/gfx/insets.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/bubble_positioner.h"
#include "chrome/browser/views/bubble_border.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/icu/public/common/unicode/ubidi.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <commctrl.h>
#include <dwmapi.h>

#include "app/win_util.h"
#endif

namespace {

enum ResultViewState {
  NORMAL = 0,
  SELECTED,
  HOVERED,
  NUM_STATES
};

enum ColorKind {
  BACKGROUND = 0,
  TEXT,
  DIMMED_TEXT,
  URL,
  NUM_KINDS
};

SkColor GetColor(ResultViewState state, ColorKind kind) {
  static bool initialized = false;
  static SkColor colors[NUM_STATES][NUM_KINDS];
  if (!initialized) {
#if defined(OS_WIN)
    colors[NORMAL][BACKGROUND] = color_utils::GetSysSkColor(COLOR_WINDOW);
    colors[SELECTED][BACKGROUND] = color_utils::GetSysSkColor(COLOR_HIGHLIGHT);
    colors[NORMAL][TEXT] = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    colors[SELECTED][TEXT] = color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
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

const SkAlpha kGlassPopupAlpha = 240;
const SkAlpha kOpaquePopupAlpha = 255;
// The minimum distance between the top and bottom of the icon and the top or
// bottom of the row. "Minimum" is used because the vertical padding may be
// larger, depending on the size of the text.
const int kIconVerticalPadding = 2;
// The minimum distance between the top and bottom of the text and the top or
// bottom of the row. See comment about the use of "minimum" for
// kIconVerticalPadding.
const int kTextVerticalPadding = 3;
// The padding at the left edge of the row, left of the icon.
const int kRowLeftPadding = 6;
// The padding on the right edge of the row, right of the text.
const int kRowRightPadding = 3;
// The horizontal distance between the right edge of the icon and the left edge
// of the text.
const int kIconTextSpacing = 9;
// The size delta between the font used for the edit and the result rows. Passed
// to gfx::Font::DeriveFont.
#if !defined(OS_CHROMEOS)
const int kEditFontAdjust = -1;
#else
// Don't adjust font on chromeos as it becomes too small.
const int kEditFontAdjust = 0;
#endif

}

class AutocompleteResultView : public views::View {
 public:
  AutocompleteResultView(AutocompleteResultViewModel* model,
                         int model_index,
                         const gfx::Font& font);
  virtual ~AutocompleteResultView();

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void set_match(const AutocompleteMatch& match) { match_ = match; }

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  ResultViewState GetState() const;

  SkBitmap* GetIcon() const;

  // Draws the specified |text| into the canvas, using highlighting provided by
  // |classifications|. If |force_dim| is true, ACMatchClassification::DIM is
  // added to all of the classifications. Returns the x position to the right
  // of the string.
  int DrawString(gfx::Canvas* canvas,
                 const std::wstring& text,
                 const ACMatchClassifications& classifications,
                 bool force_dim,
                 int x,
                 int y);

  // Draws an individual sub-fragment with the specified style. Returns the x
  // position to the right of the fragment.
  int DrawStringFragment(gfx::Canvas* canvas,
                         const std::wstring& text,
                         int style,
                         int x,
                         int y,
                         bool force_rtl_directionality);

  // Gets the font and text color for a fragment with the specified style.
  gfx::Font GetFragmentFont(int style) const;
  SkColor GetFragmentTextColor(int style) const;

  // This row's model and model index.
  AutocompleteResultViewModel* model_;
  size_t model_index_;

  // The font used to derive fonts for rendering the text in this row.
  gfx::Font font_;

  // A context used for mirroring regions.
  class MirroringContext;
  scoped_ptr<MirroringContext> mirroring_context_;

  // Layout rects for various sub-components of the view.
  gfx::Rect icon_bounds_;
  gfx::Rect text_bounds_;

  // Icons for rows.
  static SkBitmap* icon_url_;
  static SkBitmap* icon_url_selected_;
  static SkBitmap* icon_history_;
  static SkBitmap* icon_history_selected_;
  static SkBitmap* icon_search_;
  static SkBitmap* icon_search_selected_;
  static SkBitmap* icon_more_;
  static SkBitmap* icon_more_selected_;
  static SkBitmap* icon_star_;
  static SkBitmap* icon_star_selected_;
  static int icon_size_;

  AutocompleteMatch match_;

  static bool initialized_;
  static void InitClass();

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultView);
};

// static
SkBitmap* AutocompleteResultView::icon_url_ = NULL;
SkBitmap* AutocompleteResultView::icon_url_selected_ = NULL;
SkBitmap* AutocompleteResultView::icon_history_ = NULL;
SkBitmap* AutocompleteResultView::icon_history_selected_ = NULL;
SkBitmap* AutocompleteResultView::icon_search_ = NULL;
SkBitmap* AutocompleteResultView::icon_search_selected_ = NULL;
SkBitmap* AutocompleteResultView::icon_star_ = NULL;
SkBitmap* AutocompleteResultView::icon_star_selected_ = NULL;
SkBitmap* AutocompleteResultView::icon_more_ = NULL;
SkBitmap* AutocompleteResultView::icon_more_selected_ = NULL;
int AutocompleteResultView::icon_size_ = 0;
bool AutocompleteResultView::initialized_ = false;

// This class is a utility class which mirrors an x position, calculates the
// index of the i-th run of a text, and calculates the index of the i-th
// fragment of a run.
// To render a styled text, we split a text into fragments and draw each
// fragment with the specified style. Unfortunately, it is not trivial to
// implement the above steps in a mirrored window.
// When we split a URL "www.google.com" into three fragments ('www.', 'google',
// and '.com') and draw them in a mirrored window as shown in the following
// steps, the output text becomes ".comgooglewww.".
// 1. Draw 'www.'
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.             | |             www.|
//      +-----------------+ +-----------------+
// 2. Draw 'google'
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.google       | |       googlewww.|
//      +-----------------+ +-----------------+
// 3. Draw '.com'
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.google.com   | |   .comgooglewww.|
//      +-----------------+ +-----------------+
// To fix this fragment-ordering problem, we should swap the run indices and
// fragment indices when rendering in a mirrred coordinate as listed below.
// 1. Draw 'www.' for LTR (or ".com" for RTL)
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.             | |             .com|
//      +-----------------+ +-----------------+
// 2. Draw 'google'
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.google       | |       google.com|
//      +-----------------+ +-----------------+
// 3. Draw '.com' for LTR (or "www." for RTL)
//      +-----------------+ +-----------------+
//      |LTR window       | |       RTL window|
//      +-----------------+ +-----------------+
//      |www.google.com   | |   www.google.com|
//      +-----------------+ +-----------------+
// This class encapsulates the above steps for AutocompleteResultView.
class AutocompleteResultView::MirroringContext {
 public:
  MirroringContext() : min_x_(0), center_x_(0), max_x_(0), mirrored_(false) { }

  // Initializes a mirroring context with the bounding region of a text.
  // This class uses the center of this region as an axis for calculating
  // mirrored coordinates.
  int Initialize(int x1, int x2, bool enabled);

  // Returns the "left" side of the specified region.
  // When the application language is a right-to-left one, this function
  // calculates the mirrored coordinates of the input region and returns the
  // left side of the mirrored region.
  // The input region must be in the bounding region specified in the
  // Initialize() function.
  int GetLeft(int x1, int x2) const;

  // Returns the index of the i-th run of a text.
  // When we split a text into runs, we need to write each run in the LTR
  // (or RTL) order if UI language is LTR (or RTL), respectively.
  int GetRun(int i, int size) const {
    return mirrored_ ? (size - i - 1) : i;
  }

  // Returns the index of the i-th text fragment of a run.
  // When we split a run into fragments, we need to write each fragment in the
  // LTR (or RTL) order if UI language is LTR (or RTL), respectively.
  size_t GetClassification(size_t i, size_t size, bool run_rtl) const {
    return (mirrored_ != run_rtl) ? (size - i - 1) : i;
  }

  // Returns whether or not the x coordinate is mirrored.
  bool mirrored() const {
    return mirrored_;
  }

 private:
  int min_x_;
  int center_x_;
  int max_x_;
  bool mirrored_;

  DISALLOW_COPY_AND_ASSIGN(MirroringContext);
};

int AutocompleteResultView::MirroringContext::Initialize(int x1, int x2,
                                                         bool mirrored) {
  min_x_ = std::min(x1, x2);
  max_x_ = std::max(x1, x2);
  center_x_ = min_x_ + (max_x_ - min_x_) / 2;
  mirrored_ = mirrored;
  return x1;
}

int AutocompleteResultView::MirroringContext::GetLeft(int x1, int x2) const {
  return mirrored_ ?
      (center_x_ + (center_x_ - std::max(x1, x2))) : std::min(x1, x2);
}

AutocompleteResultView::AutocompleteResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const gfx::Font& font)
    : model_(model),
      model_index_(model_index),
      font_(font),
      mirroring_context_(new MirroringContext()),
      match_(NULL, 0, false, AutocompleteMatch::URL_WHAT_YOU_TYPED) {
  CHECK(model_index >= 0);
  InitClass();
}

AutocompleteResultView::~AutocompleteResultView() {
}

void AutocompleteResultView::Paint(gfx::Canvas* canvas) {
  const ResultViewState state = GetState();
  if (state != NORMAL)
    canvas->drawColor(GetColor(state, BACKGROUND));

  int x = MirroredLeftPointForRect(icon_bounds_);

  // Paint the icon.
  canvas->DrawBitmapInt(*GetIcon(), x, icon_bounds_.y());

  // Paint the text.
  // Initialize the |mirroring_context_| with the left and right positions.
  // The DrawString() function uses this |mirroring_context_| to calculate the
  // position of an input text.
  bool text_mirroring = View::UILayoutIsRightToLeft();
  int text_left = MirroredLeftPointForRect(text_bounds_);
  int text_right = text_mirroring ? x - kIconTextSpacing : text_bounds_.right();
  x = mirroring_context_->Initialize(text_left, text_right, text_mirroring);
  x = DrawString(canvas, match_.contents, match_.contents_class, false, x,
                 text_bounds_.y());

  // Paint the description.
  if (!match_.description.empty()) {
    std::wstring separator =
        l10n_util::GetString(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
    ACMatchClassifications classifications;
    classifications.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    x = DrawString(canvas, separator, classifications, true, x,
                   text_bounds_.y());

    DrawString(canvas, match_.description, match_.description_class, true, x,
               text_bounds_.y());
  }
}

void AutocompleteResultView::Layout() {
  icon_bounds_.SetRect(kRowLeftPadding, (height() - icon_size_) / 2,
                       icon_size_, icon_size_);
  int text_x = icon_bounds_.right() + kIconTextSpacing;
  text_bounds_.SetRect(
      text_x,
      std::max(0, (height() - font_.height()) / 2),
      std::max(0, bounds().right() - text_x - kRowRightPadding),
      font_.height());
}

gfx::Size AutocompleteResultView::GetPreferredSize() {
  int text_height = font_.height() + 2 * kTextVerticalPadding;
  int icon_height = icon_size_ + 2 * kIconVerticalPadding;
  return gfx::Size(0, std::max(icon_height, text_height));
}


ResultViewState AutocompleteResultView::GetState() const {
  if (model_->IsSelectedIndex(model_index_))
    return SELECTED;
  return model_->IsHoveredIndex(model_index_) ? HOVERED : NORMAL;
}

SkBitmap* AutocompleteResultView::GetIcon() const {
  bool selected = model_->IsSelectedIndex(model_index_);
  if (match_.starred)
    return selected ? icon_star_selected_ : icon_star_;
  switch (match_.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::NAVSUGGEST:
      return selected ? icon_url_selected_ : icon_url_;
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD:
      return selected ? icon_history_selected_ : icon_history_;
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE:
      return selected ? icon_search_selected_ : icon_search_;
    case AutocompleteMatch::OPEN_HISTORY_PAGE:
      return selected ? icon_more_selected_ : icon_more_;
    default:
      NOTREACHED();
      return NULL;
  }
}

int AutocompleteResultView::DrawString(
    gfx::Canvas* canvas,
    const std::wstring& text,
    const ACMatchClassifications& classifications,
    bool force_dim,
    int x,
    int y) {
  if (!text.length())
    return x;

  // Initialize a bidirectional line iterator of ICU and split the text into
  // visual runs. (A visual run is consecutive characters which have the same
  // display direction and should be displayed at once.)
  BiDiLineIterator bidi_line;
  if (!bidi_line.Open(text, mirroring_context_->mirrored(), false))
    return x;
  const int runs = bidi_line.CountRuns();

  // Draw the visual runs.
  // This loop splits each run into text fragments with the given
  // classifications and draws the text fragments.
  // When the direction of a run is right-to-left, we have to mirror the
  // x-coordinate of this run and render the fragments in the right-to-left
  // reading order. To handle this display order independently from the one of
  // this popup window, this loop renders a run with the steps below:
  // 1. Create a local display context for each run;
  // 2. Render the run into the local display context, and;
  // 3. Copy the local display context to the one of the popup window.
  for (int run = 0; run < runs; ++run) {
    int run_start = 0;
    int run_length = 0;

    // The index we pass to GetVisualRun corresponds to the position of the run
    // in the displayed text. For example, the string "Google in HEBREW" (where
    // HEBREW is text in the Hebrew language) has two runs: "Google in " which
    // is an LTR run, and "HEBREW" which is an RTL run. In an LTR context, the
    // run "Google in " has the index 0 (since it is the leftmost run
    // displayed). In an RTL context, the same run has the index 1 because it
    // is the rightmost run. This is why the order in which we traverse the
    // runs is different depending on the locale direction.
    //
    // Note that for URLs we always traverse the runs from lower to higher
    // indexes because the return order of runs for a URL always matches the
    // physical order of the context.
    int current_run = mirroring_context_->GetRun(run, runs);
    const UBiDiDirection run_direction = bidi_line.GetVisualRun(current_run,
                                                                &run_start,
                                                                &run_length);
    const int run_end = run_start + run_length;

    // Split this run with the given classifications and draw the fragments.
    for (size_t classification = 0; classification < classifications.size();
         ++classification) {
      size_t i = mirroring_context_->GetClassification(
          classification, classifications.size(), run_direction == UBIDI_RTL);
      size_t text_start = std::max(static_cast<size_t>(run_start),
                                   classifications[i].offset);
      size_t text_end = std::min(static_cast<size_t>(run_end),
          i < classifications.size() - 1 ?
          classifications[i + 1].offset : run_end);
      int style = classifications[i].style;
      if (force_dim)
        style |= ACMatchClassification::DIM;

      // We specify RTL directionlity explicitly only if the run is an RTL run
      // and we can't specify the string directionlaity using an LRE/PDF pair.
      // Note that URLs are always displayed using LTR directionality
      // (regardless of the locale) and therefore they are excluded.
      const bool force_rtl_directionality =
           !(classifications[i].style & ACMatchClassification::URL) &&
           (run_direction == UBIDI_RTL) &&
           (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT);

      if (text_start < text_end) {
        x += DrawStringFragment(canvas,
                                text.substr(text_start, text_end - text_start),
                                style, x, y, force_rtl_directionality);
      }
    }
  }
  return x;
}

int AutocompleteResultView::DrawStringFragment(
    gfx::Canvas* canvas,
    const std::wstring& text,
    int style,
    int x,
    int y,
    bool force_rtl_directionality) {
  gfx::Font display_font = GetFragmentFont(style);
  // Clamp text width to the available width within the popup so we elide if
  // necessary.
  int string_width = std::min(display_font.GetStringWidth(text),
                              width() - kRowRightPadding - x);
  int string_left = mirroring_context_->GetLeft(x, x + string_width);
  const int flags = force_rtl_directionality ?
      gfx::Canvas::FORCE_RTL_DIRECTIONALITY : 0;
  canvas->DrawStringInt(text, GetFragmentFont(style),
                        GetFragmentTextColor(style), string_left, y,
                        string_width, display_font.height(), flags);
  return string_width;
}

gfx::Font AutocompleteResultView::GetFragmentFont(int style) const {
  return (style & ACMatchClassification::MATCH) ?
      font_.DeriveFont(0, gfx::Font::BOLD) : font_;
}

SkColor AutocompleteResultView::GetFragmentTextColor(int style) const {
  const ResultViewState state = GetState();
  if (style & ACMatchClassification::URL)
    return GetColor(state, URL);
  return GetColor(state,
      (style & ACMatchClassification::DIM) ? DIMMED_TEXT : TEXT);
}

void AutocompleteResultView::InitClass() {
  if (!initialized_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    icon_url_ = rb.GetBitmapNamed(IDR_O2_GLOBE);
    icon_url_selected_ = rb.GetBitmapNamed(IDR_O2_GLOBE_SELECTED);
    icon_history_ = rb.GetBitmapNamed(IDR_O2_HISTORY);
    icon_history_selected_ = rb.GetBitmapNamed(IDR_O2_HISTORY_SELECTED);
    icon_search_ = rb.GetBitmapNamed(IDR_O2_SEARCH);
    icon_search_selected_ = rb.GetBitmapNamed(IDR_O2_SEARCH_SELECTED);
    icon_star_ = rb.GetBitmapNamed(IDR_O2_STAR);
    icon_star_selected_ = rb.GetBitmapNamed(IDR_O2_STAR_SELECTED);
    icon_more_ = rb.GetBitmapNamed(IDR_O2_MORE);
    icon_more_selected_ = rb.GetBitmapNamed(IDR_O2_MORE_SELECTED);
    // All icons are assumed to be square, and the same size.
    icon_size_ = icon_url_->width();
    initialized_ = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, public:

AutocompletePopupContentsView::AutocompletePopupContentsView(
    const gfx::Font& font,
    AutocompleteEditView* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    const BubblePositioner* bubble_positioner)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      bubble_positioner_(bubble_positioner),
      result_font_(font.DeriveFont(kEditFontAdjust)),
      ignore_mouse_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(size_animation_(this)) {
  // The following little dance is required because set_border() requires a
  // pointer to a non-const object.
  BubbleBorder* bubble_border = new BubbleBorder;
  bubble_border_ = bubble_border;
  set_border(bubble_border);
}

gfx::Rect AutocompletePopupContentsView::GetPopupBounds() const {
  if (!size_animation_.IsAnimating())
    return target_bounds_;

  gfx::Rect current_frame_bounds = start_bounds_;
  int total_height_delta = target_bounds_.height() - start_bounds_.height();
  // Round |current_height_delta| instead of truncating so we won't leave single
  // white pixels at the bottom of the popup as long when animating very small
  // height differences.
  int current_height_delta = static_cast<int>(
      size_animation_.GetCurrentValue() * total_height_delta - 0.5);
  current_frame_bounds.set_height(
      current_frame_bounds.height() + current_height_delta);
  return current_frame_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AutocompletePopupView overrides:

bool AutocompletePopupContentsView::IsOpen() const {
  return (popup_ != NULL);
}

void AutocompletePopupContentsView::InvalidateLine(size_t line) {
  GetChildViewAt(static_cast<int>(line))->SchedulePaint();
}

void AutocompletePopupContentsView::UpdatePopupAppearance() {
  if (model_->result().empty()) {
    // No matches, close any existing popup.
    if (popup_ != NULL) {
      size_animation_.Stop();
      popup_->CloseNow();
      popup_.reset();
    }
    return;
  }

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  int total_child_height = 0;
  size_t child_view_count = GetChildViewCount();
  for (size_t i = 0; i < model_->result().size(); ++i) {
    AutocompleteResultView* result_view;
    if (i >= child_view_count) {
      result_view = new AutocompleteResultView(this, i, result_font_);
      AddChildView(result_view);
    } else {
      result_view = static_cast<AutocompleteResultView*>(GetChildViewAt(i));
    }
    result_view->set_match(GetMatchAtIndex(i));
    total_child_height += result_view->GetPreferredSize().height();
  }

  // Calculate desired bounds.
  gfx::Rect location_stack_bounds =
      bubble_positioner_->GetLocationStackBounds();
  gfx::Rect new_target_bounds(bubble_border_->GetBounds(location_stack_bounds,
      gfx::Size(location_stack_bounds.width(), total_child_height)));

  // If we're animating and our target height changes, reset the animation.
  // NOTE: If we just reset blindly on _every_ update, then when the user types
  // rapidly we could get "stuck" trying repeatedly to animate shrinking by the
  // last few pixels to get to one visible result.
  if (new_target_bounds.height() != target_bounds_.height())
    size_animation_.Reset();
  target_bounds_ = new_target_bounds;

  if (popup_ == NULL) {
    // If the popup is currently closed, we need to create it.
    popup_.reset(new AutocompletePopupClass(edit_view_, this));
  } else {
    // Animate the popup shrinking, but don't animate growing larger since that
    // would make the popup feel less responsive.
    GetWidget()->GetBounds(&start_bounds_, true);
    if (target_bounds_.height() < start_bounds_.height())
      size_animation_.Show();
    else
      start_bounds_ = target_bounds_;
    popup_->SetBounds(GetPopupBounds());
  }

  SchedulePaint();
}

void AutocompletePopupContentsView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void AutocompletePopupContentsView::OnDragCanceled() {
  ignore_mouse_drag_ = true;
}

AutocompletePopupModel* AutocompletePopupContentsView::GetModel() {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AutocompleteResultViewModel implementation:

bool AutocompletePopupContentsView::IsSelectedIndex(size_t index) const {
  return HasMatchAt(index) ? index == model_->selected_line() : false;
}

bool AutocompletePopupContentsView::IsHoveredIndex(size_t index) const {
  return HasMatchAt(index) ? index == model_->hovered_line() : false;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AnimationDelegate implementation:

void AutocompletePopupContentsView::AnimationProgressed(
    const Animation* animation) {
  // We should only be running the animation when the popup is already visible.
  DCHECK(popup_ != NULL);
  popup_->SetBounds(GetPopupBounds());
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, views::View overrides:

void AutocompletePopupContentsView::Paint(gfx::Canvas* canvas) {
  // We paint our children in an unconventional way.
  //
  // Because the border of this view creates an anti-aliased round-rect region
  // for the contents, we need to render our rectangular result child views into
  // this round rect region. We can't use a simple clip because clipping is
  // 1-bit and we get nasty jagged edges.
  //
  // Instead, we paint all our children into a second canvas and use that as a
  // shader to fill a path representing the round-rect clipping region. This
  // yields a nice anti-aliased edge.
  gfx::Canvas contents_canvas(width(), height(), true);
  contents_canvas.drawColor(GetColor(NORMAL, BACKGROUND));
  View::PaintChildren(&contents_canvas);
  // We want the contents background to be slightly transparent so we can see
  // the blurry glass effect on DWM systems behind. We do this _after_ we paint
  // the children since they paint text, and GDI will reset this alpha data if
  // we paint text after this call.
  MakeCanvasTransparent(&contents_canvas);

  // Now paint the contents of the contents canvas into the actual canvas.
  SkPaint paint;
  paint.setAntiAlias(true);

  SkShader* shader = SkShader::CreateBitmapShader(
      contents_canvas.getDevice()->accessBitmap(false),
      SkShader::kClamp_TileMode,
      SkShader::kClamp_TileMode);
  paint.setShader(shader);
  shader->unref();

  gfx::Path path;
  MakeContentsPath(&path, GetLocalBounds(false));
  canvas->drawPath(path, paint);

  // Now we paint the border, so it will be alpha-blended atop the contents.
  // This looks slightly better in the corners than drawing the contents atop
  // the border.
  PaintBorder(canvas);
}

void AutocompletePopupContentsView::Layout() {
  UpdateBlurRegion();

  // Size our children to the available content area.
  gfx::Rect contents_rect = GetLocalBounds(false);
  int child_count = GetChildViewCount();
  int top = contents_rect.y();
  for (int i = 0; i < child_count; ++i) {
    View* v = GetChildViewAt(i);
    v->SetBounds(contents_rect.x(), top, contents_rect.width(),
                 v->GetPreferredSize().height());
    top = v->bounds().bottom();
  }

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}


void AutocompletePopupContentsView::OnMouseEntered(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void AutocompletePopupContentsView::OnMouseMoved(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(GetIndexForPoint(event.location()));
}

void AutocompletePopupContentsView::OnMouseExited(
    const views::MouseEvent& event) {
  model_->SetHoveredLine(AutocompletePopupModel::kNoMatch);
}

bool AutocompletePopupContentsView::OnMousePressed(
    const views::MouseEvent& event) {
  ignore_mouse_drag_ = false;  // See comment on |ignore_mouse_drag_| in header.
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) {
    size_t index = GetIndexForPoint(event.location());
    model_->SetHoveredLine(index);
    if (HasMatchAt(index) && event.IsLeftMouseButton())
      model_->SetSelectedLine(index, false);
  }
  return true;
}

void AutocompletePopupContentsView::OnMouseReleased(
    const views::MouseEvent& event,
    bool canceled) {
  if (canceled || ignore_mouse_drag_) {
    ignore_mouse_drag_ = false;
    return;
  }

  size_t index = GetIndexForPoint(event.location());
  if (event.IsOnlyMiddleMouseButton())
    OpenIndex(index, NEW_BACKGROUND_TAB);
  else if (event.IsOnlyLeftMouseButton())
    OpenIndex(index, CURRENT_TAB);
}

bool AutocompletePopupContentsView::OnMouseDragged(
    const views::MouseEvent& event) {
  if (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) {
    size_t index = GetIndexForPoint(event.location());
    model_->SetHoveredLine(index);
    if (!ignore_mouse_drag_ && HasMatchAt(index) && event.IsLeftMouseButton())
      model_->SetSelectedLine(index, false);
  }
  return true;
}

views::View* AutocompletePopupContentsView::GetViewForPoint(
    const gfx::Point& /*point*/) {
  // This View takes control of the mouse events, so it should be considered the
  // active view for any point inside of it.
  return this;
}


////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, private:

bool AutocompletePopupContentsView::HasMatchAt(size_t index) const {
  return index < model_->result().size();
}

const AutocompleteMatch& AutocompletePopupContentsView::GetMatchAtIndex(
    size_t index) const {
  return model_->result().match_at(index);
}

void AutocompletePopupContentsView::MakeContentsPath(
    gfx::Path* path,
    const gfx::Rect& bounding_rect) {
  SkRect rect;
  rect.set(SkIntToScalar(bounding_rect.x()),
           SkIntToScalar(bounding_rect.y()),
           SkIntToScalar(bounding_rect.right()),
           SkIntToScalar(bounding_rect.bottom()));

  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path->addRoundRect(rect, radius, radius);
}

void AutocompletePopupContentsView::UpdateBlurRegion() {
#if defined(OS_WIN)
  // We only support background blurring on Vista with Aero-Glass enabled.
  if (!win_util::ShouldUseVistaFrame() || !GetWidget())
    return;

  // Provide a blurred background effect within the contents region of the
  // popup.
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  bb.fEnable = true;

  // Translate the contents rect into widget coordinates, since that's what
  // DwmEnableBlurBehindWindow expects a region in.
  gfx::Rect contents_rect = GetLocalBounds(false);
  gfx::Point origin(contents_rect.origin());
  views::View::ConvertPointToWidget(this, &origin);
  contents_rect.set_origin(origin);

  gfx::Path contents_path;
  MakeContentsPath(&contents_path, contents_rect);
  ScopedGDIObject<HRGN> popup_region;
  popup_region.Set(contents_path.CreateNativeRegion());
  bb.hRgnBlur = popup_region.Get();
  DwmEnableBlurBehindWindow(GetWidget()->GetNativeView(), &bb);
#endif
}

void AutocompletePopupContentsView::MakeCanvasTransparent(
    gfx::Canvas* canvas) {
  // Allow the window blur effect to show through the popup background.
  SkAlpha alpha = GetThemeProvider()->ShouldUseNativeFrame() ?
      kGlassPopupAlpha : kOpaquePopupAlpha;
  canvas->drawColor(SkColorSetA(GetColor(NORMAL, BACKGROUND), alpha),
                    SkXfermode::kDstIn_Mode);
}

void AutocompletePopupContentsView::OpenIndex(
    size_t index,
    WindowOpenDisposition disposition) {
  if (!HasMatchAt(index))
    return;

  const AutocompleteMatch& match = model_->result().match_at(index);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), index,
                      is_keyword_hint ? std::wstring() : keyword);
}

size_t AutocompletePopupContentsView::GetIndexForPoint(
    const gfx::Point& point) {
  if (!HitTest(point))
    return AutocompletePopupModel::kNoMatch;

  int nb_match = model_->result().size();
  DCHECK(nb_match <= GetChildViewCount());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = GetChildViewAt(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return i;
  }
  return AutocompletePopupModel::kNoMatch;
}

// static
AutocompletePopupView* AutocompletePopupView::CreatePopupView(
    const gfx::Font& font,
    AutocompleteEditView* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    const BubblePositioner* bubble_positioner) {
  return new AutocompletePopupContentsView(font, edit_view, edit_model,
                                           profile, bubble_positioner);
}
