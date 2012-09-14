// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/download/download_item_controller.h"
#include "chrome/browser/ui/cocoa/download/download_shelf_mac.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_view.h"
#import "chrome/browser/ui/cocoa/hover_button.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/l10n/l10n_util.h"

using content::DownloadItem;

// Download shelf autoclose behavior:
//
// The download shelf autocloses if all of this is true:
// 1) An item on the shelf has just been opened.
// 2) All remaining items on the shelf have been opened in the past.
// 3) The mouse leaves the shelf and remains off the shelf for 5 seconds.
//
// If the mouse re-enters the shelf within the 5 second grace period, the
// autoclose is canceled.  An autoclose can only be scheduled in response to a
// shelf item being opened or removed.  If an item is opened and then the
// resulting autoclose is canceled, subsequent mouse exited events will NOT
// trigger an autoclose.
//
// If the shelf is manually closed while a download is still in progress, that
// download is marked as "opened" for these purposes.  If the shelf is later
// reopened, these previously-in-progress download will not block autoclose,
// even if that download was never actually clicked on and opened.

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

// Amount of time between when the mouse is moved off the shelf and the shelf is
// autoclosed, in seconds.
const NSTimeInterval kAutoCloseDelaySeconds = 5;

// The size of the x button by default.
const NSSize kHoverCloseButtonDefaultSize = { 18, 18 };

}  // namespace

@interface DownloadShelfController(Private)
- (void)showDownloadShelf:(BOOL)enable;
- (void)layoutItems:(BOOL)skipFirst;
- (void)closed;
- (BOOL)canAutoClose;

- (void)viewFrameDidChange:(NSNotification*)notification;

- (void)installTrackingArea;
- (void)cancelAutoCloseAndRemoveTrackingArea;

- (void)willEnterFullscreen;
- (void)willLeaveFullscreen;
- (void)updateCloseButton;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:base::mac::FrameworkBundle()])) {
    resizeDelegate_ = resizeDelegate;
    maxShelfHeight_ = NSHeight([[self view] bounds]);
    currentShelfHeight_ = maxShelfHeight_;
    if (browser && browser->window())
      isFullscreen_ = browser->window()->IsFullscreen();
    else
      isFullscreen_ = NO;

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
    navigator_ = browser;
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK(hoverCloseButton_);

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [[self animatableView] setResizeDelegate:resizeDelegate_];
  [[self view] setPostsFrameChangedNotifications:YES];
  [defaultCenter addObserver:self
                    selector:@selector(viewFrameDidChange:)
                        name:NSViewFrameDidChangeNotification
                      object:[self view]];

  // These notifications are declared in fullscreen_controller, and are posted
  // without objects.
  [defaultCenter addObserver:self
                    selector:@selector(willEnterFullscreen)
                        name:kWillEnterFullscreenNotification
                      object:nil];
  [defaultCenter addObserver:self
                    selector:@selector(willLeaveFullscreen)
                        name:kWillLeaveFullscreenNotification
                      object:nil];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cancelAutoCloseAndRemoveTrackingArea];

  // The controllers will unregister themselves as observers when they are
  // deallocated. No need to do that here.
  [super dealloc];
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

- (AnimatableView*)animatableView {
  return static_cast<AnimatableView*>([self view]);
}

- (void)showDownloadsTab:(id)sender {
  chrome::ShowDownloads(bridge_->browser());
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

- (void)downloadWasOpened:(DownloadItemController*)item_controller {
  // This should only be called on the main thead.
  DCHECK([NSThread isMainThread]);

  if ([self canAutoClose])
    [self installTrackingArea];
}

// We need to explicitly release our download controllers here since they need
// to remove themselves as observers before the remaining shutdown happens.
- (void)exiting {
  [[self animatableView] stopAnimation];
  [self cancelAutoCloseAndRemoveTrackingArea];
  downloadItemControllers_.reset();
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showDownloadShelf:(BOOL)enable {
  if ([self isVisible] == enable)
    return;

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
  [self updateCloseButton];
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
  [self cancelAutoCloseAndRemoveTrackingArea];

  // If |sender| isn't nil, then we're being closed from the UI by the user and
  // we need to tell our shelf implementation to close. Otherwise, we're being
  // closed programmatically by our shelf implementation.
  bool auto_closed = (sender == nil);

  int numInProgress = 0;
  for (NSUInteger i = 0; i < [downloadItemControllers_ count]; ++i) {
    if ([[downloadItemControllers_ objectAtIndex:i]download]->IsInProgress())
      ++numInProgress;
  }
  download_util::RecordShelfClose(
      [downloadItemControllers_ count], numInProgress, auto_closed);
  if (auto_closed)
    [self showDownloadShelf:NO];
  else
    bridge_->Close();
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
  [self cancelAutoCloseAndRemoveTrackingArea];

  // Insert new item at the left.
  scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithModel:model
                                              shelf:self
                                          navigator:navigator_]);

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
  // Don't remove completed downloads if the shelf is just being auto-hidden
  // rather than explicitly closed by the user.
  if (bridge_->is_hidden())
    return;
  NSUInteger i = 0;
  while (i < [downloadItemControllers_ count]) {
    DownloadItemController* itemController =
        [downloadItemControllers_ objectAtIndex:i];
    DownloadItem* download = [itemController download];
    bool isTransferDone = download->IsComplete() ||
                          download->IsCancelled() ||
                          download->IsInterrupted();
    if (isTransferDone &&
        download->GetSafetyState() != DownloadItem::DANGEROUS) {
      [self remove:itemController];
    } else {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      download->SetOpened(true);
      ++i;
    }
  }
}

- (void)mouseEntered:(NSEvent*)event {
  // If the mouse re-enters the download shelf, cancel the auto-close.  Further
  // mouse exits should not trigger autoclose, so also remove the tracking area.
  [self cancelAutoCloseAndRemoveTrackingArea];
}

- (void)mouseExited:(NSEvent*)event {
  // Cancel any previous hide requests, just to be safe.
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(hide:)
                                             object:self];

  // Schedule an autoclose after a delay.  If the mouse is moved back into the
  // view, or if an item is added to the shelf, the timer will be canceled.
  [self performSelector:@selector(hide:)
             withObject:self
             afterDelay:kAutoCloseDelaySeconds];
}

- (BOOL)canAutoClose {
  for (NSUInteger i = 0; i < [downloadItemControllers_ count]; ++i) {
    DownloadItemController* itemController =
        [downloadItemControllers_ objectAtIndex:i];
    if (![itemController download]->GetOpened())
      return NO;
  }
  return YES;
}

- (void)installTrackingArea {
  // Install the tracking area to listen for mouseExited messages and trigger
  // the shelf autoclose.
  if (trackingArea_.get())
    return;

  trackingArea_.reset([[NSTrackingArea alloc]
                        initWithRect:[[self view] bounds]
                             options:NSTrackingMouseEnteredAndExited |
                                     NSTrackingActiveAlways
                               owner:self
                            userInfo:nil]);
  [[self view] addTrackingArea:trackingArea_];
}

- (void)cancelAutoCloseAndRemoveTrackingArea {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(hide:)
                                             object:self];

  if (trackingArea_.get()) {
    [[self view] removeTrackingArea:trackingArea_];
    trackingArea_.reset(nil);
  }
}

- (void)willEnterFullscreen {
  isFullscreen_ = YES;
  [self updateCloseButton];
}

- (void)willLeaveFullscreen {
  isFullscreen_ = NO;
  [self updateCloseButton];
}

- (void)updateCloseButton {
  if (!barIsVisible_)
    return;

  NSRect selfBounds = [[self view] bounds];
  NSRect hoverFrame = [hoverCloseButton_ frame];
  NSRect bounds;

  if (isFullscreen_) {
    bounds = NSMakeRect(NSMinX(hoverFrame), 0,
                        selfBounds.size.width - NSMinX(hoverFrame),
                        selfBounds.size.height);
  } else {
    bounds.origin.x = NSMinX(hoverFrame);
    bounds.origin.y = NSMidY(hoverFrame) -
                      kHoverCloseButtonDefaultSize.height / 2.0;
    bounds.size = kHoverCloseButtonDefaultSize;
  }

  // Set the tracking off to create a new tracking area for the control.
  // When changing the bounds/frame on a HoverButton, the tracking isn't updated
  // correctly, it needs to be turned off and back on.
  [hoverCloseButton_ setTrackingEnabled:NO];
  [hoverCloseButton_ setFrame:bounds];
  [hoverCloseButton_ setTrackingEnabled:YES];
}
@end
