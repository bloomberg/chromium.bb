// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_AURA)
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

ChromeBrowserMainExtraPartsViews::ChromeBrowserMainExtraPartsViews() {
}

ChromeBrowserMainExtraPartsViews::~ChromeBrowserMainExtraPartsViews() {
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}

void ChromeBrowserMainExtraPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_.reset(new ChromeViewsDelegate);

  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

#if defined(USE_AURA)
  wm_state_.reset(new wm::WMState);
#endif
}

void ChromeBrowserMainExtraPartsViews::PreCreateThreads() {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif
}
