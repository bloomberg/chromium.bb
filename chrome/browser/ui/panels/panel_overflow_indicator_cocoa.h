// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_COCOA_H_
#pragma once

#include "chrome/browser/ui/panels/panel_overflow_indicator.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class PanelOverflowIndicatorCocoa : public PanelOverflowIndicator {
 public:
  PanelOverflowIndicatorCocoa();
  virtual ~PanelOverflowIndicatorCocoa();

  // Overridden from OverflowIndicator:
  virtual int GetHeight() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual int GetCount() const OVERRIDE;
  virtual void SetCount(int count) OVERRIDE;
  virtual void DrawAttention() OVERRIDE;
  virtual void StopDrawingAttention() OVERRIDE;
  virtual bool IsDrawingAttention() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelOverflowIndicatorCocoa);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_COCOA_H_
