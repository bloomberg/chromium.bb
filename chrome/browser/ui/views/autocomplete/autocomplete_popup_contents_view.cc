// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"

#include "base/compiler_specific.h"
#include "base/i18n/bidi_line_iterator.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/instant/promo_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "unicode/ubidi.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/painter.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include <commctrl.h>
#include <dwmapi.h>
#include <objidl.h>

#include "base/win/scoped_gdi_object.h"
#include "views/widget/widget_win.h"
#endif

#if defined(OS_LINUX)
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/gfx/skia_utils_gtk.h"
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

const char16 kEllipsis[] = { 0x2026 };

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
// The size delta between the font used for the edit and the result rows. Passed
// to gfx::Font::DeriveFont.
#if defined(OS_CHROMEOS)
// Don't adjust the size on Chrome OS (http://crbug.com/61433).
const int kEditFontAdjust = 0;
#else
const int kEditFontAdjust = -1;
#endif

// Horizontal padding between the buttons on the opt in promo.
const int kOptInButtonPadding = 2;

// Padding around the opt in view.
const int kOptInLeftPadding = 12;
const int kOptInRightPadding = 10;
const int kOptInTopPadding = 6;
const int kOptInBottomPadding = 5;

// Horizontal/Vertical inset of the promo background.
const int kOptInBackgroundHInset = 6;
const int kOptInBackgroundVInset = 2;

// Border for instant opt-in buttons. Consists of two 9 patch painters: one for
// the normal state, the other for the pressed state.
class OptInButtonBorder : public views::Border {
 public:
  OptInButtonBorder() {
    border_painter_.reset(CreatePainter(IDR_OPT_IN_BUTTON));
    border_pushed_painter_.reset(CreatePainter(IDR_OPT_IN_BUTTON_P));
  }

  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const {
    views::Painter* painter;
    if (static_cast<const views::CustomButton&>(view).state() ==
        views::CustomButton::BS_PUSHED) {
      painter = border_pushed_painter_.get();
    } else {
      painter = border_painter_.get();
    }
    painter->Paint(view.width(), view.height(), canvas);
  }

  virtual void GetInsets(gfx::Insets* insets) const {
    insets->Set(3, 8, 3, 8);
  }

 private:
  // Creates 9 patch painter from the image with the id |image_id|.
  views::Painter* CreatePainter(int image_id) {
    SkBitmap* image =
        ResourceBundle::GetSharedInstance().GetBitmapNamed(image_id);
    int w = image->width() / 2;
    if (image->width() % 2 == 0)
      w--;
    int h = image->height() / 2;
    if (image->height() % 2 == 0)
      h--;
    gfx::Insets insets(h, w, h, w);
    return views::Painter::CreateImagePainter(*image, insets, true);
  }

  scoped_ptr<views::Painter> border_painter_;
  scoped_ptr<views::Painter> border_pushed_painter_;

  DISALLOW_COPY_AND_ASSIGN(OptInButtonBorder);
};

}  // namespace

