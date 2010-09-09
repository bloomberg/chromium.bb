// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
#pragma once

#include <Cocoa/Cocoa.h>

class TabContents;

// A class that controls the web contents of a tab. It manages displaying the
// native view for a given TabContents.
// Note that just creating the class does not display the view. We defer
// inserting it until the box is the correct size to avoid multiple resize
// messages to the renderer. You must call |-ensureContentsVisible| to display
// the render widget host view.

@interface TabContentsController : NSViewController {
 @private
  TabContents* contents_;  // weak
}
@property(readonly, nonatomic) TabContents* tabContents;

// Create the contents of a tab represented by |contents| and loaded from the
// nib given by |name|.
- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents;

// Called when the tab contents is the currently selected tab and is about to be
// removed from the view hierarchy.
- (void)willBecomeUnselectedTab;

// Called when the tab contents is about to be put into the view hierarchy as
// the selected tab. Handles things such as ensuring the toolbar is correctly
// enabled.
- (void)willBecomeSelectedTab;

// Call when the tab contents is about to be replaced with the currently
// selected tab contents to do not trigger unnecessary content relayout.
- (void)ensureContentsSizeDoesNotChange;

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible;

// Called when the tab contents is updated in some non-descript way (the
// notification from the model isn't specific). |updatedContents| could reflect
// an entirely new tab contents object.
- (void)tabDidChange:(TabContents*)updatedContents;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_CONTENTS_CONTROLLER_H_
