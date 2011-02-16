// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
#pragma once

#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "views/view.h"

class AutocompleteResultViewModel;
namespace gfx {
class Canvas;
}

class AutocompleteResultView : public views::View {
 public:
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
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Returns the preferred height for a single row.
  int GetPreferredHeight(const gfx::Font& font,
                         const gfx::Font& bold_font);
  static SkColor GetColor(ResultViewState state, ColorKind kind);

 protected:
  virtual void PaintMatch(gfx::Canvas* canvas,
                          const AutocompleteMatch& match,
                          int x);

  int icon_vertical_padding_;
  int text_vertical_padding_;

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

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
