// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/infobar.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "skia/ext/skia_utils_mac.h"

// C++ class that receives INFOBAR_ADDED and INFOBAR_REMOVED
// notifications and proxies them back to |controller|.
class InfoBarNotificationObserver : public NotificationObserver {
 public:
  InfoBarNotificationObserver(InfoBarContainerController* controller)
      : controller_(controller) {
  }

 private:
  // NotificationObserver implementation
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    TabContentsWrapper* tab_contents = Source<TabContentsWrapper>(source).ptr();
    switch (type) {
      case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED:
        [controller_ addInfoBar:Details<InfoBarAddedDetails>(details)->
                                    CreateInfoBar(tab_contents)
                        animate:YES];
        break;

      case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED: {
        InfoBarRemovedDetails* removed_details =
            Details<InfoBarRemovedDetails>(details).ptr();
        [controller_
            closeInfoBarsForDelegate:removed_details->first
                             animate:(removed_details->second ? YES : NO)];
        break;
      }

      case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED: {
        InfoBarReplacedDetails* replaced_details =
            Details<InfoBarReplacedDetails>(details).ptr();
        [controller_ closeInfoBarsForDelegate:replaced_details->first
                                      animate:NO];
        [controller_ addInfoBar:replaced_details->second->
                                    CreateInfoBar(tab_contents)
                        animate:NO];
        break;
      }

      default:
        NOTREACHED();  // we don't ask for anything else!
        break;
    }

    [controller_ positionInfoBarsAndRedraw];
  }

  InfoBarContainerController* controller_;  // weak, owns us.
};


@interface InfoBarContainerController (PrivateMethods)
// Returns the desired height of the container view, computed by
// adding together the heights of all its subviews.
- (CGFloat)desiredHeight;

@end


@implementation InfoBarContainerController

- (id)initWithResizeDelegate:(id<ViewResizer>)resizeDelegate {
  DCHECK(resizeDelegate);
  if ((self = [super initWithNibName:@"InfoBarContainer"
                              bundle:base::mac::MainAppBundle()])) {
    resizeDelegate_ = resizeDelegate;
    infoBarObserver_.reset(new InfoBarNotificationObserver(self));

    // NSMutableArray needs an initial capacity, and we rarely ever see
    // more than two infobars at a time, so that seems like a good choice.
    infobarControllers_.reset([[NSMutableArray alloc] initWithCapacity:2]);
    closingInfoBars_.reset([[NSMutableSet alloc] initWithCapacity:2]);
  }
  return self;
}

- (void)dealloc {
  DCHECK_EQ([infobarControllers_ count], 0U);
  DCHECK_EQ([closingInfoBars_ count], 0U);
  view_id_util::UnsetID([self view]);
  [super dealloc];
}

- (void)awakeFromNib {
  // The info bar container view is an ordinary NSView object, so we set its
  // ViewID here.
  view_id_util::SetID([self view], VIEW_ID_INFO_BAR_CONTAINER);
}

- (void)willRemoveController:(InfoBarController*)controller {
  [closingInfoBars_ addObject:controller];
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
  [closingInfoBars_ removeObject:controller];
  [self positionInfoBarsAndRedraw];
}

- (BrowserWindowController*)browserWindowController {
  id controller = [[[self view] window] windowController];
  if (![controller isKindOfClass:[BrowserWindowController class]])
    return nil;
  return controller;
}

- (void)changeTabContents:(TabContentsWrapper*)contents {
  registrar_.RemoveAll();
  [self removeAllInfoBars];

  currentTabContents_ = contents;
  if (currentTabContents_) {
    InfoBarTabHelper* infobar_helper =
        currentTabContents_->infobar_tab_helper();
    for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
      InfoBar* infobar = infobar_helper->
          GetInfoBarDelegateAt(i)->CreateInfoBar(currentTabContents_);
      [self addInfoBar:infobar animate:NO];
    }

    Source<TabContentsWrapper> source(currentTabContents_);
    registrar_.Add(infoBarObserver_.get(),
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED, source);
    registrar_.Add(infoBarObserver_.get(),
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, source);
    registrar_.Add(infoBarObserver_.get(),
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED, source);
  }

  [self positionInfoBarsAndRedraw];
}

