// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
#pragma once

#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class AutocompleteResultViewModel;

namespace gfx {
class Canvas;
}

class AutocompleteResultView : public views::View,
                               private ui::AnimationDelegate {
 public:
  // Keep these ordered from least dominant (normal) to most dominant
  // (selected).
  enum ResultViewState {
    NORMAL = 0,
    HOVERED,
    SELECTED,
    NUM_STATES
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    DIMMED_TEXT,
    URL,
    DIVIDER,
    NUM_KINDS
  };

  AutocompleteResultView(AutocompleteResultViewModel* model,
                         int model_index,
                         const gfx::Font& font,
                         const gfx::Font& bold_font);
  virtual ~AutocompleteResultView();

  static SkColor GetColor(ResultViewState state, ColorKind kind);

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  void Invalidate();

  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  ResultViewState GetState() const;

 protected:
  virtual void PaintMatch(gfx::Canvas* canvas,
                          const AutocompleteMatch& match,
                          int x);

  // Returns the height of the text portion of the result view. In the base
  // class, this is the height of one line of text.
  virtual int GetTextHeight() const;

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

  const gfx::Rect& text_bounds() const { return text_bounds_; }

  void set_edge_item_padding(int value) { edge_item_padding_ = value; }
  void set_item_padding(int value) { item_padding_ = value; }
  void set_minimum_text_vertical_padding(int value) {
    minimum_text_vertical_padding_ = value;
  }

 private:
  struct ClassificationData;
  typedef std::vector<ClassificationData> Classifications;

  struct RunData;
  typedef std::vector<RunData> Runs;

  // Predicate functions for use when sorting the runs.
  static bool SortRunsLogically(const RunData& lhs, const RunData& rhs);
  static bool SortRunsVisually(const RunData& lhs, const RunData& rhs);

  const SkBitmap* GetIcon() const;
  const SkBitmap* GetKeywordIcon() const;

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

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  static int default_icon_size_;

  // Default values cached here, may be overridden using the setters above.
  int edge_item_padding_;
  int item_padding_;
  int minimum_text_vertical_padding_;

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

  AutocompleteMatch match_;

  gfx::Rect text_bounds_;
  gfx::Rect icon_bounds_;

  gfx::Rect keyword_text_bounds_;
  scoped_ptr<views::ImageView> keyword_icon_;

  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_H_
