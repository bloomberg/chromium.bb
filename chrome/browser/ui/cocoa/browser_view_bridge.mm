// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_view_bridge.h"

#include "build/buildflag.h"
#include "chrome/browser/ui/views_mode_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"

#if BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

namespace browser_view_bridge {

BrowserView* BrowserViewForNativeWindow(gfx::NativeWindow window) {
#if BUILDFLAG(MAC_VIEWS_BROWSER)
  if (!views_mode_controller::IsViewsBrowserCocoa())
    return BrowserView::GetBrowserViewForNativeWindow(window);
#endif
  return nullptr;
}

bool BrowserViewHasAcceleratorForEvent(BrowserView* browser_view,
                                       NSEvent* event) {
#if BUILDFLAG(MAC_VIEWS_BROWSER)
  ui::Accelerator accelerator(ui::KeyboardCodeFromNative(event),
                              ui::EventFlagsFromModifiers(event.modifierFlags));
  return browser_view->IsAcceleratorRegistered(accelerator);
#endif
  return false;
}

}  // namespace browser_view_bridge
