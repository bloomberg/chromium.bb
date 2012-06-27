// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "ui/aura/desktop/desktop_stacking_client.h"
#include "ui/aura/desktop/desktop_screen.h"
#include "ui/aura/env.h"
#include "ui/aura/single_display_manager.h"
#include "ui/gfx/screen.h"

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (browser::ShouldOpenAshOnStartup()) {
    browser::OpenAsh();
  } else {
    aura::Env::GetInstance()->SetDisplayManager(new aura::SingleDisplayManager);
    stacking_client_.reset(new aura::DesktopStackingClient);
    gfx::Screen::SetInstance(aura::CreateDesktopScreen());
  }
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  browser::CloseAsh();
}

namespace browser {

void AddAshToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
}

}  // namespace browser
