// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/devtools/devtools_window.h"

@class GraySplitView;
class Profile;

namespace content {
class WebContents;
}

// A class that handles updates of the devTools view within a browser window.
// It swaps in the relevant devTools contents for a given WebContents or removes
// the view, if there's no devTools contents to show.
@interface DevToolsController : NSObject<NSSplitViewDelegate> {
 @private
  // A view hosting docked devTools contents.
  scoped_nsobject<GraySplitView> splitView_;

  DevToolsDockSide dockSide_;

  // Docked devtools window instance. NULL when current tab is not inspected
  // or is inspected with undocked version of DevToolsWindow.
  DevToolsWindow* devToolsWindow_;
}

- (id)init;

// This controller's view.
- (NSView*)view;

// The compiler seems to have trouble handling a function named "view" that
// returns an NSSplitView, so provide a differently-named method.
- (NSSplitView*)splitView;

// Depending on |contents|'s state, decides whether the docked web inspector
// should be shown or hidden and adjusts its height (|delegate_| handles
// the actual resize).
- (void)updateDevToolsForWebContents:(content::WebContents*)contents
                         withProfile:(Profile*)profile;

- (void)setTopContentOffset:(CGFloat)offset;

@end

#endif  // CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
