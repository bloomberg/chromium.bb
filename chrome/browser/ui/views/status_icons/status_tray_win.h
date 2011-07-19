// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
#pragma once

#include <windows.h>

#include "chrome/browser/status_icons/status_tray.h"

class StatusTrayWin : public StatusTray {
 public:
  StatusTrayWin();
  ~StatusTrayWin();

  // Exposed for testing.
  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);
 protected:
  // Overriden from StatusTray:
  virtual StatusIcon* CreatePlatformStatusIcon();

 private:
  // Static callback invoked when a message comes in to our messaging window.
  static LRESULT CALLBACK WndProcStatic(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam);

  // The unique icon ID we will assign to the next icon.
  UINT next_icon_id_;

  // The window used for processing events.
  HWND window_;

  // The message ID of the "TaskbarCreated" message, sent to us when we need to
  // reset our status icons.
  UINT taskbar_created_message_;

  DISALLOW_COPY_AND_ASSIGN(StatusTrayWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_

