// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_

#include <vector>
#include "base/memory/scoped_ptr.h"
#include "ui/base/win/hwnd_subclass.h"

class SkBitmap;

class TaskbarWindowThumbnailerDelegateWin {
 public:
  // Returns the list of handles for all windows that are used to construct the
  // thumbnail. If empty list is returned, the snapshot of current window
  // is used.
  virtual std::vector<HWND> GetSnapshotWindowHandles() const = 0;
};

// Provides the custom thumbnail and live preview for the window that appears
// in the taskbar (Windows 7 and later).
class TaskbarWindowThumbnailerWin : public ui::HWNDMessageFilter {
 public:
  TaskbarWindowThumbnailerWin(HWND hwnd,
                              TaskbarWindowThumbnailerDelegateWin* delegate);
  virtual ~TaskbarWindowThumbnailerWin();

  // Starts using the custom snapshot for live preview. The snapshot is only
  // captured once when the system requests it, so the updates of the panels'
  // content will not be automatically reflected in the thumbnail.
  void Start();

  // Stops providing the custom snapshot for live preview.
  void Stop();

  // Captures the snapshot now instead of when the system requests it.
  void CaptureSnapshot();

  // Invalidates the snapshot such that a fresh copy can be obtained next time
  // when the system requests it.
  void InvalidateSnapshot();

  // Provide the snapshot to the new window. If the snapshot is captured for the
  // old window, it will also be used for the new window.
  void ReplaceWindow(HWND new_hwnd);

 private:
  // Overridden from ui::HWNDMessageFilter:
  virtual bool FilterMessage(HWND hwnd,
                             UINT message,
                             WPARAM w_param,
                             LPARAM l_param,
                             LRESULT* l_result) OVERRIDE;

  // Message handlers.
  bool OnDwmSendIconicThumbnail(int width, int height, LRESULT* l_result);
  bool OnDwmSendIconicLivePreviewBitmap(LRESULT* l_result);

  // Captures and returns the screenshot of the window. The caller is
  // responsible to release the returned SkBitmap instance.
  SkBitmap* CaptureWindowImage() const;

  HWND hwnd_;
  TaskbarWindowThumbnailerDelegateWin* delegate_;  // Weak, owns us.
  scoped_ptr<SkBitmap> capture_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TaskbarWindowThumbnailerWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_
