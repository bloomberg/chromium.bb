// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/chrome_browser_main_extra_parts_aura.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "ui/aura/env.h"

#if !defined(USE_ASH)
#include "ui/aura/desktop/desktop_screen.h"
#include "ui/aura/desktop/desktop_stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // !USE_ASH

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura() {
}

void ChromeBrowserMainExtraPartsAura::PreProfileInit() {
#if !defined(USE_ASH)
  gfx::Screen::SetInstance(aura::CreateDesktopScreen());
  aura::Env::GetInstance()->SetMonitorManager(new aura::SingleMonitorManager);
  stacking_client_.reset(new aura::DesktopStackingClient);
#endif  // !USE_ASH
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
#if !defined(USE_ASH)
  stacking_client_.reset();
#endif

  // aura::Env instance is deleted in BrowserProcessImpl::StartTearDown
  // after the metrics service is deleted.
}

namespace browser {

void AddAuraToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAura());
}

}  // namespace browser
