// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class OmniboxPopupContentsView;
enum class OmniboxPartState;
enum class OmniboxTint;

namespace gfx {
class Canvas;
class Image;
}

class OmniboxTabSwitchButton;
class OmniboxTextView;

class OmniboxResultView : public views::View,
                          private gfx::AnimationDelegate {
 public:
  // Keep these ordered from least dominant (normal) to most dominant
  // (selected).
  // TODO(tapted): Remove these: replace with OmniboxPartState.
  enum ResultViewState {
    NORMAL = 0,
    HOVERED,
    SELECTED,
    NUM_STATES
  };

  enum ColorKind {
    TEXT,
    DIMMED_TEXT,
    URL,
    INVISIBLE_TEXT,
    NUM_KINDS
  };

  OmniboxResultView(OmniboxPopupContentsView* model,
                    int model_index,
                    const gfx::FontList& font_list);
  ~OmniboxResultView() override;

  SkColor GetColor(ResultViewState state, ColorKind kind) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  void Invalidate();

  // Invoked when this result view has been selected.
  void OnSelected();

  ResultViewState GetState() const;
  OmniboxPartState GetThemeState() const;
  OmniboxTint GetTint() const;

  // Notification that the match icon has changed and schedules a repaint.
  void OnMatchIconUpdated();

  // Stores the image in a local data member and schedules a repaint.
  void SetAnswerImage(const gfx::ImageSkia& image);

  // Allow other classes to trigger navigation.
  void OpenMatch(WindowOpenDisposition disposition);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  // Returns the height of the text portion of the result view.
  int GetTextHeight() const;

  gfx::Image GetIcon() const;

  SkColor GetVectorIconColor() const;

  // Whether to render only the keyword match.  Returns true if |match_| has an
  // associated keyword match that has been animated so close to the start that
  // the keyword match will hide even the icon of the regular match.
  bool ShowOnlyKeywordMatch() const;

  // Returns the height of the the description section of answer suggestions.
  int GetAnswerHeight() const;

  // Returns the margin that should appear at the top and bottom of the result.
  int GetVerticalMargin() const;

  // Sets the hovered state of this result.
  void SetHovered(bool hovered);

  // views::View:
  void Layout() override;
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // This row's model and model index.
  OmniboxPopupContentsView* model_;
  size_t model_index_;

  // Whether this view is in the hovered state.
  bool is_hovered_;

  // Cache the font height as a minor optimization.
  int font_height_;

  AutocompleteMatch match_;

  gfx::Rect answer_icon_bounds_;
  gfx::Rect icon_bounds_;

  std::unique_ptr<views::ImageView> keyword_icon_;
  std::unique_ptr<OmniboxTabSwitchButton> tab_switch_button_;

  std::unique_ptr<gfx::SlideAnimation> animation_;

  // If the answer has an icon, cache the image.
  gfx::ImageSkia answer_image_;

  // Weak pointers for easy reference.
  OmniboxTextView* content_view_;
  OmniboxTextView* description_view_;
  OmniboxTextView* keyword_content_view_;
  OmniboxTextView* keyword_description_view_;
  OmniboxTextView* separator_view_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
