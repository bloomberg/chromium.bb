// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/chromeos/panels/panel_scroller_container.h"

#include "ui/gfx/canvas.h"

PanelScrollerContainer::PanelScrollerContainer(PanelScroller* scroller,
                                               views::View* contents)
    : views::View(),
      scroller_(scroller),
      contents_(contents) {
  AddChildView(0, contents_);
  // TODO(brettw) figure out memory management.
}

PanelScrollerContainer::~PanelScrollerContainer() {
}

gfx::Size PanelScrollerContainer::GetPreferredSize() {
  return gfx::Size(100, 500);
}

void PanelScrollerContainer::Layout() {
}

void PanelScrollerContainer::Paint(gfx::Canvas* canvas) {
  canvas->DrawLineInt(0xFF000080, 0, 0, size().width(), size().height());
}
