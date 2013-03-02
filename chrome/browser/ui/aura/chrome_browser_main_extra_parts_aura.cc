// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/chrome_browser_main_extra_parts_aura.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/ui/aura/active_desktop_monitor.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/native_widget_aura.h"

#if defined(OS_LINUX)
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "ui/linux_ui/linux_ui.h"
#else
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_init.h"
#endif

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura() {
}

ChromeBrowserMainExtraPartsAura::~ChromeBrowserMainExtraPartsAura() {
}

void ChromeBrowserMainExtraPartsAura::ToolkitInitialized() {
#if !defined(OS_CHROMEOS)
#if defined(USE_ASH)
  active_desktop_monitor_.reset(new ActiveDesktopMonitor);
  if (!chrome::ShouldOpenAshOnStartup())
#endif
  {
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   views::CreateDesktopScreen());
  }
#endif

#if !defined(USE_ASH) && defined(OS_LINUX)
  // TODO(erg): Refactor this into a dlopen call when we add a GTK3 port.
  ui::LinuxUI::SetInstance(BuildGtk2UI());
#endif
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
  active_desktop_monitor_.reset();

  // aura::Env instance is deleted in BrowserProcessImpl::StartTearDown
  // after the metrics service is deleted.
}

namespace chrome {

void AddAuraToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAura());
}

}  // namespace chrome
