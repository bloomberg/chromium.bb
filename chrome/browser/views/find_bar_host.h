// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIND_BAR_HOST_H_
#define CHROME_BROWSER_VIEWS_FIND_BAR_HOST_H_

#include "app/animation.h"
#include "app/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/views/dropdown_bar_host.h"
#include "views/controls/textfield/textfield.h"

class BrowserView;
class FindBarController;
class FindBarView;
class FindNotificationDetails;

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarHost implements the container widget for the
// find-in-page functionality. It uses the appropriate implementation from
// find_bar_host_win.cc or find_bar_host_gtk.cc to draw its content and is
// responsible for showing, hiding, closing, and moving the widget if needed,
// for example if the widget is obscuring the selection results. It also
// receives notifications about the search results and communicates that to
// the view.
//
// There is one FindBarHost per BrowserView, and its state is updated
// whenever the selected Tab is changed. The FindBarHost is created when
// the BrowserView is attached to the frame's Widget for the first time.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarHost : public DropdownBarHost,
                    public FindBar,
                    public FindBarTesting {
 public:
  explicit FindBarHost(BrowserView* browser_view);
  virtual ~FindBarHost();

  // Forwards selected keystrokes to the renderer. This is useful to make sure
  // that arrow keys and PageUp and PageDown result in scrolling, instead of
  // being eaten because the FindBar has focus. Returns true if the keystroke
  // was forwarded, false if not.
  bool MaybeForwardKeystrokeToWebpage(
      const views::Textfield::Keystroke& key_stroke);

  // FindBar implementation:
  virtual FindBarController* GetFindBarController() const {
    return find_bar_controller_;
  }
  virtual void SetFindBarController(FindBarController* find_bar_controller) {
    find_bar_controller_ = find_bar_controller;
  }
  virtual void Show(bool animate);
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);
  virtual void SetFindText(const string16& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text);
  virtual void AudibleAlert();
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();
  virtual FindBarTesting* GetFindBarTesting();

  // Overridden from views::AcceleratorTarget in DropdownBarHost class:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // FindBarTesting implementation:
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible);

  // Overridden from DropdownBarHost:
  // Returns the rectangle representing where to position the find bar. It uses
  // GetDialogBounds and positions itself within that, either to the left (if an
  // InfoBar is present) or to the right (no InfoBar). If
  // |avoid_overlapping_rect| is specified, the return value will be a rectangle
  // located immediately to the left of |avoid_overlapping_rect|, as long as
  // there is enough room for the dialog to draw within the bounds. If not, the
  // dialog position returned will overlap |avoid_overlapping_rect|.
  // Note: |avoid_overlapping_rect| is expected to use coordinates relative to
  // the top of the page area, (it will be converted to coordinates relative to
  // the top of the browser window, when comparing against the dialog
  // coordinates). The returned value is relative to the browser window.
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  // It then calls UpdateWindowEdges to make sure we don't overwrite the Chrome
  // window border. If |no_redraw| is set, the window is getting moved but not
  // sized, and should not be redrawn to reduce update flicker.
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);

 private:
  // The find bar widget needs rounded edges, so we create a polygon
  // that corresponds to the background images for this window (and
  // make the polygon only contain the pixels that we want to
  // draw). The polygon is then given to SetWindowRgn which changes
  // the window from being a rectangle in shape, to being a rect with
  // curved edges. We also check to see if the region should be
  // truncated to prevent from drawing onto Chrome's window border.
  void UpdateWindowEdges(const gfx::Rect& new_pos);

  // Allows implementation to tweak widget position.
  void GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect);

  // Allows native implementation to prevent keystrokes from being forwarded.
  bool ShouldForwardKeystrokeToWebpageNative(
      const views::Textfield::Keystroke& key_stroke);

  // Returns the FindBarView.
  FindBarView* find_bar_view();

  // A pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  DISALLOW_COPY_AND_ASSIGN(FindBarHost);
};

#endif  // CHROME_BROWSER_VIEWS_FIND_BAR_HOST_H_
