// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/ui/ash/ash_init.h"

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (browser::ShouldOpenAshOnStartup())
    browser::OpenAsh();
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
