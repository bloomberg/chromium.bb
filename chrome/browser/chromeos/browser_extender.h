// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class BrowserView;

namespace views {
class Window;
}  // namespace views


// BrowserExtender adds chromeos specific features to BrowserView.
// The factory method |Create(BrowserView*)| returns two different types
// of extender depending on the type of BrowserView. If the browser_view
// is normal browser, it returns NormalExtender that adds controls to
// the title bar as follows:
//                  ____  __ __
//      [MainMenu] /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
//
// If it is popup window, it returns PopupExtender that creates
// a title window.
class BrowserExtender {
 public:
  // Factory method to create a BrowserExtender for given
  // BrowserView object. Please see the class description for details.
  static BrowserExtender* Create(BrowserView* browser_view);

  virtual ~BrowserExtender() {}

  // Initializes the extender.
  virtual void Init() = 0;

  // Layouts controls within the given bounds and returns the remaining
  // bounds for tabstip to be layed out.
  virtual gfx::Rect Layout(const gfx::Rect& bounds) { return bounds; }

  // Tests if the given |point|, which is given in BrowserView coordinates,
  // hits any of controls.
  virtual bool NonClientHitTest(const gfx::Point& browser_view_point) = 0;

  // Updates the title bar (if any).
  virtual void UpdateTitleBar() {}

  // Called when the BrowserView is shown.
  virtual void Show() = 0;

  // Called when the BrowserView is closed.
  virtual void Close() {}

  // Called when the browser window is either activated or deactivated.
  virtual void ActivationChanged() {}

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

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_
