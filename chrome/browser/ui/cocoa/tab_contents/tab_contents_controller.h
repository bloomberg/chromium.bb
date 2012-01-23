// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
#pragma once

#include <Cocoa/Cocoa.h>

namespace content {
class WebContents;
}

// A class that controls the WebContents view. It manages displaying the
// native view for a given WebContents.
// Note that just creating the class does not display the view. We defer
// inserting it until the box is the correct size to avoid multiple resize
// messages to the renderer. You must call |-ensureContentsVisible| to display
// the render widget host view.

@interface TabContentsController : NSViewController {
 @private
   content::WebContents* contents_;  // weak
}
@property(readonly, nonatomic) content::WebContents* webContents;

// Create the contents of a tab represented by |contents|.
- (id)initWithContents:(content::WebContents*)contents;

// Call when the tab contents is about to be replaced with the currently
// selected tab contents to do not trigger unnecessary content relayout.
- (void)ensureContentsSizeDoesNotChange;

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible;

// Call to change the underlying web contents object. View is not changed,
// call |-ensureContentsVisible| to display the |newContents|'s render widget
// host view.
- (void)changeWebContents:(content::WebContents*)newContents;

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
- (void)tabDidChange:(content::WebContents*)updatedContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
