// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_aura.h"

#include "ui/aura/env.h"

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura()
    : ChromeBrowserMainExtraParts() {
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
  // aura::Env instance is deleted in BrowserProcessImpl::StartTearDown
  // after the metrics service is deleted.
}
