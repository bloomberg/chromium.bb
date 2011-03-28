// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
#pragma once

#include <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

class TabContents;
class TabContentsNotificationBridge;
@class TabContentsController;

// The interface for the tab contents view controller's delegate.

@protocol TabContentsControllerDelegate

// Tells the delegate when the tab contents view's frame is about to change.
- (void)tabContentsViewFrameWillChange:(TabContentsController*)source
                             frameRect:(NSRect)frameRect;

@end

// A class that controls the TabContents view. It manages displaying the
// native view for a given TabContents.
// Note that just creating the class does not display the view. We defer
// inserting it until the box is the correct size to avoid multiple resize
// messages to the renderer. You must call |-ensureContentsVisible| to display
// the render widget host view.

@interface TabContentsController : NSViewController {
 @private
  TabContents* contents_;  // weak
  // Delegate to be notified about size changes.
  id<TabContentsControllerDelegate> delegate_;  // weak
  scoped_ptr<TabContentsNotificationBridge> tabContentsBridge_;
}
@property(readonly, nonatomic) TabContents* tabContents;

- (id)initWithContents:(TabContents*)contents
              delegate:(id<TabContentsControllerDelegate>)delegate;

// Call when the tab contents is about to be replaced with the currently
// selected tab contents to do not trigger unnecessary content relayout.
- (void)ensureContentsSizeDoesNotChange;

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible;

// Call to change the underlying tab contents object. View is not changed,
// call |-ensureContentsVisible| to display the |newContents|'s render widget
// host view.
- (void)changeTabContents:(TabContents*)newContents;

// Called when the tab contents is the currently selected tab and is about to be
// removed from the view hierarchy.
- (void)willBecomeUnselectedTab;

// Called when the tab contents is about to be put into the view hierarchy as
// the selected tab. Handles things such as ensuring the toolbar is correctly
// enabled.
- (void)willBecomeSelectedTab;

// Called when the tab contents is updated in some non-descript way (the
// notification from the model isn't specific). |updatedContents| could reflect
// an entirely new tab contents object.
- (void)tabDidChange:(TabContents*)updatedContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
