// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_

#include "app/gfx/font.h"
#include "app/slide_animation.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "views/view.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"
#else
#include "chrome/browser/views/autocomplete/autocomplete_popup_gtk.h"
#endif

class AutocompleteEditModel;
class AutocompleteEditViewWin;
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

  // Called when the line at the specified index should be opened with the
  // provided disposition.
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition) = 0;

  // Called when the line at the specified index should be shown as hovered.
  virtual void SetHoveredLine(size_t index) = 0;

  // Called when the line at the specified index should be shown as selected.
  virtual void SetSelectedLine(size_t index, bool revert_to_default) = 0;
};

// A view representing the contents of the autocomplete popup.
class AutocompletePopupContentsView : public views::View,
                                      public AutocompleteResultViewModel,
                                      public AutocompletePopupView,
                                      public AnimationDelegate {
 public:
  AutocompletePopupContentsView(const gfx::Font& font,
                                AutocompleteEditView* edit_view,
                                AutocompleteEditModel* edit_model,
                                Profile* profile,
                                const BubblePositioner* bubble_positioner);
  virtual ~AutocompletePopupContentsView() {}

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

  // Overridden from AutocompleteResultViewModel:
  virtual bool IsSelectedIndex(size_t index) const;
  virtual bool IsHoveredIndex(size_t index) const;
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition);
  virtual void SetHoveredLine(size_t index);
  virtual void SetSelectedLine(size_t index, bool revert_to_default);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void PaintChildren(gfx::Canvas* canvas) {
    // We paint our children inside Paint().
  }
  virtual void Layout();

 private:
#if defined(OS_WIN)
  typedef AutocompletePopupWin AutocompletePopupClass;
#else
  typedef AutocompletePopupGtk AutocompletePopupClass;
#endif

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

  // The popup that contains this view.
  scoped_ptr<AutocompletePopupClass> popup_;

  // The provider of our result set.
  scoped_ptr<AutocompletePopupModel> model_;

  // The edit view that invokes us.
  AutocompleteEditView* edit_view_;

  // An object that tells the popup how to position itself.
  const BubblePositioner* bubble_positioner_;

  // Our border, which can compute our desired bounds.
  const BubbleBorder* bubble_border_;

  // The font that we should use for result rows. This is based on the font used
  // by the edit that created us.
  gfx::Font result_font_;

  // The popup sizes vertically using an animation when the popup is getting
  // shorter (not larger, that makes it look "slow").
  SlideAnimation size_animation_;
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupContentsView);
};

#endif  // CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
