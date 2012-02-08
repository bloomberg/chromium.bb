// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PANEL_FRAME_VIEW_H_
#define ASH_SHELL_PANEL_FRAME_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

class PanelCaption;

class ASH_EXPORT PanelFrameView : public views::NonClientFrameView {
 public:
  PanelFrameView();
  virtual ~PanelFrameView();

 private:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

  // Height of panel's caption
  int CaptionHeight() const;

  // Child View class describing the panel's title bar behavior
  // and buttons, owned by the view hierarchy
  PanelCaption* panel_caption_;
  gfx::Rect client_view_bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelFrameView);
};

}

#endif // ASH_SHELL_PANEL_FRAME_VIEW_H_
