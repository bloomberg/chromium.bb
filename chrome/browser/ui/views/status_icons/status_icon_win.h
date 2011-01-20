// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
#pragma once

#include <windows.h>
#include <shellapi.h>

#include "base/win/scoped_gdi_object.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/status_icons/status_icon.h"

namespace views {
class Menu2;
}

class StatusIconWin : public StatusIcon {
 public:
  // Constructor which provides this icon's unique ID and messaging window.
  StatusIconWin(UINT id, HWND window, UINT message);
  virtual ~StatusIconWin();

  // Overridden from StatusIcon:
  virtual void SetImage(const SkBitmap& image);
  virtual void SetPressedImage(const SkBitmap& image);
  virtual void SetToolTip(const string16& tool_tip);

  UINT icon_id() const { return icon_id_; }

  UINT message_id() const { return message_id_; }

  // Handles a click event from the user - if |left_button_click| is true and
  // there is a registered observer, passes the click event to the observer,
  // otherwise displays the context menu if there is one.
  void HandleClickEvent(int x, int y, bool left_button_click);

  // Re-creates the status tray icon now after the taskbar has been created.
  void ResetIcon();

 protected:
  // Overridden from StatusIcon.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* menu);

 private:
  void InitIconData(NOTIFYICONDATA* icon_data);

  // The unique ID corresponding to this icon.
  UINT icon_id_;

  // Window used for processing messages from this icon.
  HWND window_;

  // The message identifier used for status icon messages.
  UINT message_id_;

  // The currently-displayed icon for the window.
  base::win::ScopedHICON icon_;

  // Context menu associated with this icon (if any).
  scoped_ptr<views::Menu2> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_WIN_H_
