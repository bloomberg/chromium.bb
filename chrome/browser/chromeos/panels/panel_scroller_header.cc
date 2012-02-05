// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

void PanelScrollerHeader::OnMouseReleased(const views::MouseEvent& event) {
  OnMouseCaptureLost();
}

void PanelScrollerHeader::OnMouseCaptureLost() {
  scroller_->HeaderClicked(this);
}

gfx::Size PanelScrollerHeader::GetPreferredSize() {
  return gfx::Size(size().width(), 18);
}

void PanelScrollerHeader::OnPaint(gfx::Canvas* canvas) {
  // TODO(brettw) fill this out with real styling.
  canvas->FillRect(GetLocalBounds(), 0xFFFFFFFF);
  const int y = height() - 1;
  canvas->DrawLine(gfx::Point(0, y), gfx::Point(width(), y), 0xFFE6E6E6);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font font =
      rb.GetFont(ui::ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  int font_top = 1;
  canvas->DrawStringInt(title_, font, 0xFF000000, 3, font_top,
                        size().width(), size().height() - font_top);
}
