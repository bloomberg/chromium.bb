// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab_strip.h"

#include "app/gfx/canvas.h"
#include "base/command_line.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace {
const int kVerticalTabSpacing = 2;
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip() {
}

SideTabStrip::~SideTabStrip() {
}

// static
bool SideTabStrip::Available() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVerticalTabs);
}

// static
bool SideTabStrip::Visible(Profile* profile) {
  return Available() &&
      profile->GetPrefs()->GetBoolean(prefs::kUseVerticalTabs);
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, views::View overrides:

void SideTabStrip::Layout() {
  int y = 0;
  for (int c = GetChildViewCount(), i = 0; i < c; ++i) {
    views::View* child = GetChildViewAt(i);
    child->SetBounds(0, y, width(), child->GetPreferredSize().height());
    y = child->bounds().bottom() + kVerticalTabSpacing;
  }
}

void SideTabStrip::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorBLUE, 0, 0, width(), height());
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(127, 0);
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, private:

