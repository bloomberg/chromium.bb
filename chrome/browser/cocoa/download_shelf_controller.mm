// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_controller.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/animatable_view.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/download_item_controller.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#import "chrome/browser/cocoa/hyperlink_button_cell.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

namespace {

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
const size_t kMaxDownloadItemCount = 16;

// Horizontal padding between two download items.
const int kDownloadItemPadding = 0;

// Duration for the open-new-leftmost-item animation, in seconds.
const NSTimeInterval kDownloadItemOpenDuration = 0.8;

// Duration for download shelf closing animation, in seconds.
const NSTimeInterval kDownloadShelfCloseDuration = 0.12;

}  // namespace

@interface DownloadShelfController(Private)
- (void)showDownloadShelf:(BOOL)enable;
- (void)layoutItems:(BOOL)skipFirst;
- (void)closed;

- (void)updateTheme;
- (void)themeDidChangeNotification:(NSNotification*)notification;
- (void)viewFrameDidChange:(NSNotification*)notification;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:mac_util::MainAppBundle()])) {
    resizeDelegate_ = resizeDelegate;
    maxShelfHeight_ = NSHeight([[self view] bounds]);
    currentShelfHeight_ = maxShelfHeight_;

    // Reset the download shelf's frame height to zero.  It will be properly
    // positioned and sized the first time we try to set its height. (Just
    // setting the rect to NSZeroRect does not work: it confuses Cocoa's view
    // layout logic. If the shelf's width is too small, cocoa makes the download
    // item container view wider than the browser window).
    NSRect frame = [[self view] frame];
    frame.size.height = 0;
    [[self view] setFrame:frame];

    downloadItemControllers_.reset([[NSMutableArray alloc] init]);

    bridge_.reset(new DownloadShelfMac(browser, self));
  }
  return self;
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kBrowserThemeDidChangeNotification
                      object:nil];

  [[self animatableView] setResizeDelegate:resizeDelegate_];
  [[self view] setPostsFrameChangedNotifications:YES];
  [defaultCenter addObserver:self
                    selector:@selector(viewFrameDidChange:)
                        name:NSViewFrameDidChangeNotification
                      object:[self view]];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* favicon = rb.GetNSImageNamed(IDR_DOWNLOADS_FAVICON);
  DCHECK(favicon);
  [image_ setImage:favicon];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // The controllers will unregister themselves as observers when they are
  // deallocated. No need to do that here.
  [super dealloc];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self updateTheme];
}

// Called after the frame's rect has changed; usually when the height is
// animated.
- (void)viewFrameDidChange:(NSNotification*)notification {
  // Anchor subviews at the top of |view|, so that it looks like the shelf
  // is sliding out.
  CGFloat newShelfHeight = NSHeight([[self view] frame]);
  if (newShelfHeight == currentShelfHeight_)
    return;

  for (NSView* view in [[self view] subviews]) {
    NSRect frame = [view frame];
    frame.origin.y -= currentShelfHeight_ - newShelfHeight;
    [view setFrame:frame];
  }
  currentShelfHeight_ = newShelfHeight;
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme {
  NSColor* color = nil;

  if (bridge_.get() && bridge_->browser() && bridge_->browser()->profile()) {
    ThemeProvider* provider = bridge_->browser()->profile()->GetThemeProvider();

    color =
        provider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT, false);
  }

  if (!color)
    color = [HyperlinkButtonCell defaultTextColor];

  [showAllDownloadsCell_ setTextColor:color];
}

- (AnimatableView*)animatableView {
  return static_cast<AnimatableView*>([self view]);
}

- (void)showDownloadsTab:(id)sender {
  bridge_->browser()->ShowDownloadsTab();
}

- (void)remove:(DownloadItemController*)download {
  // Look for the download in our controller array and remove it. This will
  // explicity release it so that it removes itself as an Observer of the
  // DownloadItem. We don't want to wait for autorelease since the DownloadItem
  // we are observing will likely be gone by then.
  [[NSNotificationCenter defaultCenter] removeObserver:download];

  // TODO(dmaclach): Remove -- http://crbug.com/25845
  [[download view] removeFromSuperview];

  [downloadItemControllers_ removeObject:download];

  [self layoutItems];

  // Check to see if we have any downloads remaining and if not, hide the shelf.
  if (![downloadItemControllers_ count])
    [self showDownloadShelf:NO];
}

