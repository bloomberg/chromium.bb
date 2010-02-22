// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_CHROMEOS_H_

#include "chrome/browser/views/frame/browser_frame_gtk.h"

namespace chromeos {

class BrowserFrameChromeos : public BrowserFrameGtk {
 public:
  BrowserFrameChromeos(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameChromeos();

  // BrowserFrameGtk overrides.
  virtual void Init();

  // views::WindowGtk overrides.
  virtual bool IsMaximized() const;

 private:
  // Returns true if the browser instance is for panel/popup window.
  bool IsPanel() const;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameChromeos);
};

}  // namespace chroemos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_CHROMEOS_H_
