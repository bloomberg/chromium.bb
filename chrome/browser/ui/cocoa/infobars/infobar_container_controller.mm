// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_cocoa.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_container.h"

@interface InfoBarContainerController ()
// Removes |controller| from the list of controllers in this container and
// removes its view from the view hierarchy.  This method is safe to call while
// |controller| is still on the call stack.
- (void)removeController:(InfoBarController*)controller;
@end


@implementation InfoBarContainerController

@synthesize shouldSuppressTopInfoBarTip = shouldSuppressTopInfoBarTip_;

- (id)initWithResizeDelegate:(id<ViewResizer>)resizeDelegate {
  DCHECK(resizeDelegate);
  if ((self = [super initWithNibName:nil bundle:nil])) {
    base::scoped_nsobject<NSView> view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [view setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
    view_id_util::SetID(view, VIEW_ID_INFO_BAR_CONTAINER);
    [self setView:view];

    resizeDelegate_ = resizeDelegate;
    containerCocoa_.reset(new InfoBarContainerCocoa(self));
    infobarControllers_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)dealloc {
  // Delete the container so that any remaining infobars are removed.
  containerCocoa_.reset();
  DCHECK_EQ([infobarControllers_ count], 0U);
  view_id_util::UnsetID([self view]);
  [super dealloc];
}

- (BrowserWindowController*)browserWindowController {
  id controller = [[[self view] window] windowController];
  if (![controller isKindOfClass:[BrowserWindowController class]])
    return nil;
  return controller;
}

- (CGFloat)infobarArrowX {
  LocationBarViewMac* locationBar =
      [[self browserWindowController] locationBarBridge];
  return locationBar->GetPageInfoBubblePoint().x;
}

- (void)changeWebContents:(content::WebContents*)contents {
  currentWebContents_ = contents;
  InfoBarService* infobar_service =
      contents ? InfoBarService::FromWebContents(contents) : NULL;
  containerCocoa_->ChangeInfoBarManager(infobar_service);
}

- (void)tabDetachedWithContents:(content::WebContents*)contents {
  if (currentWebContents_ == contents)
    [self changeWebContents:NULL];
}

- (CGFloat)overlappingTipHeight {
  return containerCocoa_->GetVerticalOverlap(NULL);
}

- (void)addInfoBar:(InfoBarCocoa*)infobar
          position:(NSUInteger)position {
  InfoBarController* controller = infobar->controller();
  [controller setContainerController:self];
  [infobarControllers_ insertObject:controller atIndex:position];

  NSView* relativeView = nil;
  if (position > 0)
    relativeView = [[infobarControllers_ objectAtIndex:position - 1] view];
  [[self view] addSubview:[controller view]
               positioned:NSWindowAbove
               relativeTo:relativeView];
}

- (void)removeInfoBar:(InfoBarCocoa*)infobar {
  [infobar->controller() infobarWillHide];
  [self removeController:infobar->controller()];
}

- (void)positionInfoBarsAndRedraw:(BOOL)isAnimating {
  if (isAnimating_ != isAnimating) {
    isAnimating_ = isAnimating;
    if ([resizeDelegate_ respondsToSelector:@selector(setAnimationInProgress:)])
      [resizeDelegate_ setAnimationInProgress:isAnimating_];
  }

  NSRect containerBounds = [[self view] bounds];
  int minY = 0;

  // Stack the infobars at the bottom of the view, starting with the
  // last infobar and working our way to the front of the array.  This
  // way we ensure that the first infobar added shows up on top, with
  // the others below.
  for (InfoBarController* controller in
           [infobarControllers_ reverseObjectEnumerator]) {
    NSRect frame;
    frame.origin.x = NSMinX(containerBounds);
    frame.origin.y = minY;
    frame.size.width = NSWidth(containerBounds);
    frame.size.height = [controller infobar]->total_height();
    [[controller view] setFrame:frame];

    minY += NSHeight(frame) - [controller infobar]->arrow_height();
    [controller layoutArrow];
  }

  int totalHeight = 0;
  int overlap = containerCocoa_->GetVerticalOverlap(&totalHeight);

  if (NSHeight([[self view] frame]) != totalHeight) {
    [resizeDelegate_ resizeView:[self view] newHeight:totalHeight];
  } else if (oldOverlappingTipHeight_ != overlap) {
    // If the infobar overlap changed but the height didn't change then
    // explicitly ask for a layout.
    [[self browserWindowController] layoutInfoBars];
  }
  oldOverlappingTipHeight_ = overlap;
}

- (void)setShouldSuppressTopInfoBarTip:(BOOL)flag {
  if (shouldSuppressTopInfoBarTip_ == flag)
    return;
  shouldSuppressTopInfoBarTip_ = flag;
  [self positionInfoBarsAndRedraw:isAnimating_];
}

- (void)removeController:(InfoBarController*)controller {
  if (![infobarControllers_ containsObject:controller])
    return;

  // This code can be executed while InfoBarController is still on the stack, so
  // we retain and autorelease the controller to prevent it from being
  // dealloc'ed too early.
  [[controller retain] autorelease];
  [[controller view] removeFromSuperview];
  [infobarControllers_ removeObject:controller];
}

@end
