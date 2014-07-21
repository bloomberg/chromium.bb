// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

struct AutocompleteMatch;
class LocationBarView;
class OmniboxEditModel;
class OmniboxResultView;
class OmniboxView;
class Profile;

// A view representing the contents of the autocomplete popup.
class OmniboxPopupContentsView : public views::View,
                                 public OmniboxPopupView,
                                 public views::ViewTargeterDelegate,
                                 public gfx::AnimationDelegate {
 public:
  // Factory method for creating the AutocompletePopupView.
  static OmniboxPopupView* Create(const gfx::FontList& font_list,
                                  OmniboxView* omnibox_view,
                                  OmniboxEditModel* edit_model,
                                  LocationBarView* location_bar_view);

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  virtual void LayoutChildren();

  // Overridden from OmniboxPopupView:
  virtual bool IsOpen() const OVERRIDE;
  virtual void InvalidateLine(size_t line) OVERRIDE;
  virtual void UpdatePopupAppearance() OVERRIDE;
  virtual gfx::Rect GetTargetBounds() OVERRIDE;
  virtual void PaintUpdatesNow() OVERRIDE;
  virtual void OnDragCanceled() OVERRIDE;

  // Overridden from gfx::AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual views::View* GetTooltipHandlerForPoint(
      const gfx::Point& point) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  bool IsSelectedIndex(size_t index) const;
  bool IsHoveredIndex(size_t index) const;
  gfx::Image GetIconIfExtensionMatch(size_t index) const;

  int max_match_contents_width() const {
    return max_match_contents_width_;
  }

 protected:
  OmniboxPopupContentsView(const gfx::FontList& font_list,
                           OmniboxView* omnibox_view,
                           OmniboxEditModel* edit_model,
                           LocationBarView* location_bar_view);
  virtual ~OmniboxPopupContentsView();

  LocationBarView* location_bar_view() { return location_bar_view_; }

  virtual void PaintResultViews(gfx::Canvas* canvas);

  // Calculates the height needed to show all the results in the model.
  virtual int CalculatePopupHeight();
  virtual OmniboxResultView* CreateResultView(int model_index,
                                              const gfx::FontList& font_list);

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  // This method should not be triggered directly as we paint our children
  // in an un-conventional way inside OnPaint. We use a separate canvas to
  // paint the children. Hence we override this method to a no-op so that
  // the view hierarchy does not "accidentally" trigger this.
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;

  scoped_ptr<OmniboxPopupModel> model_;

 private:
  class AutocompletePopupWidget;

  // views::ViewTargeterDelegate:
  virtual views::View* TargetForRect(views::View* root,
                                     const gfx::Rect& rect) OVERRIDE;

  // Call immediately after construction.
  void Init();

  // Returns true if the model has a match at the specified index.
  bool HasMatchAt(size_t index) const;

  // Returns the match at the specified index within the popup model.
  const AutocompleteMatch& GetMatchAtIndex(size_t index) const;

  // Fill a path for the contents' roundrect. |bounding_rect| is the rect that
  // bounds the path.
  void MakeContentsPath(gfx::Path* path, const gfx::Rect& bounding_rect);

  // Find the index of the match under the given |point|, specified in window
  // coordinates. Returns OmniboxPopupModel::kNoMatch if there isn't a match at
  // the specified point.
  size_t GetIndexForPoint(const gfx::Point& point);

  // Processes a located event (e.g. mouse/gesture) and sets the selection/hover
  // state of a line in the list.
  void UpdateLineEvent(const ui::LocatedEvent& event,
                       bool should_set_selected_line);

  // Opens an entry from the list depending on the event and the selected
  // disposition.
  void OpenSelectedLine(const ui::LocatedEvent& event,
                        WindowOpenDisposition disposition);

  OmniboxResultView* result_view_at(size_t i);

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

  // If the user cancels a dragging action (i.e. by pressing ESC), we don't have
  // a convenient way to release mouse capture. Instead we use this flag to
  // simply ignore all remaining drag events, and the eventual mouse release
  // event. Since OnDragCanceled() can be called when we're not dragging, this
  // flag is reset to false on a mouse pressed event, to make sure we don't
  // erroneously ignore the next drag.
  bool ignore_mouse_drag_;

  // The popup sizes vertically using an animation when the popup is getting
  // shorter (not larger, that makes it look "slow").
  gfx::SlideAnimation size_animation_;
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  int left_margin_;
  int right_margin_;

  const gfx::ImageSkia* bottom_shadow_;  // Ptr owned by resource bundle.

  // Amount of extra padding to add to the popup on the top and bottom.
  int outside_vertical_padding_;

  // When the dropdown is not wide enough while displaying postfix suggestions,
  // we use the width of widest match contents to shift the suggestions so that
  // the widest suggestion just reaches the end edge.
  int max_match_contents_width_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENTS_VIEW_H_
