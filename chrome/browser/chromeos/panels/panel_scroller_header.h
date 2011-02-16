// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_HEADER_H_
#define CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_HEADER_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "views/view.h"

class PanelScroller;

class PanelScrollerHeader : public views::View {
 public:
  explicit PanelScrollerHeader(PanelScroller* scroller);
  virtual ~PanelScrollerHeader();

  void set_title(const string16& title) { title_ = title; }

  // views::View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual gfx::Size GetPreferredSize();
  virtual void OnPaint(gfx::Canvas* canvas);

 private:
  // Non-owning pointer to our parent scroller object.
  PanelScroller* scroller_;

  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(PanelScrollerHeader);
};

#endif  // CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_HEADER_H_
