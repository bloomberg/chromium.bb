// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

class TabContents;

// PreviewableContentsController manages the display of up to two tab contents
// views.  It is primarily for use with Instant results.  This class supports
// the notion of an "active" view vs. a "preview" tab contents view.
//
// The "active" view is a container view that can be retrieved using
// |-activeContainer|.  Its contents are meant to be managed by an external
// class.
//
// The "preview" can be set using |-showPreview:| and |-hidePreview|.  When a
// preview is set, the active view is hidden (but stays in the view hierarchy).
// When the preview is removed, the active view is reshown.
@interface PreviewableContentsController : NSViewController {
 @private
  // Container view for the "active" contents.
  IBOutlet NSView* activeContainer_;

  // The preview TabContents.  Will be NULL if no preview is currently showing.
  TabContents* previewContents_;  // weak
}

@property(readonly, nonatomic) NSView* activeContainer;

// Sets the current preview and installs its TabContentsView into the view
// hierarchy.  Hides the active view.  |preview| must not be NULL.
- (void)showPreview:(TabContents*)preview;

// Closes the current preview and shows the active view.
- (void)hidePreview;

// Returns YES if the preview contents is currently showing.
- (BOOL)isShowingPreview;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_
