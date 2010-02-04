// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab.h"

#include "app/gfx/canvas.h"

namespace {
const int kVerticalTabHeight = 27;
};

////////////////////////////////////////////////////////////////////////////////
// SideTab, public:

SideTab::SideTab(SideTabstrip* tabstrip) {
}

SideTab::~SideTab() {
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, views::View overrides:

void SideTab::Layout() {
  // TODO(beng):
}

void SideTab::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorRED, 0, 0, width(), height());
}

gfx::Size SideTab::GetPreferredSize() {
  return gfx::Size(0, kVerticalTabHeight);
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, private:

