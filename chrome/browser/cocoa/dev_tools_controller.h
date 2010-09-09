// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_DEV_TOOLS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_DEV_TOOLS_CONTROLLER_H_
#pragma once

#import <Foundation/Foundation.h>

#include "base/scoped_nsobject.h"

@class NSSplitView;
@class NSView;

class TabContents;

// DevTools controller's delegate interface. Delegate is responsible
// for the actual subviews resize and layout.
@protocol DevToolsControllerDelegate

// Resizes the devTools view to the new |height| and adjusts window layout
// accordingly.
- (void)resizeDevToolsToNewHeight:(CGFloat)height;

@end

// A class that handles updates of the devTools view within a browser window.
// It swaps in the relevant devTools contents for a given TabContents or removes
// the vew, if there's no devTools contents to show.
@interface DevToolsController : NSObject {
 @private
  // A view hosting docked devTools contents.
  scoped_nsobject<NSSplitView> devToolsView_;
  id<DevToolsControllerDelegate> delegate_;  // weak
}

- (id)initWithView:(NSSplitView*)devToolsView
          delegate:(id<DevToolsControllerDelegate>)delegate;

// Depending on |contents|'s state, decides whether the docked web inspector
// should be shown or hidden and adjusts its height (|delegate_| handles
// the actual resize).
- (void)updateDevToolsForTabContents:(TabContents*)contents;

@end

#endif  // CHROME_BROWSER_COCOA_DEV_TOOLS_CONTROLLER_H_
