// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"

typedef LRESULT (*VolumeNameFunc)(LPCWSTR drive,
                                  LPWSTR volume_name,
                                  unsigned int volume_name_len);
namespace chrome {

class MediaDeviceNotificationsWindowWin {
 public:
  MediaDeviceNotificationsWindowWin();
  // Only for use in unit tests.
  explicit MediaDeviceNotificationsWindowWin(VolumeNameFunc volumeNameFunc);

  virtual ~MediaDeviceNotificationsWindowWin();

  virtual LRESULT OnDeviceChange(UINT event_type, DWORD data);

 private:
  void Init();

  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

  static LRESULT CALLBACK WndProcThunk(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam);

  // The window class of |window_|.
  ATOM atom_;

  // The handle of the module that contains the window procedure of |window_|.
  HMODULE instance_;

  HWND window_;
  VolumeNameFunc volume_name_func_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceNotificationsWindowWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
