// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SIDEBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_SIDEBAR_CONTROLLER_H_
#pragma once

#import <Foundation/Foundation.h>

#include "base/scoped_nsobject.h"

@class NSSplitView;
@class NSView;

class TabContents;

// Sidebar controller's delegate interface. Delegate is responsible
// for the actual subviews resize and layout.
@protocol SidebarControllerDelegate

// Resizes the sidebar view to the new |width| and adjusts window layout
// accordingly.
- (void)resizeSidebarToNewWidth:(CGFloat)width;

@end

// A class that handles updates of the sidebar view within a browser window.
// It swaps in the relevant sidebar contents for a given TabContents or removes
// the vew, if there's no sidebar contents to show.
@interface SidebarController : NSObject {
 @private
  // A view hosting sidebar contents.
  scoped_nsobject<NSSplitView> sidebarView_;
  id<SidebarControllerDelegate> delegate_;  // weak
  // Currently displayed sidebar contents.
  TabContents* sidebarContents_;  // weak.
}

- (id)initWithView:(NSSplitView*)sidebarView
          delegate:(id<SidebarControllerDelegate>)delegate;

// Depending on |contents|'s state, decides whether the sidebar
// should be shown or hidden and adjusts its width (|delegate_| handles
// the actual resize).
- (void)updateSidebarForTabContents:(TabContents*)contents;

@end

#endif  // CHROME_BROWSER_COCOA_SIDEBAR_CONTROLLER_H_
