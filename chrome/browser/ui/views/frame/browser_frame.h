// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
#pragma once

#include "build/build_config.h"
#include "views/window/non_client_view.h"

class BaseTabStrip;
class BrowserView;
class Profile;

namespace gfx {
class Font;
class Rect;
}  // namespace gfx

namespace ui {
class ThemeProvider;
}

namespace views {
class Window;

#if defined(OS_WIN)
class WindowWin;
#endif
}  // namespace views

// This is a virtual interface that allows system specific browser frames.
class BrowserFrame {
 public:
  virtual ~BrowserFrame() {}

  // Creates the appropriate BrowserFrame for this platform. The returned
  // object is owned by the caller.
  static BrowserFrame* Create(BrowserView* browser_view, Profile* profile);

  static const gfx::Font& GetTitleFont();

  // Returns the Window associated with this frame. Guraranteed non-NULL after
  // construction.
  virtual views::Window* GetWindow() = 0;

  // Determine the distance of the left edge of the minimize button from the
  // left edge of the window. Used in our Non-Client View's Layout.
  virtual int GetMinimizeButtonOffset() const = 0;

  // Retrieves the bounds, in non-client view coordinates for the specified
  // TabStrip.
  virtual gfx::Rect GetBoundsForTabStrip(BaseTabStrip* tabstrip) const = 0;

  // Returns the y coordinate within the window at which the horizontal TabStrip
  // begins (or would begin).  If |restored| is true, this is calculated as if
  // we were in restored mode regardless of the current mode.
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const = 0;

  // Tells the frame to update the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Returns the theme provider for this frame.
  virtual ui::ThemeProvider* GetThemeProviderForFrame() const = 0;

  // Returns true if the window should use the native frame view. This is true
  // if there are no themes applied on Vista, or if there are themes applied and
  // this browser window is an app or popup.
  virtual bool AlwaysUseNativeFrame() const = 0;

  // Returns the NonClientFrameView of this frame.
  virtual views::View* GetFrameView() const = 0;

  // Notifies the frame that the tab strip display mode changed so it can update
  // its frame treatment if necessary.
  virtual void TabStripDisplayModeChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
