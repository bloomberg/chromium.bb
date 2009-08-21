// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_controller_target.h"
#import "chrome/browser/cocoa/tab_view.h"

@interface TabController(Private)
- (void)updateVisibility;
@end

@implementation TabController

@synthesize loadingState = loadingState_;
@synthesize target = target_;
@synthesize action = action_;

// The min widths match the windows values and are sums of left + right
// padding, of which we have no comparable constants (we draw using paths, not
// images). The selected tab width includes the close box width.
+ (float)minTabWidth { return 31; }
+ (float)minSelectedTabWidth { return 47; }
+ (float)maxTabWidth { return 220.0; }

- (TabView*)tabView {
  return static_cast<TabView*>([self view]);
}

- (id)init {
  self = [super initWithNibName:@"TabView" bundle:mac_util::MainAppBundle()];
  if (self != nil) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(viewResized:)
               name:NSViewFrameDidChangeNotification
             object:[self view]];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// The internals of |-setSelected:| but doesn't check if we're already set
// to |selected|. Pass the selection change to the subviews that need it and
// mark ourselves as needing a redraw.
- (void)internalSetSelected:(BOOL)selected {
  selected_ = selected;
  [(TabView *)[self view] setState:selected];
  [self updateVisibility];
  [[self view] setNeedsDisplay:YES];
}

// Called when the tab's nib is done loading and all outlets are hooked up.
- (void)awakeFromNib {
  // Ensure we don't show favicon if the tab is already too small to begin with.
  [self updateVisibility];
  [(id)iconView_ setImage:nsimage_cache::ImageNamed(@"nav.pdf")];
  [[self view] addSubview:backgroundButton_
               positioned:NSWindowBelow
               relativeTo:nil];
  [self internalSetSelected:selected_];
}

- (IBAction)closeTab:(id)sender {
  if ([[self target] respondsToSelector:@selector(closeTab:)]) {
    [[self target] performSelector:@selector(closeTab:)
                        withObject:[self view]];
  }
}

// Dispatches the command in the tag to the registered target object.
- (IBAction)commandDispatch:(id)sender {
  TabStripModel::ContextMenuCommand command =
      static_cast<TabStripModel::ContextMenuCommand>([sender tag]);
  [[self target] commandDispatch:command forController:self];
}

// Called for each menu item on its target, which would be this controller.
// Returns YES if the menu item should be enabled. We ask the tab's
// target for the proper answer.
- (BOOL)validateMenuItem:(NSMenuItem*)item {
  TabStripModel::ContextMenuCommand command =
      static_cast<TabStripModel::ContextMenuCommand>([item tag]);
  return [[self target] isCommandEnabled:command forController:self];
}

- (void)setTitle:(NSString *)title {
  [backgroundButton_ setToolTip:title];
  [super setTitle:title];
}

- (void)setSelected:(BOOL)selected {
  if (selected_ != selected)
    [self internalSetSelected:selected];
}

- (BOOL)selected {
  return selected_;
}

- (void)setIconView:(NSView*)iconView {
  NSRect currentFrame = [iconView_ frame];
  [iconView_ removeFromSuperview];
  iconView_ = iconView;
  [iconView_ setFrame:currentFrame];
  // Ensure we don't show favicon if the tab is already too small to begin with.
  [self updateVisibility];
  [[self view] addSubview:iconView_];
}

- (NSView*)iconView {
  return iconView_;
}

- (NSString *)toolTip {
  return [backgroundButton_ toolTip];
}

// Return a rough approximation of the number of icons we could fit in the
// tab. We never actually do this, but it's a helpful guide for determining
// how much space we have available.
- (int)iconCapacity {
  float width = NSWidth([[self view] frame]);
  float leftPadding = NSMinX([iconView_ frame]);
  float rightPadding = width - NSMaxX([closeButton_ frame]);
  float iconWidth = NSWidth([iconView_ frame]);

  width -= leftPadding + rightPadding;
  return width / iconWidth;
}

// Returns YES if we should show the icon. When tabs get too small, we clip
// the favicon before the close box for selected tabs, and prefer the favicon
// for unselected tabs.
// TODO(pinkerton): don't show the icon if there's no image data (eg, NTP).
- (BOOL)shouldShowIcon {
  int iconCapacity = [self iconCapacity];
  if ([self selected])
    return iconCapacity >= 2;
  return iconCapacity >= 1;
}

// Returns YES if we should be showing the close box. The selected tab always
// shows the close box.
- (BOOL)shouldShowCloseBox {
  return [self selected] || [self iconCapacity] >= 3;
}

// Call to update the visibility of certain subviews, such as the icon or
// close box, based on criteria like if the tab is selected and the current
// tab width.
- (void)updateVisibility {
  [iconView_ setHidden:[self shouldShowIcon] ? NO : YES];
  [closeButton_ setHidden:[self shouldShowCloseBox] ? NO : YES];
}

// Called when our view is resized. If it gets too small, start by hiding
// the close box and only show it if tab is selected. Eventually, hide the
// icon as well. We know that this is for our view because we only registered
// for notifications from our specific view.
- (void)viewResized:(NSNotification*)info {
  [self updateVisibility];
}

@end
