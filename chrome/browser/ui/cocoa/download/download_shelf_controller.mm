// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/download/download_stats.h"
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
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#import "ui/base/cocoa/hover_button.h"
#include "ui/base/l10n/l10n_util.h"

using content::DownloadItem;

// Download shelf autoclose behavior:
//
// The download shelf autocloses if all of this is true:
// 1) An item on the shelf has just been opened or removed.
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
- (void)removeDownload:(DownloadItemController*)download
        isShelfClosing:(BOOL)isShelfClosing;
- (void)layoutItems:(BOOL)skipFirst;
- (void)closed;
- (void)maybeAutoCloseAfterDelay;
- (void)scheduleAutoClose;
- (void)cancelAutoClose;
- (void)autoClose;
- (void)viewFrameDidChange:(NSNotification*)notification;
- (void)installTrackingArea;
- (void)removeTrackingArea;
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
  [self installTrackingArea];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cancelAutoClose];
  [self removeTrackingArea];

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

- (IBAction)showDownloadsTab:(id)sender {
  chrome::ShowDownloads(bridge_->browser());
}

- (IBAction)handleClose:(id)sender {
  bridge_->Close(DownloadShelf::USER_ACTION);
}

- (void)remove:(DownloadItemController*)download {
  [self removeDownload:download
        isShelfClosing:NO];
}

- (void)removeDownload:(DownloadItemController*)download
        isShelfClosing:(BOOL)isShelfClosing {
  // Look for the download in our controller array and remove it. This will
  // explicity release it so that it removes itself as an Observer of the
  // DownloadItem. We don't want to wait for autorelease since the DownloadItem
  // we are observing will likely be gone by then.
  [[NSNotificationCenter defaultCenter] removeObserver:download];

  // TODO(dmaclach): Remove -- http://crbug.com/25845
  [[download view] removeFromSuperview];

  [downloadItemControllers_ removeObject:download];

  if (!isShelfClosing) {
    [self layoutItems];

    // If there are no more downloads or if all the remaining downloads have
    // been opened, we can close the shelf.
    [self maybeAutoCloseAfterDelay];
  }
}

- (void)downloadWasOpened:(DownloadItemController*)item_controller {
  // This should only be called on the main thead.
  DCHECK([NSThread isMainThread]);
  [self maybeAutoCloseAfterDelay];
}

// We need to explicitly release our download controllers here since they need
// to remove themselves as observers before the remaining shutdown happens.
- (void)exiting {
  [[self animatableView] stopAnimation];
  [self removeTrackingArea];
  [self cancelAutoClose];
  while ([downloadItemControllers_ count] > 0) {
    [self removeDownload:[downloadItemControllers_ lastObject]
          isShelfClosing:YES];
  }
  downloadItemControllers_.reset();
}

- (void)showDownloadShelf:(BOOL)show
             isUserAction:(BOOL)isUserAction {
  [self cancelAutoClose];
  shouldCloseOnMouseExit_ = NO;

  if ([self isVisible] == show)
    return;

  if (!show) {
    int numInProgress = 0;
    for (NSUInteger i = 0; i < [downloadItemControllers_ count]; ++i) {
      DownloadItem* item = [[downloadItemControllers_ objectAtIndex:i]download];
      if (item->GetState() == DownloadItem::IN_PROGRESS)
        ++numInProgress;
    }
    RecordDownloadShelfClose(
        [downloadItemControllers_ count], numInProgress, !isUserAction);
  }

  // Animate the shelf out, but not in.
  // TODO(rohitrao): We do not animate on the way in because Cocoa is already
  // doing a lot of work to set up the download arrow animation.  I've chosen to
  // do no animation over janky animation.  Find a way to make animating in
  // smoother.
  AnimatableView* view = [self animatableView];
  if (show)
    [view setHeight:maxShelfHeight_];
  else
    [view animateToNewHeight:0 duration:kDownloadShelfCloseDuration];

  barIsVisible_ = show;
  [self updateCloseButton];
}

- (DownloadShelf*)bridge {
  return bridge_.get();
}

