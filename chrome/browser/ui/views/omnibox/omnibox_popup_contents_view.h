// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"

struct AutocompleteMatch;
class LocationBarView;
class OmniboxEditModel;
class OmniboxResultView;
class OmniboxView;

namespace ui {
struct AXNodeData;
}

// A view representing the contents of the autocomplete popup.
class OmniboxPopupContentsView : public views::View,
                                 public OmniboxPopupView,
                                 public gfx::AnimationDelegate {
 public:
  OmniboxPopupContentsView(const gfx::FontList& font_list,
                           OmniboxView* omnibox_view,
                           OmniboxEditModel* edit_model,
                           LocationBarView* location_bar_view);
  ~OmniboxPopupContentsView() override;

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  // Opens a match from the list specified by |index| with the type of tab or
  // window specified by |disposition|.
  void OpenMatch(size_t index, WindowOpenDisposition disposition);

  // Returns the icon that should be displayed next to |match|. If the icon is
  // available as a vector icon, it will be |vector_icon_color|.
  gfx::Image GetMatchIcon(const AutocompleteMatch& match,
                          SkColor vector_icon_color) const;

  // Sets the line specified by |index| as selected.
  virtual void SetSelectedLine(size_t index);

  // Returns true if the line specified by |index| is selected.
  virtual bool IsSelectedIndex(size_t index) const;

  // OmniboxPopupView:
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override;
  void OnLineSelected(size_t line) override;
  void UpdatePopupAppearance() override;
  void OnMatchIconUpdated(size_t match_index) override;
  gfx::Rect GetTargetBounds() override;
  void PaintUpdatesNow() override;
  void OnDragCanceled() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::View:
  void Layout() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData*) override;

 private:
  class AutocompletePopupWidget;

  // Calculates the height needed to show all the results in the model.
  int CalculatePopupHeight();

  // Size our children to the available content area.
  void LayoutChildren();

  // Returns true if the model has a match at the specified index.
  bool HasMatchAt(size_t index) const;

  // Returns the match at the specified index within the popup model.
  const AutocompleteMatch& GetMatchAtIndex(size_t index) const;

  // Find the index of the match under the given |point|, specified in window
  // coordinates. Returns OmniboxPopupModel::kNoMatch if there isn't a match at
  // the specified point.
  size_t GetIndexForPoint(const gfx::Point& point);

  OmniboxResultView* result_view_at(size_t i);

  LocationBarView* location_bar_view() { return location_bar_view_; }

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;

  std::unique_ptr<OmniboxPopupModel> model_;

  // The popup that contains this view.  We create this, but it deletes itself
  // when its window is destroyed.  This is a WeakPtr because it's possible for
  // the OS to destroy the window and thus delete this object before we're
  // deleted, or without our knowledge.
  base::WeakPtr<AutocompletePopupWidget> popup_;

  // The edit view that invokes us.
  OmniboxView* omnibox_view_;

  LocationBarView* location_bar_view_;

  // The font list used for result rows, based on the omnibox font list.
  gfx::FontList font_list_;

  // The popup sizes vertically using an animation when the popup is getting
  // shorter (not larger, that makes it look "slow").
  gfx::SlideAnimation size_animation_;
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  int start_margin_;
  int end_margin_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_
