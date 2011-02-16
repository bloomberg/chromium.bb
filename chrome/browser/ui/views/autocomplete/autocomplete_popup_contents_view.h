// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#pragma once

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "views/view.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_win.h"
#else
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_gtk.h"
#endif

class AutocompleteEditModel;
class AutocompleteEditViewWin;
struct AutocompleteMatch;
class BubbleBorder;
class Profile;

// An interface implemented by an object that provides data to populate
// individual result views.
class AutocompleteResultViewModel {
 public:
  // Returns true if the index is selected.
  virtual bool IsSelectedIndex(size_t index) const = 0;

  // Returns true if the index is hovered.
  virtual bool IsHoveredIndex(size_t index) const = 0;

  // Returns the special-case icon we should use for the given index, or NULL
  // if we should use the default icon.
  virtual const SkBitmap* GetSpecialIcon(size_t index) const = 0;
};

// A view representing the contents of the autocomplete popup.
class AutocompletePopupContentsView : public views::View,
                                      public AutocompleteResultViewModel,
                                      public AutocompletePopupView,
                                      public ui::AnimationDelegate {
 public:
  AutocompletePopupContentsView(const gfx::Font& font,
                                AutocompleteEditView* edit_view,
                                AutocompleteEditModel* edit_model,
                                Profile* profile,
                                const views::View* location_bar);
  virtual ~AutocompletePopupContentsView();

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual gfx::Rect GetTargetBounds();
  virtual void PaintUpdatesNow();
  virtual void OnDragCanceled();

  // Overridden from AutocompleteResultViewModel:
  virtual bool IsSelectedIndex(size_t index) const;
  virtual bool IsHoveredIndex(size_t index) const;
  virtual const SkBitmap* GetSpecialIcon(size_t index) const;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void PaintChildren(gfx::Canvas* canvas) {
    // We paint our children inside OnPaint().
  }
  virtual void Layout();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseMoved(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual views::View* GetViewForPoint(const gfx::Point& point);

 private:
#if defined(OS_WIN)
  typedef AutocompletePopupWin AutocompletePopupClass;
#else
  typedef AutocompletePopupGtk AutocompletePopupClass;
#endif
  class InstantOptInView;

  // Returns true if the model has a match at the specified index.
  bool HasMatchAt(size_t index) const;

  // Returns the match at the specified index within the popup model.
  const AutocompleteMatch& GetMatchAtIndex(size_t index) const;

  // Fill a path for the contents' roundrect. |bounding_rect| is the rect that
  // bounds the path.
  void MakeContentsPath(gfx::Path* path, const gfx::Rect& bounding_rect);

  // Updates the window's blur region for the current size.
  void UpdateBlurRegion();

  // Makes the contents of the canvas slightly transparent.
  void MakeCanvasTransparent(gfx::Canvas* canvas);

  // Called when the line at the specified index should be opened with the
  // provided disposition.
  void OpenIndex(size_t index, WindowOpenDisposition disposition);

  // Find the index of the match under the given |point|, specified in window
  // coordinates. Returns AutocompletePopupModel::kNoMatch if there isn't a
  // match at the specified point.
  size_t GetIndexForPoint(const gfx::Point& point);

  // Returns the target bounds given the specified content height.
  gfx::Rect CalculateTargetBounds(int h);

  // Invoked if the user clicks on one of the opt-in buttons. Removes the opt-in
  // view.
  void UserPressedOptIn(bool opt_in);

  // The popup that contains this view.  We create this, but it deletes itself
  // when its window is destroyed.  This is a WeakPtr because it's possible for
  // the OS to destroy the window and thus delete this object before we're
  // deleted, or without our knowledge.
  base::WeakPtr<AutocompletePopupClass> popup_;

  // The provider of our result set.
  scoped_ptr<AutocompletePopupModel> model_;

  // The edit view that invokes us.
  AutocompleteEditView* edit_view_;

  // An object that the popup positions itself against.
  const views::View* location_bar_;

  // Our border, which can compute our desired bounds.
  const BubbleBorder* bubble_border_;

  // The font that we should use for result rows. This is based on the font used
  // by the edit that created us.
  gfx::Font result_font_;

  // The font used for portions that match the input.
  gfx::Font result_bold_font_;

  // If the user cancels a dragging action (i.e. by pressing ESC), we don't have
  // a convenient way to release mouse capture. Instead we use this flag to
  // simply ignore all remaining drag events, and the eventual mouse release
  // event. Since OnDragCanceled() can be called when we're not dragging, this
  // flag is reset to false on a mouse pressed event, to make sure we don't
  // erroneously ignore the next drag.
  bool ignore_mouse_drag_;

  // The popup sizes vertically using an animation when the popup is getting
  // shorter (not larger, that makes it look "slow").
  ui::SlideAnimation size_animation_;
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  // If non-NULL the instant opt-in-view is visible.
  views::View* opt_in_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
