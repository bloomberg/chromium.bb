// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/toolbar/media_router_action_platform_delegate_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"

// static
scoped_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::Create(Browser* browser) {
  return make_scoped_ptr(new MediaRouterActionPlatformDelegateCocoa(browser));
}

MediaRouterActionPlatformDelegateCocoa::MediaRouterActionPlatformDelegateCocoa(
    Browser* browser)
    : MediaRouterActionPlatformDelegate(),
      browser_(browser) {
  DCHECK(browser_);
}

MediaRouterActionPlatformDelegateCocoa::
    ~MediaRouterActionPlatformDelegateCocoa() {
}

bool MediaRouterActionPlatformDelegateCocoa::CloseOverflowMenuIfOpen() {
  // TODO(apacible): This should be factored to share code with extension
  // actions.
  WrenchMenuController* wrenchMenuController =
      [[[BrowserWindowController
          browserWindowControllerForWindow:
              browser_->window()->GetNativeWindow()]
          toolbarController] wrenchMenuController];
  if (![wrenchMenuController isMenuOpen])
    return false;

  [wrenchMenuController cancel];
  return true;
}
