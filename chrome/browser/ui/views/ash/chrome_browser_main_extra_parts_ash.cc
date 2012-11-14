// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/ash/tab_scrubber.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_stacking_client.h"

#if defined(FILE_MANAGER_EXTENSION)
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#endif

#if !defined(OS_CHROMEOS)
class ScreenTypeDelegateWin : public gfx::ScreenTypeDelegate {
 public:
  ScreenTypeDelegateWin() {}
  virtual gfx::ScreenType GetScreenTypeForNativeView(
      gfx::NativeView view) OVERRIDE {
    return chrome::IsNativeViewInAsh(view) ?
        gfx::SCREEN_TYPE_ALTERNATE :
        gfx::SCREEN_TYPE_NATIVE;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTypeDelegateWin);
};
#endif

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (chrome::ShouldOpenAshOnStartup()) {
    chrome::OpenAsh();
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kAshEnableTabScrubbing)) {
      TabScrubber::GetInstance();
    }
  } else {
#if !defined(OS_CHROMEOS)
    gfx::Screen::SetScreenTypeDelegate(new ScreenTypeDelegateWin);
#endif
  }

#if defined(FILE_MANAGER_EXTENSION)
  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);
#endif
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  chrome::CloseAsh();
}

namespace chrome {

void AddAshToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
}

}  // namespace chrome