- (BOOL)isVisible {
  return barIsVisible_;
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

- (void)addDownloadItem:(DownloadItem*)downloadItem {
  DCHECK([NSThread isMainThread]);
  base::scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithDownload:downloadItem
                                                 shelf:self
                                             navigator:navigator_]);
  [self add:controller.get()];
}

- (void)add:(DownloadItemController*)controller {
  DCHECK([NSThread isMainThread]);
  [self cancelAutoClose];
  shouldCloseOnMouseExit_ = NO;

  // Insert new item at the left.
  // Adding at index 0 in NSMutableArrays is O(1).
  [downloadItemControllers_ insertObject:controller atIndex:0];

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
    [self removeDownload:[downloadItemControllers_ lastObject]
          isShelfClosing:NO];
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
    DownloadItem::DownloadState state = download->GetState();
    bool isTransferDone = state == DownloadItem::COMPLETE ||
                          state == DownloadItem::CANCELLED ||
                          state == DownloadItem::INTERRUPTED;
    if (isTransferDone && !download->IsDangerous()) {
      [self removeDownload:itemController
            isShelfClosing:YES];
    } else {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      download->SetOpened(true);
      ++i;
    }
  }
}

- (void)mouseEntered:(NSEvent*)event {
  isMouseInsideView_ = YES;
  // If the mouse re-enters the download shelf, cancel the auto-close.  Further
  // mouse exits should not trigger autoclose.
  if (shouldCloseOnMouseExit_) {
    [self cancelAutoClose];
    shouldCloseOnMouseExit_ = NO;
  }
}

- (void)mouseExited:(NSEvent*)event {
  isMouseInsideView_ = NO;
  if (shouldCloseOnMouseExit_)
    [self scheduleAutoClose];
}

- (void)scheduleAutoClose {
  // Cancel any previous hide requests, just to be safe.
  [self cancelAutoClose];

  // Schedule an autoclose after a delay.  If the mouse is moved back into the
  // view, or if an item is added to the shelf, the timer will be canceled.
  [self performSelector:@selector(autoClose)
             withObject:nil
             afterDelay:kAutoCloseDelaySeconds];
}

- (void)cancelAutoClose {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(autoClose)
                                             object:nil];
}

- (void)maybeAutoCloseAfterDelay {
  // We can close the shelf automatically if all the downloads on the shelf have
  // been opened.
  for (NSUInteger i = 0; i < [downloadItemControllers_ count]; ++i) {
    DownloadItemController* itemController =
        [downloadItemControllers_ objectAtIndex:i];
    if (![itemController download]->GetOpened())
      return;
  }

  if ([self isVisible] && [downloadItemControllers_ count] > 0 &&
      isMouseInsideView_) {
    // If there are download items on the shelf and the user is potentially stil
    // interacting with them, schedule an auto close after the user moves the
    // mouse off the shelf.
    shouldCloseOnMouseExit_ = YES;
  } else {
    // We notify the DownloadShelf of our intention to close even if the shelf
    // is currently hidden. If the shelf was temporarily hidden (e.g. because
    // the browser window entered fullscreen mode), then this prevents the shelf
    // from being shown again when the browser exits fullscreen mode.
    [self autoClose];
  }
}

- (void)autoClose {
  bridge_->Close(DownloadShelf::AUTOMATIC);
}

- (void)installTrackingArea {
  // Install the tracking area to listen for mouseEntered and mouseExited
  // messages.
  DCHECK(!trackingArea_.get());

  trackingArea_.reset([[CrTrackingArea alloc]
                        initWithRect:[[self view] bounds]
                             options:NSTrackingMouseEnteredAndExited |
                                     NSTrackingActiveAlways |
                                     NSTrackingInVisibleRect
                               owner:self
                            userInfo:nil]);
  [[self view] addTrackingArea:trackingArea_.get()];
}

- (void)removeTrackingArea {
  if (trackingArea_.get()) {
    [[self view] removeTrackingArea:trackingArea_.get()];
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
