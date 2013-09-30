// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/textfield/textfield.h"

class BrowserView;
class FindBarController;
class FindNotificationDetails;

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarHost implements the container widget for the
// find-in-page functionality. It uses the appropriate implementation from
// find_bar_host_win.cc or find_bar_host_aura.cc to draw its content and is
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

  // Forwards selected key events to the renderer. This is useful to make sure
  // that arrow keys and PageUp and PageDown result in scrolling, instead of
  // being eaten because the FindBar has focus. Returns true if the keystroke
  // was forwarded, false if not.
  bool MaybeForwardKeyEventToWebpage(const ui::KeyEvent& key_event);

  // FindBar implementation:
  virtual FindBarController* GetFindBarController() const OVERRIDE;
  virtual void SetFindBarController(
      FindBarController* find_bar_controller) OVERRIDE;
  virtual void Show(bool animate) OVERRIDE;
  virtual void Hide(bool animate) OVERRIDE;
  virtual void SetFocusAndSelection() OVERRIDE;
  virtual void ClearResults(const FindNotificationDetails& results) OVERRIDE;
  virtual void StopAnimation() OVERRIDE;
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw) OVERRIDE;
  virtual void SetFindTextAndSelectedRange(
      const string16& find_text,
      const gfx::Range& selected_range) OVERRIDE;
  virtual string16 GetFindText() OVERRIDE;
  virtual gfx::Range GetSelectedRange() OVERRIDE;
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text) OVERRIDE;
  virtual void AudibleAlert() OVERRIDE;
  virtual bool IsFindBarVisible() OVERRIDE;
  virtual void RestoreSavedFocus() OVERRIDE;
  virtual bool HasGlobalFindPasteboard() OVERRIDE;
  virtual void UpdateFindBarForChangedWebContents() OVERRIDE;
  virtual FindBarTesting* GetFindBarTesting() OVERRIDE;

  // Overridden from ui::AcceleratorTarget in DropdownBarHost class:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // FindBarTesting implementation:
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) OVERRIDE;
  virtual string16 GetFindSelectedText() OVERRIDE;
  virtual string16 GetMatchCountText() OVERRIDE;
  virtual int GetWidth() OVERRIDE;

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
  virtual gfx::Rect GetDialogPosition(
      gfx::Rect avoid_overlapping_rect) OVERRIDE;
  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  // It then calls UpdateWindowEdges to make sure we don't overwrite the Chrome
  // window border. If |no_redraw| is set, the window is getting moved but not
  // sized, and should not be redrawn to reduce update flicker.
  virtual void SetDialogPosition(const gfx::Rect& new_pos,
                                 bool no_redraw) OVERRIDE;

  // Retrieves the boundaries that the find bar widget has to work with
  // within the Chrome frame window. The resulting rectangle will be a
  // rectangle that overlaps the bottom of the Chrome toolbar by one
  // pixel (so we can create the illusion that the dropdown widget is
  // part of the toolbar) and covers the page area, except that we
  // deflate the rect width by subtracting (from both sides) the width
  // of the toolbar and some extra pixels to account for the width of
  // the Chrome window borders. |bounds| is relative to the browser
  // window. If the function fails to determine the browser
  // window/client area rectangle or the rectangle for the page area
  // then |bounds| will be an empty rectangle.
  virtual void GetWidgetBounds(gfx::Rect* bounds) OVERRIDE;

  // Additional accelerator handling (on top of what DropDownBarHost does).
  virtual void RegisterAccelerators() OVERRIDE;
  virtual void UnregisterAccelerators() OVERRIDE;

 protected:
  // Overridden from DropdownBarHost:
  virtual void OnVisibilityChanged() OVERRIDE;

 private:
  // Allows implementation to tweak widget position.
  void GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect);

  // Allows native implementation to prevent key events from being forwarded.
  bool ShouldForwardKeyEventToWebpageNative(const ui::KeyEvent& key_event);

  // Returns the FindBarView.
  FindBarView* find_bar_view() { return static_cast<FindBarView*>(view()); }

  // A pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  DISALLOW_COPY_AND_ASSIGN(FindBarHost);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
