// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DROPDOWN_BAR_HOST_H_
#define CHROME_BROWSER_VIEWS_DROPDOWN_BAR_HOST_H_

#include "app/animation.h"
#include "app/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"

class BrowserView;
class DropdownBarView;
class SlideAnimation;
class TabContents;

namespace views {
class ExternalFocusTracker;
class View;
class Widget;
}

////////////////////////////////////////////////////////////////////////////////
//
// The DropdownBarHost implements the container widget for the UI that
// is shown at the top of browser contents. It uses the appropriate
// implementation from dropdown_bar_host_win.cc or dropdown_bar_host_gtk.cc to
// draw its content and is responsible for showing, hiding, animating, closing,
// and moving the bar if needed, for example if the widget is
// obscuring the selection results in FindBar.
//
////////////////////////////////////////////////////////////////////////////////
class DropdownBarHost : public views::AcceleratorTarget,
                        public views::FocusChangeListener,
                        public AnimationDelegate {
 public:
  explicit DropdownBarHost(BrowserView* browser_view);
  virtual ~DropdownBarHost();

  // Initializes the dropdown bar host with the give view.
  void Init(DropdownBarView* view);

  // Whether we are animating the position of the dropdown widget.
  bool IsAnimating() const;
  // Returns true if the dropdown bar view is visible, or false otherwise.
  bool IsVisible() const;
  // Shows the dropdown bar.
  void Show(bool animate);
  // Hides the dropdown bar.
  void Hide(bool animate);
  // Selects text in the entry field and set focus.
  void SetFocusAndSelection();
  // Stops the animation.
  void StopAnimation();

  // Returns the rectangle representing where to position the dropdown widget.
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect) = 0;

  // Moves the widget to the provided location, moves it to top
  // in the z-order (HWND_TOP, not HWND_TOPMOST for windows) and shows
  // the widget (if hidden).
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) = 0;

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // Overridden from views::AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator) = 0;

  // AnimationDelegate implementation:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Get the offset with which to paint the theme image.
  void GetThemePosition(gfx::Rect* bounds);

  // During testing we can disable animations by setting this flag to true,
  // so that opening and closing the dropdown bar is shown instantly, instead of
  // having to poll it while it animates to open/closed status.
  static bool disable_animations_during_testing_;

  // Returns the browser view that the dropdown belongs to.
  BrowserView* browser_view() const { return browser_view_; }

 protected:
  // Returns the dropdown bar view.
  DropdownBarView* view() const { return view_; }

  // Returns the focus tracker.
  views::ExternalFocusTracker* focus_tracker() const {
    return focus_tracker_.get();
  }

  // Resets the focus tracker.
  void ResetFocusTracker();

  // The focus manager we register with to keep track of focus changes.
  views::FocusManager* focus_manager() const { return focus_manager_; }

  // Returns the host widget.
  views::Widget* host() const { return host_.get(); }

  // Returns the animation offset.
  int animation_offset() const { return animation_offset_; }

  // Retrieves the boundaries that the dropdown widget has to work with
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
  void GetWidgetBounds(gfx::Rect* bounds);

  // Registers this class as the handler for when Escape is pressed. We will
  // unregister once we loose focus. See also: SetFocusChangeListener().
  void RegisterEscAccelerator();

  // When we loose focus, we unregister the handler for Escape. See
  // also: SetFocusChangeListener().
  void UnregisterEscAccelerator();

  // Creates and returns the native Widget.
  views::Widget* CreateHost();

  // Allows implementation to tweak widget position.
  void SetWidgetPositionNative(const gfx::Rect& new_pos, bool no_redraw);

  // Returns the native view (is a child of the window widget in gtk).
  gfx::NativeView GetNativeView(BrowserView* browser_view);

  // Returns a keyboard event suitable for fowarding.
  NativeWebKeyboardEvent GetKeyboardEvent(
      const TabContents* contents,
      const views::Textfield::Keystroke& key_stroke);

  // Returns the animation for the dropdown.
  SlideAnimation* animation() {
    return animation_.get();
  }

 private:
  // The BrowserView that created us.
  BrowserView* browser_view_;

  // Our view, which is responsible for drawing the UI.
  DropdownBarView* view_;

  // The y position pixel offset of the widget while animating the
  // dropdown widget.
  int animation_offset_;

  // The animation class to use when opening the Dropdown widget.
  scoped_ptr<SlideAnimation> animation_;

  // The focus manager we register with to keep track of focus changes.
  views::FocusManager* focus_manager_;

  // True if the accelerator target for Esc key is registered.
  bool esc_accel_target_registered_;

  // Tracks and stores the last focused view which is not the DropdownBarView
  // or any of its children. Used to restore focus once the DropdownBarView is
  // closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // Host is the Widget implementation that is created and maintained by the
  // dropdown bar. It contains the DropdownBarView.
  scoped_ptr<views::Widget> host_;

  // A flag to manually manage visibility. GTK/X11 is asynchrnous and
  // the state of the widget can be out of sync.
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(DropdownBarHost);
};

#endif  // CHROME_BROWSER_VIEWS_DROPDOWN_BAR_HOST_H_