class AutocompletePopupContentsView::InstantOptInView
    : public views::View,
      public views::ButtonListener {
 public:
  InstantOptInView(AutocompletePopupContentsView* contents_view,
                   const gfx::Font& label_font,
                   const gfx::Font& button_font)
      : contents_view_(contents_view),
        bg_painter_(views::Painter::CreateVerticalGradient(
                        SkColorSetRGB(255, 242, 183),
                        SkColorSetRGB(250, 230, 145))) {
    views::Label* label = new views::Label(
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_INSTANT_OPT_IN_LABEL)));
    label->SetFont(label_font);

    views::GridLayout* layout = new views::GridLayout(this);
    layout->SetInsets(kOptInTopPadding, kOptInLeftPadding,
                      kOptInBottomPadding, kOptInRightPadding);
    SetLayoutManager(layout);

    const int first_column_set = 1;
    views::GridLayout::Alignment v_align = views::GridLayout::CENTER;
    views::ColumnSet* column_set = layout->AddColumnSet(first_column_set);
    column_set->AddColumn(views::GridLayout::TRAILING, v_align, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::CENTER, v_align, 0,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kOptInButtonPadding);
    column_set->AddColumn(views::GridLayout::CENTER, v_align, 0,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->LinkColumnSizes(2, 4, -1);
    layout->StartRow(0, first_column_set);
    layout->AddView(label);
    layout->AddView(CreateButton(IDS_INSTANT_OPT_IN_ENABLE, button_font));
    layout->AddView(CreateButton(IDS_INSTANT_OPT_IN_NO_THANKS, button_font));
  }

  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    contents_view_->UserPressedOptIn(
        sender->tag() == IDS_INSTANT_OPT_IN_ENABLE);
    // WARNING: we've been deleted.
  }

  virtual void Paint(gfx::Canvas* canvas) {
    canvas->Save();
    canvas->TranslateInt(kOptInBackgroundHInset, kOptInBackgroundVInset);
    bg_painter_->Paint(width() - kOptInBackgroundHInset * 2,
                       height() - kOptInBackgroundVInset * 2, canvas);
    canvas->DrawRectInt(ResourceBundle::toolbar_separator_color, 0, 0,
                        width() - kOptInBackgroundHInset * 2,
                        height() - kOptInBackgroundVInset * 2);
    canvas->Restore();
  }

 private:
  // Creates and returns a button configured for the opt-in promo.
  views::View* CreateButton(int id, const gfx::Font& font) {
    // NOTE: we can't use NativeButton as the popup is a layered window and
    // native buttons don't draw  in layered windows.
    // TODO: these buttons look crap. Figure out the right border/background to
    // use.
    views::TextButton* button =
        new views::TextButton(this, UTF16ToWide(l10n_util::GetStringUTF16(id)));
    button->set_border(new OptInButtonBorder());
    button->SetNormalHasBorder(true);
    button->set_tag(id);
    button->SetFont(font);
    button->set_animate_on_state_change(false);
    return button;
  }

  AutocompletePopupContentsView* contents_view_;
  scoped_ptr<views::Painter> bg_painter_;

  DISALLOW_COPY_AND_ASSIGN(InstantOptInView);
};

class AutocompleteResultView : public views::View {
 public:
  AutocompleteResultView(AutocompleteResultViewModel* model,
                         int model_index,
                         const gfx::Font& font,
                         const gfx::Font& bold_font);
  virtual ~AutocompleteResultView();

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void set_match(const AutocompleteMatch& match) { match_ = match; }

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Returns the preferred height for a single row.
  static int GetPreferredHeight(const gfx::Font& font,
                                const gfx::Font& bold_font);

 private:
  // Precalculated data used to draw the portion of a match classification that
  // fits entirely within one run.
  struct ClassificationData {
    string16 text;
    const gfx::Font* font;
    SkColor color;
    int pixel_width;
  };
  typedef std::vector<ClassificationData> Classifications;

  // Precalculated data used to draw a complete visual run within the match.
  // This will include all or part of at leasdt one, and possibly several,
  // classifications.
  struct RunData {
    size_t run_start;  // Offset within the match text where this run begins.
    int visual_order;  // Where this run occurs in visual order.  The earliest
                       // run drawn is run 0.
    bool is_rtl;
    int pixel_width;
    Classifications classifications;  // Classification pieces within this run,
                                      // in logical order.
  };
  typedef std::vector<RunData> Runs;

  // Predicate functions for use when sorting the runs.
  static bool SortRunsLogically(const RunData& lhs, const RunData& rhs);
  static bool SortRunsVisually(const RunData& lhs, const RunData& rhs);

  ResultViewState GetState() const;

  const SkBitmap* GetIcon() const;

  // Draws the specified |text| into the canvas, using highlighting provided by
  // |classifications|. If |force_dim| is true, ACMatchClassification::DIM is
  // added to all of the classifications. Returns the x position to the right
  // of the string.
  int DrawString(gfx::Canvas* canvas,
                 const string16& text,
                 const ACMatchClassifications& classifications,
                 bool force_dim,
                 int x,
                 int y);

