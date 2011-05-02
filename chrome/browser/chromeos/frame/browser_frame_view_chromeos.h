// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_VIEW_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_VIEW_CHROMEOS_H_
#pragma once

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

namespace chromeos {

class BrowserFrameViewChromeos : public OpaqueBrowserFrameView {
 public:
  BrowserFrameViewChromeos(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserFrameViewChromeos();

  // View overrides.
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
  virtual views::View* GetEventHandlerForPoint(
      const gfx::Point& point) OVERRIDE;

 protected:
  // OpaqueBrowserFrameView overrides.
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void ModifyMaximizedFramePainting(int* top_offset,
                                            SkBitmap** left_corner,
                                            SkBitmap** right_corner) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserFrameViewChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BROWSER_FRAME_VIEW_CHROMEOS_H_
