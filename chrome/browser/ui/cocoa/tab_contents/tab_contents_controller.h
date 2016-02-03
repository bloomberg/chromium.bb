// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

class FullscreenObserver;

namespace content {
class WebContents;
}

// A class that controls the WebContents view. It internally creates a container
// view (the NSView accessed by calling |-view|) which manages the layout and
// display of the WebContents view.
//
// Client code that inserts [controller view] into the view hierarchy needs to
// beforehand call the |-ensureContentsSizeDoesNotChange| method. This will
// avoid multiple resize messages being sent to the renderer and triggering
// redundant and costly layouts. After the view has been inserted, client code
// calls |-ensureContentsVisible| to display the WebContents view.
//
// AutoEmbedFullscreen mode: When enabled, TabContentsController will observe
// for WebContents fullscreen changes and automatically swap the normal
// WebContents view with the fullscreen view (if different). In addition, if a
// WebContents is being screen-captured, the view will be centered within the
// container view, sized to the aspect ratio of the capture video resolution,
// and scaling will be avoided whenever possible.
@interface TabContentsController : NSViewController {
 @private
   content::WebContents* contents_;  // weak
   // When |fullscreenObserver_| is not-NULL, TabContentsController monitors for
   // and auto-embeds fullscreen widgets as a subview.
   scoped_ptr<FullscreenObserver> fullscreenObserver_;
   // Set to true while TabContentsController is embedding a fullscreen widget
   // view as a subview instead of the normal WebContentsView render view.
   // Note: This will be false in the case of non-Flash fullscreen.
   BOOL isEmbeddingFullscreenWidget_;
}
@property(readonly, nonatomic) content::WebContents* webContents;

// This flag is set to true when we don't want the fullscreen widget to
// resize. This is done so that we can avoid resizing the fullscreen widget
// to intermediate sizes during the fullscreen transition.
// As a result, we would prevent janky movements during the transition and
// Pepper Fullscreen from blowing up.
@property(assign, nonatomic) BOOL blockFullscreenResize;

// Create the contents of a tab represented by |contents|.
- (id)initWithContents:(content::WebContents*)contents;

// Call when the container view owned by TabContentsController is about to be
// resized and inserted into the view hierarchy, so as to not trigger
// unnecessary content re-layout.
- (void)ensureContentsSizeDoesNotChange;

// Call after the container view is inserted into the view hierarchy and
// properly sized. Then, this method will select either the WebContents view or
// the fullscreen view and swap it into the view hierarchy for display.
- (void)ensureContentsVisible;

// Called after we enter fullscreen to ensure that the fullscreen widget will
// have the right frame.
- (void)updateFullscreenWidgetFrame;

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

// Called to switch the container's subview to the WebContents-owned fullscreen
// widget or back to WebContentsView's widget.
- (void)toggleFullscreenWidget:(BOOL)enterFullscreen;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_TAB_CONTENTS_CONTROLLER_H_
