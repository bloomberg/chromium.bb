// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_BASE_H_
#define CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_BASE_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"

class AvatarMenuButtonGtk;

namespace content {
class WebContents;
}

class BrowserTitlebarBase {
 public:
  virtual ~BrowserTitlebarBase() { }

  // Build the titlebar, the space above the tab strip, and (maybe) the min,
  // max, close buttons.
  virtual void Init() = 0;

  // Updates the title and icon when in app or popup/panel mode (no tabstrip).
  virtual void UpdateTitleAndIcon() = 0;

  // Update the appearance of the title bar based on whether we're showing a
  // custom frame or not.  If |use_custom_frame| is true, we show an extra
  // tall titlebar and the min/max/close buttons.
  virtual void UpdateCustomFrame(bool use_custom_frame) = 0;

  // Called by the browser asking us to update the loading throbber.
  // |web_contents| is the tab that is associated with the window throbber.
  // |web_contents| can be null.
  virtual void UpdateThrobber(content::WebContents* web_contents) = 0;

  // On Windows, right clicking in the titlebar background brings up the system
  // menu.  There's no such thing on linux, so we just show the menu items we
  // add to the menu.
  virtual void ShowContextMenu(GdkEventButton* event) = 0;

  virtual GtkWidget* widget() const = 0;
  virtual void set_window(GtkWindow* window) = 0;
  virtual AvatarMenuButtonGtk* avatar_button() const = 0;
};

#endif  // CHROME_BROWSER_UI_GTK_BROWSER_TITLEBAR_BASE_H_
