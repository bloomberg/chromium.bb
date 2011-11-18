// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chrome_browser_parts_gtk.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

void ChromeBrowserMainPartsLinux::ShowMissingLocaleMessageBox() {
#if defined(USE_AURA)
  // This should never happen on Aura.
  NOTREACHED() << chrome_browser::kMissingLocaleDataMessage;
#elif defined(TOOLKIT_USES_GTK)
  ChromeBrowserPartsGtk::ShowMessageBox(
      chrome_browser::kMissingLocaleDataMessage);
#else
#error "Need MessageBox implementation for linux without Aura or Gtk"
#endif
}
