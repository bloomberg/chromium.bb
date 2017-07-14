// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class NewTabPromo;

///////////////////////////////////////////////////////////////////////////////
// NewTabButton
//
//  A subclass of ImageButton that hit-tests to the shape of the new tab button
//  and does custom drawing.
//
///////////////////////////////////////////////////////////////////////////////
class NewTabButton : public views::ImageButton,
                     public views::MaskedTargeterDelegate,
                     public views::WidgetObserver {
 public:
  NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener);
  ~NewTabButton() override;

  // Set the background offset used to match the background image to the frame
  // image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Returns the offset from the top of the tabstrip at which the new tab
  // button's visible region begins.
  static int GetTopOffset();

  // Shows the NewTabPromo when the NewTabFeatureEngagementTracker calls for it.
  void ShowPromo();

 private:
// views::ImageButton:
#if defined(OS_WIN)
  void OnMouseReleased(const ui::MouseEvent& event) override;
#endif
  void OnGestureEvent(ui::GestureEvent* event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(gfx::Path* mask) const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns the gfx::Rect around the visible portion of the New Tab Button.
  // Note: This is different than the rect around the entire New Tab Button as
  // it extends to the top of the tabstrip for Fitts' Law interaction in a
  // maximized window. Used for anchoring the NewTabPromo.
  gfx::Rect GetVisibleBounds();

  // Computes a path corresponding to the button's outer border for a given
  // |scale| and stores it in |path|.  |button_y| is used as the y-coordinate
  // for the top of the button.  If |extend_to_top| is true, the path is
  // extended vertically to y = 0.  The caller uses this for Fitts' Law purposes
  // in maximized/fullscreen mode.
  void GetBorderPath(float button_y,
                     float scale,
                     bool extend_to_top,
                     SkPath* path) const;

  // Paints the fill region of the button into |canvas|, according to the
  // supplied values from GetImage() and the given |fill| path.
  void PaintFill(bool pressed,
                 float scale,
                 const SkPath& fill,
                 gfx::Canvas* canvas) const;

  // Tab strip that contains this button.
  TabStrip* tab_strip_;

  // The offset used to paint the background image.
  gfx::Point background_offset_;

  // were we destroyed?
  bool* destroyed_;

  // Observes the NewTabPromo's Widget.  Used to tell whether the promo is
  // open and get called back when it closes.
  ScopedObserver<views::Widget, WidgetObserver> new_tab_promo_observer_;

  DISALLOW_COPY_AND_ASSIGN(NewTabButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
