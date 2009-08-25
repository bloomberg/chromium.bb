// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/common/notification_registrar.h"

class InfoBarDelegate;
class InfoBarNotificationObserver;
class TabContents;
class TabStripModel;
class TabStripModelObserverBridge;

// Controller for the infobar container view, which is the superview
// of all the infobar views.  This class owns zero or more
// InfoBarControllers, which manage the infobar views.  This class
// also receives tab strip model notifications and handles
// adding/removing infobars when needed.
@interface InfoBarContainerController : NSViewController {
 @private
  // Needed to send resize messages when infobars are added or removed.
  id<ViewResizer> resizeDelegate_;  // weak

  // The TabContents we are currently showing infobars for.
  TabContents* currentTabContents_;  // weak

  // Holds the InfoBarControllers currently owned by this container.
  scoped_nsobject<NSMutableArray> infobarControllers_;

  // Lets us get TabChanged/TabDetachedAt notifications.
  scoped_ptr<TabStripModelObserverBridge> tabObserver_;

  // Lets us registers for INFOBAR_ADDED/INFOBAR_REMOVED
  // notifications.  The actual notifications are sent to the
  // InfoBarNotificationObserver object, which proxies them back to us.
  NotificationRegistrar registrar_;
  scoped_ptr<InfoBarNotificationObserver> infoBarObserver_;
}

- (id)initWithTabStripModel:(TabStripModel*)model
             resizeDelegate:(id<ViewResizer>)resizeDelegate;

// Informs the selected TabContents that the infobars for the given
// |delegate| need to be removed.  Does not remove any infobar views
// directly, as they will be removed when handling the subsequent
// INFOBAR_REMOVED notification.  Does not notify |delegate| that the
// infobar was closed.
- (void)removeDelegate:(InfoBarDelegate*)delegate;

@end


@interface InfoBarContainerController (ForTheObserverAndTesting)

// Adds an infobar view for the given delegate.  Callers must call
// positionInfoBarsAndRedraw after calling this method.
- (void)addInfoBar:(InfoBarDelegate*)delegate;

// Removes all the infobar views for a given delegate.  Callers must
// call positionInfoBarsAndRedraw after calling this method.
- (void)removeInfoBarsForDelegate:(InfoBarDelegate*)delegate;

// Replaces all info bars for the delegate with a new info bar.
// This simply calls removeInfoBarsForDelegate: and then addInfoBar:.
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
