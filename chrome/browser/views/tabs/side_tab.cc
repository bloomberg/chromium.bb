// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"

namespace {
const int kVerticalTabHeight = 27;
const int kTextPadding = 4;
const int kFavIconHeight = 16;
};

// static
gfx::Font* SideTab::font_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// SideTab, public:

SideTab::SideTab(SideTabModel* model) : model_(model) {
  InitClass();
}

SideTab::~SideTab() {
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, views::View overrides:

void SideTab::Layout() {
  // TODO(beng):
}

void SideTab::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(model_->IsSelected(this) ? SK_ColorBLUE : SK_ColorRED,
                      0, 0, width(), height());
  gfx::Rect text_rect = GetLocalBounds(false);
  text_rect.Inset(kTextPadding, kTextPadding, kTextPadding, kTextPadding);

  canvas->DrawStringInt(model_->GetTitle(this), *font_, SK_ColorBLACK,
                        text_rect.x(), text_rect.y(), text_rect.width(),
                        text_rect.height());
}

gfx::Size SideTab::GetPreferredSize() {
  const int kTabHeight = std::max(font_->height() + 2 * kTextPadding,
                                  kFavIconHeight + 2 * kTextPadding);
  return gfx::Size(0, kTabHeight);
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, private:

// static
void SideTab::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font_ = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
    initialized = true;
  }
}

