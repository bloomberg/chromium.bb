// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/panels/panel_scroller_header.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/panels/panel_scroller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

PanelScrollerHeader::PanelScrollerHeader(PanelScroller* scroller)
    : views::View(),
      scroller_(scroller) {
}

PanelScrollerHeader::~PanelScrollerHeader() {
}

bool PanelScrollerHeader::OnMousePressed(const views::MouseEvent& event) {
  return true;
}

bool PanelScrollerHeader::OnMouseDragged(const views::MouseEvent& event) {
  return false;
}

void PanelScrollerHeader::OnMouseReleased(const views::MouseEvent& event,
                                          bool canceled) {
  scroller_->HeaderClicked(this);
}

gfx::Size PanelScrollerHeader::GetPreferredSize() {
  return gfx::Size(size().width(), 18);
}

void PanelScrollerHeader::OnPaint(gfx::Canvas* canvas) {
  // TODO(brettw) fill this out with real styling.
  canvas->FillRectInt(0xFFFFFFFF, 0, 0, size().width(), size().height());
  canvas->DrawLineInt(0xFFE6E6E6, 0, size().height() - 1,
                      size().width(), size().height() - 1);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  int font_top = 1;
  canvas->DrawStringInt(title_, font, 0xFF000000, 3, font_top,
                        size().width(), size().height() - font_top);
}
