// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_
#define CHROME_BROWSER_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "chrome/browser/status_icons/status_icon.h"

class SkBitmap;

class StatusIconGtk : public StatusIcon {
 public:
  StatusIconGtk();
  virtual ~StatusIconGtk();

  // Overridden from StatusIcon:
  virtual void SetImage(const SkBitmap& image);
  virtual void SetPressedImage(const SkBitmap& image);
  virtual void SetToolTip(const string16& tool_tip);

  // Exposed for testing.
  CHROMEGTK_CALLBACK_0(StatusIconGtk, void, OnClick);

 private:
  // The currently-displayed icon for the window.
  GtkStatusIcon* icon_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconGtk);
};

#endif  // CHROME_BROWSER_GTK_STATUS_ICONS_STATUS_ICON_GTK_H_
