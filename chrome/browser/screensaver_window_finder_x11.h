// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREENSAVER_WINDOW_FINDER_X11_H_
#define CHROME_BROWSER_SCREENSAVER_WINDOW_FINDER_X11_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/x/x11_util.h"

class ScreensaverWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  static bool ScreensaverWindowExists();

 protected:
  virtual bool ShouldStopIterating(XID window) OVERRIDE;

 private:
  ScreensaverWindowFinder();

  bool IsScreensaverWindow(XID window) const;

  bool exists_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverWindowFinder);
};


#endif  // CHROME_BROWSER_SCREENSAVER_WINDOW_FINDER_X11_H_
