// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "content/common/notification_registrar.h"

@class InfoBarController;
class InfoBarDelegate;
class InfoBarNotificationObserver;
class TabContents;
class TabStripModel;

// Protocol for basic container methods, as needed by an InfoBarController.
// This protocol exists to make mocking easier in unittests.
@protocol InfoBarContainer
- (void)removeDelegate:(InfoBarDelegate*)delegate;
- (void)willRemoveController:(InfoBarController*)controller;
- (void)removeController:(InfoBarController*)controller;
@end


namespace infobars {

// How tall the tip is on a normal infobar.
const CGFloat kAntiSpoofHeight = 9.0;

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

  // The TabContents we are currently showing infobars for.
  TabContents* currentTabContents_;  // weak

  // Holds the InfoBarControllers currently owned by this container.
  scoped_nsobject<NSMutableArray> infobarControllers_;

  // Holds InfoBarControllers when they are in the process of animating out.
  scoped_nsobject<NSMutableSet> closingInfoBars_;

  // Lets us registers for INFOBAR_ADDED/INFOBAR_REMOVED
  // notifications.  The actual notifications are sent to the
  // InfoBarNotificationObserver object, which proxies them back to us.
  NotificationRegistrar registrar_;
  scoped_ptr<InfoBarNotificationObserver> infoBarObserver_;
}

- (id)initWithResizeDelegate:(id<ViewResizer>)resizeDelegate;

// Informs the selected TabContents that the infobars for the given
// |delegate| need to be removed.  Does not remove any infobar views
// directly, as they will be removed when handling the subsequent
// INFOBAR_REMOVED notification.  Does not notify |delegate| that the
// infobar was closed.
- (void)removeDelegate:(InfoBarDelegate*)delegate;

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
- (void)changeTabContents:(TabContents*)contents;

// Stripped down version of TabStripModelObserverBridge:tabDetachedWithContents.
// Forwarded by BWC. Removes all infobars and deregisters for any notifications
// if |contents| is the current tab contents.
- (void)tabDetachedWithContents:(TabContents*)contents;

// Returns the number of active infobars. This is
// |infobarControllers_ - closingInfoBars_|.
- (NSUInteger)infobarCount;

// Returns the amount of additional height the container view needs to draw the
// anti-spoofing bulge. This will return 0 if |-infobarCount| is 0.
- (CGFloat)antiSpoofHeight;

@end


@interface InfoBarContainerController (ForTheObserverAndTesting)

// Adds an infobar view for the given delegate.
- (void)addInfoBar:(InfoBarDelegate*)delegate animate:(BOOL)animate;

// Closes all the infobar views for a given delegate, either immediately or by
// starting a close animation.
- (void)closeInfoBarsForDelegate:(InfoBarDelegate*)delegate
                         animate:(BOOL)animate;

// Replaces all info bars for the delegate with a new info bar.
// This simply calls closeInfoBarsForDelegate: and then addInfoBar:.
- (void)replaceInfoBarsForDelegate:(InfoBarDelegate*)old_delegate
                              with:(InfoBarDelegate*)new_delegate;

// Positions the infobar views in the container view and notifies
// |browser_controller_| that it needs to resize the container view.
- (void)positionInfoBarsAndRedraw;

@end


@interface InfoBarContainerController (JustForTesting)

// Removes all infobar views.  Callers must call
// positionInfoBarsAndRedraw() after calling this method.
- (void)removeAllInfoBars;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_CONTROLLER_H_