  // Elides |runs| to fit in |remaining_width|.  The runs in |runs| should be in
  // logical order.
  //
  // When we need to elide a run, the ellipsis will be placed at the end of that
  // run.  This means that if we elide a run whose visual direction is opposite
  // that of the drawing context, the ellipsis will not be at the "end" of the
  // drawn string.  For example, if in an LTR context we have the LTR run
  // "LTR_STRING" and the RTL run "RTL_STRING", the unelided text would be drawn
  // like:
  //     LTR_STRING GNIRTS_LTR
  // If we need to elide the RTL run, then it will be drawn like:
  //     LTR_STRING ...RTS_LTR
  // Instead of:
  //     LTR_STRING RTS_LTR...
  void Elide(Runs* runs, int remaining_width) const;

  // This row's model and model index.
  AutocompleteResultViewModel* model_;
  size_t model_index_;

  const gfx::Font normal_font_;
  const gfx::Font bold_font_;

  // Width of the ellipsis in the normal font.
  int ellipsis_width_;

  // A context used for mirroring regions.
  class MirroringContext;
  scoped_ptr<MirroringContext> mirroring_context_;

  // Layout rects for various sub-components of the view.
  gfx::Rect icon_bounds_;
  gfx::Rect text_bounds_;

  static int icon_size_;

  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultView);
};

// static
int AutocompleteResultView::icon_size_ = 0;

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
    : model_(model),
      model_index_(model_index),
      normal_font_(font),
      bold_font_(bold_font),
      ellipsis_width_(font.GetStringWidth(string16(kEllipsis))),
      mirroring_context_(new MirroringContext()),
      match_(NULL, 0, false, AutocompleteMatch::URL_WHAT_YOU_TYPED) {
  CHECK(model_index >= 0);
  if (icon_size_ == 0) {
    icon_size_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        AutocompleteMatch::TypeToIcon(AutocompleteMatch::URL_WHAT_YOU_TYPED))->
        width();
  }
}

AutocompleteResultView::~AutocompleteResultView() {
}

