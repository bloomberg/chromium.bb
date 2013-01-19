// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_

#include <vector>
#include "base/memory/scoped_ptr.h"
#include "ui/base/win/hwnd_subclass.h"

class SkBitmap;

// Provides the custom thumbnail and live preview for the window that appears
// in the taskbar (Windows 7 and later).
class TaskbarWindowThumbnailerWin : public ui::HWNDMessageFilter {
 public:
  explicit TaskbarWindowThumbnailerWin(HWND hwnd);
  virtual ~TaskbarWindowThumbnailerWin();

  // Use the snapshots from all the windows in |snapshot_hwnds| to construct
  // the thumbnail. If |snapshot_hwnds| is empty, use the snapshot of current
  // window.
  void Start(const std::vector<HWND>& snapshot_hwnds);
  void Stop();

 private:
  // Overridden from ui::HWNDMessageFilter:
  virtual bool FilterMessage(HWND hwnd,
                             UINT message,
                             WPARAM w_param,
                             LPARAM l_param,
                             LRESULT* l_result) OVERRIDE;

  // Message handlers.
  bool OnDwmSendIconicThumbnail(
      int width, int height, LRESULT* l_result);
  bool OnDwmSendIconicLivePreviewBitmap(LRESULT* l_result);

  // Captures and returns the screenshot of the window. The caller is
  // responsible to release the returned SkBitmap instance.
  SkBitmap* CaptureWindowImage() const;

  HWND hwnd_;
  std::vector<HWND> snapshot_hwnds_;
  scoped_ptr<SkBitmap> capture_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TaskbarWindowThumbnailerWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_TASKBAR_WINDOW_THUMBNAILER_WIN_H_
