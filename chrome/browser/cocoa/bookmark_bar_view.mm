// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_view.h"

#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

@interface BookmarkBarView (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(GTMTheme*)theme;
@end

@implementation BookmarkBarView

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];
  [super dealloc];
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kGTMThemeDidChangeNotification
                      object:nil];

  DCHECK(controller_ && "Expected this to be hooked up via Interface Builder");
  NSArray* types = [NSArray arrayWithObjects:NSStringPboardType,
      NSHTMLPboardType, NSURLPboardType, nil];
  [self registerForDraggedTypes:types];
}

- (void)viewDidMoveToWindow {
  if ([self window])
    [self updateTheme:[self gtm_theme]];
}

- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  GTMTheme* theme = [aNotification object];
  [self updateTheme:theme];
}

- (void)updateTheme:(GTMTheme*)theme {
  NSColor* color = [theme textColorForStyle:GTMThemeStyleBookmarksBarButton
                                      state:GTMThemeStateActiveWindow];
  [noItemTextfield_ setTextColor:color];
}

// Mouse down events on the bookmark bar should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

-(NSTextField*)noItemTextfield {
  return noItemTextfield_;
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  if ([[info draggingPasteboard] containsURLData])
    return NSDragOperationCopy;
  return NSDragOperationNone;
}

- (BOOL)wantsPeriodicDraggingUpdates {
  // TODO(port): This should probably return |YES| and the controller should
  // slide the existing bookmark buttons interactively to the side to make
  // room for the about-to-be-dropped bookmark.
  return NO;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  if ([[info draggingPasteboard] containsURLData])
    return NSDragOperationCopy;
  return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info {
  return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  DCHECK([pboard containsURLData]);

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles];

  return [controller_ addURLs:urls
                   withTitles:titles
                           at:[info draggingLocation]];
}

@end  // @implementation BookmarkBarView
