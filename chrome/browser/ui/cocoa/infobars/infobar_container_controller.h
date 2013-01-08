// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "content/public/browser/notification_registrar.h"

@class BrowserWindowController;
@class InfoBarController;
class InfoBar;
class InfoBarDelegate;
class InfoBarNotificationObserver;
class TabStripModel;

namespace content {
class WebContents;
}

// Protocol for basic container methods, as needed by an InfoBarController.
// This protocol exists to make mocking easier in unittests.
@protocol InfoBarContainer
- (void)willRemoveController:(InfoBarController*)controller;
- (void)removeController:(InfoBarController*)controller;
- (BrowserWindowController*)browserWindowController;
@end


namespace infobars {

// The height of an infobar without the tip.
const CGFloat kBaseHeight = 36.0;

// The height of the infobar tip.
const CGFloat kTipHeight = 12.0;

};  // namespace infobars


// Controller for the infobar container view, which is the superview
// of all the infobar views.  This class owns zero or more
// InfoBarControllers, which manage the infobar views.  This class
// also receives tab strip model notifications and handles
// adding/removing infobars when needed.
@interface InfoBarContainerController : NSViewController <ViewResizer,
                                                          InfoBarContainer> {
 @private
  // Needed to send resize messages when infobars are added or removed.
  id<ViewResizer> resizeDelegate_;  // weak

  // The WebContents we are currently showing infobars for.
  content::WebContents* currentWebContents_;  // weak

  // Holds the InfoBarControllers currently owned by this container.
  scoped_nsobject<NSMutableArray> infobarControllers_;

  // Holds InfoBarControllers when they are in the process of animating out.
  scoped_nsobject<NSMutableSet> closingInfoBars_;

  // Lets us registers for INFOBAR_ADDED/INFOBAR_REMOVED
  // notifications.  The actual notifications are sent to the
  // InfoBarNotificationObserver object, which proxies them back to us.
  content::NotificationRegistrar registrar_;
  scoped_ptr<InfoBarNotificationObserver> infoBarObserver_;
}

- (id)initWithResizeDelegate:(id<ViewResizer>)resizeDelegate;

// Informs the container that the |controller| is going to close. It adds the
// controller to |closingInfoBars_|. Once the animation is complete, the
// controller calls |-removeController:| to finalize cleanup.
- (void)willRemoveController:(InfoBarController*)controller;

// Removes |controller| from the list of controllers in this container and
// removes its view from the view hierarchy.  This method is safe to call while
// |controller| is still on the call stack.
- (void)removeController:(InfoBarController*)controller;

// Modifies this container to display infobars for the given
// |contents|.  Registers for INFOBAR_ADDED and INFOBAR_REMOVED
// notifications for |contents|.  If we are currently showing any
// infobars, removes them first and deregisters for any
// notifications.  |contents| can be NULL, in which case no infobars
// are shown and no notifications are registered for.
- (void)changeWebContents:(content::WebContents*)contents;

// Stripped down version of TabStripModelObserverBridge:tabDetachedWithContents.
// Forwarded by BWC. Removes all infobars and deregisters for any notifications
// if |contents| is the current tab contents.
- (void)tabDetachedWithContents:(content::WebContents*)contents;

// Returns the number of active infobars. This is
// |infobarControllers_ - closingInfoBars_|.
- (NSUInteger)infobarCount;

// Returns the amount of additional height the container view needs to draw the
// anti-spoofing tip. This will return 0 if |-infobarCount| is 0. This is the
// total amount of overlap for all infobars.
- (CGFloat)overlappingTipHeight;

@end


@interface InfoBarContainerController (ForTheObserverAndTesting)

// Adds the given infobar.  Takes ownership of |infobar|.
- (void)addInfoBar:(InfoBar*)infobar animate:(BOOL)animate;

// Closes all the infobar views for a given delegate, either immediately or by
// starting a close animation.
- (void)closeInfoBarsForDelegate:(InfoBarDelegate*)delegate
                         animate:(BOOL)animate;

// Positions the infobar views in the container view and notifies
// |browser_controller_| that it needs to resize the container view.
- (void)positionInfoBarsAndRedraw;

@end


@interface InfoBarContainerController (JustForTesting)

// Removes all infobar views.  Infobars which were already closing will be
// completely closed (i.e. the InfoBarDelegate will be deleted and we'll get a
// callback to removeController).  Other infobars will simply stop animating and
// disappear.  Callers must call positionInfoBarsAndRedraw() after calling this
// method.
- (void)removeAllInfoBars;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_
