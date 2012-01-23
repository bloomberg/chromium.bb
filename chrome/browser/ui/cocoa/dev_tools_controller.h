// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
#pragma once

#import <Foundation/Foundation.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"

@class NSSplitView;
@class NSView;

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
  scoped_nsobject<NSSplitView> splitView_;

  BOOL dockToRight_;
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

// Specifies whether devtools should dock to right.
- (void)setDockToRight:(BOOL)dock_to_right
           withProfile:(Profile*)profile;
@end

#endif  // CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
