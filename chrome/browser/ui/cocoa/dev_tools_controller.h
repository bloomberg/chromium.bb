// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
#pragma once

#import <Foundation/Foundation.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

@class NSSplitView;
@class NSView;

class Profile;
class TabContents;

// A class that handles updates of the devTools view within a browser window.
// It swaps in the relevant devTools contents for a given TabContents or removes
// the view, if there's no devTools contents to show.
@interface DevToolsController : NSObject {
 @private
  // A view hosting docked devTools contents.
  scoped_nsobject<NSSplitView> splitView_;

  // Manages currently displayed devTools contents.
  scoped_nsobject<TabContentsController> contentsController_;
}

- (id)initWithDelegate:(id<TabContentsControllerDelegate>)delegate;

// This controller's view.
- (NSView*)view;

// The compiler seems to have trouble handling a function named "view" that
// returns an NSSplitView, so provide a differently-named method.
- (NSSplitView*)splitView;

// Depending on |contents|'s state, decides whether the docked web inspector
// should be shown or hidden and adjusts its height (|delegate_| handles
// the actual resize).
- (void)updateDevToolsForTabContents:(TabContents*)contents
                         withProfile:(Profile*)profile;

// Call when the devTools view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible;

@end

#endif  // CHROME_BROWSER_UI_COCOA_DEV_TOOLS_CONTROLLER_H_
