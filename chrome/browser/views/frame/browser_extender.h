// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class BrowserView;
class Tab;

namespace views {
class Window;
}  // namespace views


// BrowserExtender adds chromeos specific features to BrowserView.
// The factory method |Create(BrowserView*)| creates different types
// of extender depending on the type of BrowserView and target platform.
// Please see browser/chromeos/browser_extenders.cc for ChromeOS extenders, and
// standard_extender.cc for Chrome browser.
class BrowserExtender {
 public:
  // Factory method to create a BrowserExtender for given
  // BrowserView object. Please see the class description for details.
  static BrowserExtender* Create(BrowserView* browser_view);

  virtual ~BrowserExtender() {}

  // Initializes the extender.
  virtual void Init() = 0;

  // Layouts controls within the given bounds. The |tabstrip_bounds| will be
  // filled with the remaining bounds for tabstip to be layed out and
  // the |bottom| will be filled with the y location where toolbar should be
  // layed out, in BrowserView cooridnates.
  virtual void Layout(const gfx::Rect& bounds,
                      gfx::Rect* tabstrip_bounds,
                      int* bottom) = 0;

  // Tests if the given |point|, which is given in BrowserView coordinates,
  // hits any of controls.
  virtual bool NonClientHitTest(const gfx::Point& browser_view_point) = 0;

  // Returns true to hide the toolbar for the window, or false
  // to use the regular logic to decide.
  virtual bool ShouldForceHideToolbar() = 0;

  // Returns true if the window should be in the maximized state.
  virtual bool ShouldForceMaximizedWindow() = 0;

  // Returns true if the compact navigation bar is focusable and got
  // focus, false otherwise.
  virtual bool SetFocusToCompactNavigationBar() = 0;

  // Toggles the visibility of CompactNavigationBar.
  virtual void ToggleCompactNavigationBar() = 0;

  // Called when a mouse entered into the |tab|.
  virtual void OnMouseEnteredToTab(Tab* tab) = 0;

  // Called when a mouse moved (hovered) on the |tab|.
  virtual void OnMouseMovedOnTab(Tab* tab) = 0;

  // Called when a mouse exited from the |tab|.
  virtual void OnMouseExitedFromTab(Tab* tab) = 0;

  // Returns the main menu's width.  This is used in the opaque frame
  // to layout otr icons and tabstrips.
  virtual int GetMainMenuWidth() const = 0;

  // Tells if the browser can be closed.
  bool can_close() const {
    return can_close_;
  }

  // Specifies if the browser can be closed or not. This typically set
  // to false when the browser is being dragged.
  void set_can_close(bool b) {
    can_close_ = b;
  }

 protected:
  explicit BrowserExtender(BrowserView* browser_view);

  // Returns the view Window object that contains the BrowserView.
  views::Window* GetBrowserWindow();

  BrowserView* browser_view() {
    return browser_view_;
  }

 private:
  // BrowserView to be extended.
  BrowserView* browser_view_;

  // True if the browser can be closed. See set_can_close method for setails.
  bool can_close_;

  DISALLOW_COPY_AND_ASSIGN(BrowserExtender);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_EXTENDER_H_
