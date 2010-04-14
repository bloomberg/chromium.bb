// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/status_icons/status_icon_win.h"

#include "gfx/icon_util.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"

StatusIconWin::StatusIconWin(UINT id, HWND window, UINT message)
    : icon_id_(id),
      window_(window),
      message_id_(message) {
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_MESSAGE;
  icon_data.uCallbackMessage = message_id_;
  BOOL result = Shell_NotifyIcon(NIM_ADD, &icon_data);
  DCHECK(result);
}

StatusIconWin::~StatusIconWin() {
  // Remove our icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  Shell_NotifyIcon(NIM_DELETE, &icon_data);
}

void StatusIconWin::SetImage(const SkBitmap& image) {
  // Create the icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_ICON;
  icon_.Set(IconUtil::CreateHICONFromSkBitmap(image));
  icon_data.hIcon = icon_.Get();
  BOOL result = Shell_NotifyIcon(NIM_MODIFY, &icon_data);
  DCHECK(result);
}

void StatusIconWin::SetPressedImage(const SkBitmap& image) {
  // Ignore pressed images, since the standard on Windows is to not highlight
  // pressed status icons.
}

void StatusIconWin::SetToolTip(const string16& tool_tip) {
  // Create the icon.
  NOTIFYICONDATA icon_data;
  InitIconData(&icon_data);
  icon_data.uFlags = NIF_TIP;
  wcscpy_s(icon_data.szTip, tool_tip.c_str());
  BOOL result = Shell_NotifyIcon(NIM_MODIFY, &icon_data);
  DCHECK(result);
}

void StatusIconWin::InitIconData(NOTIFYICONDATA* icon_data) {
  icon_data->cbSize = sizeof(icon_data);
  icon_data->hWnd = window_;
  icon_data->uID = icon_id_;
}