// We need to explicitly release our download controllers here since they need
// to remove themselves as observers before the remaining shutdown happens.
- (void)exiting {
  [[self animatableView] stopAnimation];
  downloadItemControllers_.reset();
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showDownloadShelf:(BOOL)enable {
  if ([self isVisible] == enable)
    return;

  if ([[self view] window])
    [self updateTheme];

  // Animate the shelf out, but not in.
  // TODO(rohitrao): We do not animate on the way in because Cocoa is already
  // doing a lot of work to set up the download arrow animation.  I've chosen to
  // do no animation over janky animation.  Find a way to make animating in
  // smoother.
  AnimatableView* view = [self animatableView];
  if (enable)
    [view setHeight:maxShelfHeight_];
  else
    [view animateToNewHeight:0 duration:kDownloadShelfCloseDuration];

  barIsVisible_ = enable;
}

- (DownloadShelf*)bridge {
  return bridge_.get();
}

- (BOOL)isVisible {
  return barIsVisible_;
}

- (void)show:(id)sender {
  [self showDownloadShelf:YES];
}

- (void)hide:(id)sender {
  // If |sender| isn't nil, then we're being closed from the UI by the user and
  // we need to tell our shelf implementation to close. Otherwise, we're being
  // closed programmatically by our shelf implementation.
  if (sender)
    bridge_->Close();
  else
    [self showDownloadShelf:NO];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (![self isVisible])
    [self closed];
}

- (float)height {
  return maxShelfHeight_;
}

// If |skipFirst| is true, the frame of the leftmost item is not set.
- (void)layoutItems:(BOOL)skipFirst {
  CGFloat currentX = 0;
  for (DownloadItemController* itemController
      in downloadItemControllers_.get()) {
    NSRect frame = [[itemController view] frame];
    frame.origin.x = currentX;
    frame.size.width = [itemController preferredSize].width;
    if (!skipFirst)
      [[[itemController view] animator] setFrame:frame];
    currentX += frame.size.width + kDownloadItemPadding;
    skipFirst = NO;
  }
}

- (void)layoutItems {
  [self layoutItems:NO];
}

- (void)addDownloadItem:(BaseDownloadItemModel*)model {
  DCHECK([NSThread isMainThread]);
  // Insert new item at the left.
  scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithModel:model shelf:self]);

  // Adding at index 0 in NSMutableArrays is O(1).
  [downloadItemControllers_ insertObject:controller.get() atIndex:0];

  [itemContainerView_ addSubview:[controller view]];

  // The controller is in charge of removing itself as an observer in its
  // dealloc.
  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:[controller view]];
  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:itemContainerView_];

  // Start at width 0...
  NSSize size = [controller preferredSize];
  NSRect frame = NSMakeRect(0, 0, 0, size.height);
  [[controller view] setFrame:frame];

  // ...then animate in
  frame.size.width = size.width;
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext]
      gtm_setDuration:kDownloadItemOpenDuration
            eventMask:NSLeftMouseUpMask];
  [[[controller view] animator] setFrame:frame];
  [NSAnimationContext endGrouping];

  // Keep only a limited number of items in the shelf.
  if ([downloadItemControllers_ count] > kMaxDownloadItemCount) {
    DCHECK(kMaxDownloadItemCount > 0);

    // Since no user will ever see the item being removed (needs a horizontal
    // screen resolution greater than 3200 at 16 items at 200 pixels each),
    // there's no point in animating the removal.
    [self remove:[downloadItemControllers_ lastObject]];
  }

  // Finally, move the remaining items to the right. Skip the first item when
  // laying out the items, so that the longer animation duration we set up above
  // is not overwritten.
  [self layoutItems:YES];
}

- (void)closed {
  NSUInteger i = 0;
  while (i < [downloadItemControllers_ count]) {
    DownloadItemController* itemController =
        [downloadItemControllers_ objectAtIndex:i];
    bool isTransferDone =
        [itemController download]->state() == DownloadItem::COMPLETE ||
        [itemController download]->state() == DownloadItem::CANCELLED;
    if (isTransferDone &&
        [itemController download]->safety_state() != DownloadItem::DANGEROUS) {
      [self remove:itemController];
    } else {
      ++i;
    }
  }
}

@end
