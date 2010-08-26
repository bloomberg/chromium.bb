// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/page_info_window_controller.h"

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cocoa/page_info_window_mac.h"
#include "chrome/browser/cocoa/window_size_autosaver.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace {

// The width of the window. The height will be determined by the content.
const NSInteger kWindowWidth = 460;

}  // namespace

@implementation PageInfoWindowController

- (id)init {
  NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask |
                         NSMiniaturizableWindowMask;
  scoped_nsobject<NSWindow> window(
      // Use an arbitrary height because it will be changed by the bridge.
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, kWindowWidth, 100)
                                  styleMask:styleMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  if ((self = [super initWithWindow:window.get()])) {
    [window setTitle:
        l10n_util::GetNSStringWithFixup(IDS_PAGEINFO_WINDOW_TITLE)];
    [window setDelegate:self];

    if (g_browser_process && g_browser_process->local_state()) {
      sizeSaver_.reset([[WindowSizeAutosaver alloc]
          initWithWindow:[self window]
             prefService:g_browser_process->local_state()
                    path:prefs::kPageInfoWindowPlacement]);
    }
  }
  return self;
}

- (void)setPageInfo:(PageInfoWindowMac*)pageInfo {
  pageInfo_.reset(pageInfo);
}

- (IBAction)showCertWindow:(id)sender {
  pageInfo_->ShowCertDialog(0);  // Pass it any int because it's ignored.
}

// If the page info window gets closed, we have nothing left to manage and we
// can clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

@end
