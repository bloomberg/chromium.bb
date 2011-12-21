// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_H_
#pragma once

#include "ui/gfx/rect.h"

// An interfac that encapsulates the logic to support showing the overflow
// indicator count "+N" for different platform.
class PanelOverflowIndicator {
 public:
  static PanelOverflowIndicator* Create();

  virtual ~PanelOverflowIndicator() { }

  // Returns the height of indicator widget.
  virtual int GetHeight() const = 0;

  // Gets or sets the bounds, in screen coordinates.
  virtual gfx::Rect GetBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // The count value reflects the number of additional overflow panels that are
  // not shown.
  virtual int GetCount() const = 0;
  virtual void SetCount(int count) = 0;

  // The indicator's background could be changed to draw the user's attention.
  virtual void DrawAttention() = 0;
  virtual void StopDrawingAttention() = 0;
  virtual bool IsDrawingAttention() const = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_H_
