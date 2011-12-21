// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel_overflow_indicator.h"
#include "ui/views/controls/label.h"

namespace gfx {
class Font;
}
namespace views {
class Widget;
}

class PanelOverflowIndicatorView : public PanelOverflowIndicator {
 public:
  PanelOverflowIndicatorView();
  virtual ~PanelOverflowIndicatorView();

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
  void PaintBackground();
  void UpdateBounds();
  void UpdateTitle();
  void GetFrameMask(const gfx::Rect& rect, gfx::Path* path) const;

  int count_;
  bool is_drawing_attention_;
  scoped_ptr<views::Widget> widget_;
  views::Label title_;

  DISALLOW_COPY_AND_ASSIGN(PanelOverflowIndicatorView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_VIEW_H_
