// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_CONTAINER_H_
#define CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_CONTAINER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/view.h"

class PanelScroller;

// This class wraps the contents of a panel in the panel scroller. It currently
// doesn't do anything useful, but it just a placeholder.
class PanelScrollerContainer : public views::View {
 public:
  PanelScrollerContainer(PanelScroller* scroller, views::View* contents);
  virtual ~PanelScrollerContainer();

  int HeaderSize() const;

  // view::View overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Non-owning pointer to our parent scroller object.
  PanelScroller* scroller_;

  views::View* contents_;

  DISALLOW_COPY_AND_ASSIGN(PanelScrollerContainer);
};

#endif  // CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_CONTAINER_H_
