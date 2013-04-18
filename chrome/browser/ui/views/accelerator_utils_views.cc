// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace chrome {

#if !defined(USE_AURA)

bool IsChromeAccelerator(const ui::Accelerator& accelerator, Profile* profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(
      profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (!browser)
    return false;
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeWindow());
  return browser_view->IsAcceleratorRegistered(accelerator);
}

#endif  // !USE_AURA

}  // namespace chrome
