// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_GTK_H_
#pragma once

#include "chrome/browser/ui/panels/panel_overflow_indicator.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/gtk/gtk_signal.h"

class PanelOverflowIndicatorGtk : public PanelOverflowIndicator {
 public:
  PanelOverflowIndicatorGtk();
  virtual ~PanelOverflowIndicatorGtk();

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
  CHROMEGTK_CALLBACK_1(PanelOverflowIndicatorGtk, gboolean, OnExpose,
                       GdkEventExpose*);

  int count_;
  bool is_drawing_attention_;
  GtkWidget* window_;
  GtkWidget* title_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelOverflowIndicatorGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_INDICATOR_GTK_H_