void AutocompleteResultView::Paint(gfx::Canvas* canvas) {
  const ResultViewState state = GetState();
  if (state != NORMAL)
    canvas->AsCanvasSkia()->drawColor(GetColor(state, BACKGROUND));

  // Paint the icon.
  canvas->DrawBitmapInt(*GetIcon(), GetMirroredXForRect(icon_bounds_),
                        icon_bounds_.y());

  // Paint the text.
  int x = GetMirroredXForRect(text_bounds_);
  mirroring_context_->Initialize(x, text_bounds_.width());
  x = DrawString(canvas, match_.contents, match_.contents_class, false, x,
                 text_bounds_.y());

  // Paint the description.
  // TODO(pkasting): Because we paint in multiple separate pieces, we can wind
  // up with no space even for an ellipsis for one or both of these pieces.
  // Instead, we should paint the entire match as a single long string.  This
  // would also let us use a more properly-localizable string than we get with
  // just the IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR.
  if (!match_.description.empty()) {
    string16 separator =
        l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
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

// static
int AutocompleteResultView::GetPreferredHeight(
    const gfx::Font& font,
    const gfx::Font& bold_font) {
  int text_height = std::max(font.GetHeight(), bold_font.GetHeight()) +
      (kTextVerticalPadding * 2);
  int icon_height = icon_size_ + (kIconVerticalPadding * 2);
  return std::max(icon_height, text_height);
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

ResultViewState AutocompleteResultView::GetState() const {
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

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, public:

AutocompletePopupContentsView::AutocompletePopupContentsView(
    const gfx::Font& font,
    AutocompleteEditView* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    const views::View* location_bar)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      location_bar_(location_bar),
      result_font_(font.DeriveFont(kEditFontAdjust)),
      result_bold_font_(result_font_.DeriveFont(0, gfx::Font::BOLD)),
      ignore_mouse_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(size_animation_(this)),
      opt_in_view_(NULL) {
  // The following little dance is required because set_border() requires a
  // pointer to a non-const object.
  BubbleBorder* bubble_border = new BubbleBorder(BubbleBorder::NONE);
  bubble_border_ = bubble_border;
  set_border(bubble_border);
}

AutocompletePopupContentsView::~AutocompletePopupContentsView() {
  // We don't need to do anything with |popup_| here.  The OS either has already
  // closed the window, in which case it's been deleted, or it will soon, in
  // which case there's nothing we need to do.
}

gfx::Rect AutocompletePopupContentsView::GetPopupBounds() const {
  if (!size_animation_.is_animating())
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
      // NOTE: Do NOT use CloseNow() here, as we may be deep in a callstack
      // triggered by the popup receiving a message (e.g. LBUTTONUP), and
      // destroying the popup would cause us to read garbage when we unwind back
      // to that level.
      popup_->Close();  // This will eventually delete the popup.
      popup_.reset();
    }
    return;
  }

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  int total_child_height = 0;
  size_t child_rv_count = child_count();
  if (opt_in_view_) {
    DCHECK(child_rv_count > 0);
    child_rv_count--;
  }
  for (size_t i = 0; i < model_->result().size(); ++i) {
    AutocompleteResultView* result_view;
    if (i >= child_rv_count) {
      result_view =
          new AutocompleteResultView(this, i, result_font_, result_bold_font_);
      AddChildViewAt(result_view, static_cast<int>(i));
    } else {
      result_view = static_cast<AutocompleteResultView*>(GetChildViewAt(i));
      result_view->SetVisible(true);
    }
    result_view->set_match(GetMatchAtIndex(i));
    total_child_height += result_view->GetPreferredSize().height();
  }
  for (size_t i = model_->result().size(); i < child_rv_count; ++i)
    GetChildViewAt(i)->SetVisible(false);

  PromoCounter* counter = model_->profile()->GetInstantPromoCounter();
  if (!opt_in_view_ && counter && counter->ShouldShow(base::Time::Now())) {
    opt_in_view_ = new InstantOptInView(this, result_bold_font_, result_font_);
    AddChildView(opt_in_view_);
  } else if (opt_in_view_ && (!counter ||
                              !counter->ShouldShow(base::Time::Now()))) {
    delete opt_in_view_;
    opt_in_view_ = NULL;
  }

  if (opt_in_view_)
    total_child_height += opt_in_view_->GetPreferredSize().height();

  gfx::Rect new_target_bounds = CalculateTargetBounds(total_child_height);

  // If we're animating and our target height changes, reset the animation.
  // NOTE: If we just reset blindly on _every_ update, then when the user types
  // rapidly we could get "stuck" trying repeatedly to animate shrinking by the
  // last few pixels to get to one visible result.
  if (new_target_bounds.height() != target_bounds_.height())
    size_animation_.Reset();
  target_bounds_ = new_target_bounds;

  if (popup_ == NULL) {
    // If the popup is currently closed, we need to create it.
    popup_ = (new AutocompletePopupClass(edit_view_, this))->AsWeakPtr();
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

gfx::Rect AutocompletePopupContentsView::GetTargetBounds() {
  return target_bounds_;
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

const SkBitmap* AutocompletePopupContentsView::GetSpecialIcon(
    size_t index) const {
  if (!HasMatchAt(index))
    return NULL;
  return model_->GetSpecialIconForMatch(GetMatchAtIndex(index));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AnimationDelegate implementation:

void AutocompletePopupContentsView::AnimationProgressed(
    const ui::Animation* animation) {
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
  gfx::CanvasSkia contents_canvas(width(), height(), true);
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
  MakeContentsPath(&path, GetContentsBounds());
  canvas->AsCanvasSkia()->drawPath(path, paint);

  // Now we paint the border, so it will be alpha-blended atop the contents.
  // This looks slightly better in the corners than drawing the contents atop
  // the border.
  PaintBorder(canvas);
}

void AutocompletePopupContentsView::Layout() {
  UpdateBlurRegion();

  // Size our children to the available content area.
  gfx::Rect contents_rect = GetContentsBounds();
  int top = contents_rect.y();
  for (int i = 0; i < child_count(); ++i) {
    View* v = GetChildViewAt(i);
    if (v->IsVisible()) {
      v->SetBounds(contents_rect.x(), top, contents_rect.width(),
                   v->GetPreferredSize().height());
      top = v->bounds().bottom();
    }
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
      model_->SetSelectedLine(index, false, false);
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
      model_->SetSelectedLine(index, false, false);
  }
  return true;
}

views::View* AutocompletePopupContentsView::GetViewForPoint(
    const gfx::Point& point) {
  // If there is no opt in view, then we want all mouse events. Otherwise let
  // any descendants of the opt-in view get mouse events.
  if (!opt_in_view_)
    return this;

  views::View* child = views::View::GetViewForPoint(point);
  views::View* ancestor = child;
  while (ancestor && ancestor != opt_in_view_)
    ancestor = ancestor->parent();
  return ancestor ? child : this;
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
  if (!views::WidgetWin::IsAeroGlassEnabled() || !GetWidget())
    return;

  // Provide a blurred background effect within the contents region of the
  // popup.
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  bb.fEnable = true;

  // Translate the contents rect into widget coordinates, since that's what
  // DwmEnableBlurBehindWindow expects a region in.
  gfx::Rect contents_rect = GetContentsBounds();
  gfx::Point origin(contents_rect.origin());
  views::View::ConvertPointToWidget(this, &origin);
  contents_rect.set_origin(origin);

  gfx::Path contents_path;
  MakeContentsPath(&contents_path, contents_rect);
  base::win::ScopedGDIObject<HRGN> popup_region;
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
  canvas->AsCanvasSkia()->drawColor(
      SkColorSetA(GetColor(NORMAL, BACKGROUND), alpha),
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
  string16 keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), index,
                      is_keyword_hint ? string16() : keyword);
}

size_t AutocompletePopupContentsView::GetIndexForPoint(
    const gfx::Point& point) {
  if (!HitTest(point))
    return AutocompletePopupModel::kNoMatch;

  int nb_match = model_->result().size();
  DCHECK(nb_match <= child_count());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = GetChildViewAt(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToView(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return i;
  }
  return AutocompletePopupModel::kNoMatch;
}

gfx::Rect AutocompletePopupContentsView::CalculateTargetBounds(int h) {
  gfx::Rect location_bar_bounds(location_bar_->GetContentsBounds());
  const views::Border* border = location_bar_->border();
  if (border) {
    // Adjust for the border so that the bubble and location bar borders are
    // aligned.
    gfx::Insets insets;
    border->GetInsets(&insets);
    location_bar_bounds.Inset(insets.left(), 0, insets.right(), 0);
  } else {
    // The normal location bar is drawn using a background graphic that includes
    // the border, so we inset by enough to make the edges line up, and the
    // bubble appear at the same height as the Star bubble.
    location_bar_bounds.Inset(LocationBarView::kNormalHorizontalEdgeThickness,
                              0);
  }
  gfx::Point location_bar_origin(location_bar_bounds.origin());
  views::View::ConvertPointToScreen(location_bar_, &location_bar_origin);
  location_bar_bounds.set_origin(location_bar_origin);
  return bubble_border_->GetBounds(
      location_bar_bounds, gfx::Size(location_bar_bounds.width(), h));
}

void AutocompletePopupContentsView::UserPressedOptIn(bool opt_in) {
  delete opt_in_view_;
  opt_in_view_ = NULL;
  PromoCounter* counter = model_->profile()->GetInstantPromoCounter();
  DCHECK(counter);
  counter->Hide();
  if (opt_in) {
    browser::ShowInstantConfirmDialogIfNecessary(
        location_bar_->GetWindow()->GetNativeWindow(), model_->profile());
  }
  UpdatePopupAppearance();
}
