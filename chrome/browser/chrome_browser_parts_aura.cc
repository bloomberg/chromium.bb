// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_parts_aura.h"

#include "base/logging.h"

ChromeBrowserPartsAura::ChromeBrowserPartsAura()
    : content::BrowserMainParts() {
}

void ChromeBrowserPartsAura::PreEarlyInitialization() {
  NOTIMPLEMENTED();
}

bool ChromeBrowserPartsAura::MainMessageLoopRun(int* result_code) {
  return false;
}

void ChromeBrowserPartsAura::PostMainMessageLoopStart() {
  NOTIMPLEMENTED();
}

void ChromeBrowserPartsAura::ShowMessageBox(const char* message) {
  LOG(ERROR) << "ShowMessageBox (not implemented): " << message;
  NOTIMPLEMENTED();
}