- (void)tabDetachedWithContents:(TabContentsWrapper*)contents {
  if (currentTabContents_ == contents)
    [self changeTabContents:NULL];
}

- (NSUInteger)infobarCount {
  return [infobarControllers_ count] - [closingInfoBars_ count];
}

- (CGFloat)overlappingTipHeight {
  return [self infobarCount] ? infobars::kTipHeight : 0;
}

- (void)resizeView:(NSView*)view newHeight:(CGFloat)height {
  NSRect frame = [view frame];
  frame.size.height = height;
  [view setFrame:frame];
  [self positionInfoBarsAndRedraw];
}

- (void)setAnimationInProgress:(BOOL)inProgress {
  if ([resizeDelegate_ respondsToSelector:@selector(setAnimationInProgress:)])
    [resizeDelegate_ setAnimationInProgress:inProgress];
}

@end

@implementation InfoBarContainerController (PrivateMethods)

- (CGFloat)desiredHeight {
  CGFloat height = 0;

  // Take out the height of the tip from the total size of the infobar so that
  // the tip overlaps the preceding infobar when there is more than one infobar.
  for (InfoBarController* controller in infobarControllers_.get())
    height += NSHeight([[controller view] frame]) - infobars::kTipHeight;

  // If there are any infobars, add a little extra room for the tip of the first
  // infobar.
  if (height)
    height += infobars::kTipHeight;

  return height;
}

- (void)addInfoBar:(InfoBar*)infobar animate:(BOOL)animate {
  InfoBarController* controller = infobar->controller();
  [controller setContainerController:self];
  [[controller animatableView] setResizeDelegate:self];
  [[self view] addSubview:[controller view]];
  [infobarControllers_ addObject:[controller autorelease]];

  if (animate)
    [controller animateOpen];
  else
    [controller open];

  delete infobar;
}

- (void)closeInfoBarsForDelegate:(InfoBarDelegate*)delegate
                         animate:(BOOL)animate {
  for (InfoBarController* controller in
       [NSArray arrayWithArray:infobarControllers_.get()]) {
    if ([controller delegate] == delegate) {
      [controller infobarWillClose];
      if (animate)
        [controller animateClosed];
      else
        [controller close];
    }
  }
}

- (void)removeAllInfoBars {
  // stopAnimation can remove the infobar from infobarControllers_ if it was in
  // the midst of closing, so copy the array so mutations won't cause problems.
  for (InfoBarController* controller in
       [NSArray arrayWithArray:infobarControllers_.get()]) {
    [[controller animatableView] stopAnimation];
    [[controller view] removeFromSuperview];
  }
  [infobarControllers_ removeAllObjects];
  [closingInfoBars_ removeAllObjects];
}

- (void)positionInfoBarsAndRedraw {
  NSRect containerBounds = [[self view] bounds];
  int minY = 0;

  // Stack the infobars at the bottom of the view, starting with the
  // last infobar and working our way to the front of the array.  This
  // way we ensure that the first infobar added shows up on top, with
  // the others below.
  for (InfoBarController* controller in
           [infobarControllers_ reverseObjectEnumerator]) {
    NSView* view = [controller view];
    NSRect frame = [view frame];
    frame.origin.x = NSMinX(containerBounds);
    frame.origin.y = minY;
    frame.size.width = NSWidth(containerBounds);
    [view setFrame:frame];

    minY += NSHeight(frame) - infobars::kTipHeight;
  }

  [resizeDelegate_ resizeView:[self view] newHeight:[self desiredHeight]];
}

@end
