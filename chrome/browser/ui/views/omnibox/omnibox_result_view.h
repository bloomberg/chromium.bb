// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class LocationBarView;
class OmniboxPopupContentsView;

namespace gfx {
class Canvas;
class RenderText;
}

class OmniboxResultView : public views::View,
                          private gfx::AnimationDelegate {
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

  OmniboxResultView(OmniboxPopupContentsView* model,
                    int model_index,
                    LocationBarView* location_bar_view,
                    const gfx::FontList& font_list);
  virtual ~OmniboxResultView();

  SkColor GetColor(ResultViewState state, ColorKind kind) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  void Invalidate();

  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  ResultViewState GetState() const;

  // Returns the height of the text portion of the result view. In the base
  // class, this is the height of one line of text.
  virtual int GetTextHeight() const;

  // Returns the display width required for the match contents.
  int GetMatchContentsWidth() const;

 protected:
  virtual void PaintMatch(gfx::Canvas* canvas, int x);

  // Draws given |render_text| on |canvas| at given location (|x|, |y|).
  // |contents| determines any formatting difference between contents and
  // description parts of the omnibox result (see AutocompleteMatch). If
  // |max_width| is a non-negative number, the text will be elided to fit
  // within |max_width|. Returns the x position to the right of the string.
  int DrawRenderText(gfx::Canvas* canvas,
                     gfx::RenderText* render_text,
                     bool contents,
                     int x,
                     int y,
                     int max_width) const;

  // Creates a RenderText with given |text| and rendering defaults.
  scoped_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text) const;

  // Applies styles specified by |classifications| and |force_dim| in the range
  // from |range_start| to |range_end| in the |render_text|.
  void ApplyClassifications(
      gfx::RenderText* render_text,
      const ACMatchClassifications& classifications,
      bool force_dim) const;

  // Renders match contents at a suitable location in the bounds of this view.
  gfx::RenderText* RenderMatchContents();

  const gfx::Rect& text_bounds() const { return text_bounds_; }

  void set_edge_item_padding(int value) { edge_item_padding_ = value; }
  void set_item_padding(int value) { item_padding_ = value; }
  void set_minimum_text_vertical_padding(int value) {
    minimum_text_vertical_padding_ = value;
  }

  // Returns the match to be rendered for this row.
  const AutocompleteMatch& display_match() const {
    return render_associated_keyword_match_ ?
        *match_.associated_keyword.get() : match_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(OmniboxResultViewTest, CheckComputeMatchWidths);

  // Computes the maximum width, in pixels, that can be allocated for the two
  // parts of an autocomplete result, i.e. the contents and the description.
  static void ComputeMatchMaxWidths(int contents_width,
                                    int separator_width,
                                    int description_width,
                                    int available_width,
                                    bool allow_shrinking_contents,
                                    int* contents_max_width,
                                    int* description_max_width);

  // Common initialization code of the colors returned by GetColors().
  static void CommonInitColors(const ui::NativeTheme* theme,
                               SkColor colors[][NUM_KINDS]);

  gfx::ImageSkia GetIcon() const;
  const gfx::ImageSkia* GetKeywordIcon() const;

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // Returns the offset at which the suggestion should be displayed within the
  // text bounds. The directionality of UI and match contents is used to
  // determine the offset relative to the correct edge.
  int GetDisplayOffset(bool is_ui_rtl, bool is_match_contents_rtl) const;

  static int default_icon_size_;

  static int ellipsis_width_;

  // Default values cached here, may be overridden using the setters above.
  int edge_item_padding_;
  int item_padding_;
  int minimum_text_vertical_padding_;

  // This row's model and model index.
  OmniboxPopupContentsView* model_;
  size_t model_index_;

  LocationBarView* location_bar_view_;

  const gfx::FontList font_list_;
  int font_height_;

  // A context used for mirroring regions.
  class MirroringContext;
  scoped_ptr<MirroringContext> mirroring_context_;

  AutocompleteMatch match_;

  // Whether the associated keyword match should be rendered instead of the
  // original match.
  bool render_associated_keyword_match_;

  gfx::Rect text_bounds_;
  gfx::Rect icon_bounds_;

  gfx::Rect keyword_text_bounds_;
  scoped_ptr<views::ImageView> keyword_icon_;

  scoped_ptr<gfx::SlideAnimation> animation_;

  scoped_ptr<gfx::RenderText> match_contents_render_text_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
