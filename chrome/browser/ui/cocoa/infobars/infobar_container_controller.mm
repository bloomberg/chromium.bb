// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
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
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::TAB_CONTENTS_INFOBAR_ADDED:
        [controller_ addInfoBar:Details<InfoBarDelegate>(details).ptr()
                        animate:YES];
        break;
      case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
        [controller_
          closeInfoBarsForDelegate:Details<InfoBarDelegate>(details).ptr()
                           animate:YES];
        break;
      case NotificationType::TAB_CONTENTS_INFOBAR_REPLACED: {
        typedef std::pair<InfoBarDelegate*, InfoBarDelegate*>
            InfoBarDelegatePair;
        InfoBarDelegatePair* delegates =
            Details<InfoBarDelegatePair>(details).ptr();
        [controller_
         replaceInfoBarsForDelegate:delegates->first with:delegates->second];
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

- (void)removeDelegate:(InfoBarDelegate*)delegate {
  DCHECK(currentTabContents_);
  currentTabContents_->RemoveInfoBar(delegate);
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

- (void)changeTabContents:(TabContents*)contents {
  registrar_.RemoveAll();
  [self removeAllInfoBars];

  currentTabContents_ = contents;
  if (currentTabContents_) {
    for (int i = 0; i < currentTabContents_->infobar_delegate_count(); ++i) {
      [self addInfoBar:currentTabContents_->GetInfoBarDelegateAt(i)
               animate:NO];
    }

    Source<TabContents> source(currentTabContents_);
    registrar_.Add(infoBarObserver_.get(),
                   NotificationType::TAB_CONTENTS_INFOBAR_ADDED, source);
    registrar_.Add(infoBarObserver_.get(),
                   NotificationType::TAB_CONTENTS_INFOBAR_REMOVED, source);
    registrar_.Add(infoBarObserver_.get(),
                   NotificationType::TAB_CONTENTS_INFOBAR_REPLACED, source);
  }

  [self positionInfoBarsAndRedraw];
}

- (void)tabDetachedWithContents:(TabContents*)contents {
  if (currentTabContents_ == contents)
    [self changeTabContents:NULL];
}

- (NSUInteger)infobarCount {
  return [infobarControllers_ count] - [closingInfoBars_ count];
}

- (CGFloat)antiSpoofHeight {
  return [self infobarCount] ? infobars::kAntiSpoofHeight : 0;
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
  CGFloat height = [self antiSpoofHeight];
  for (InfoBarController* controller in infobarControllers_.get())
    height += NSHeight([[controller view] frame]) - infobars::kAntiSpoofHeight;
  return height;
}

- (void)addInfoBar:(InfoBarDelegate*)delegate animate:(BOOL)animate {
  scoped_ptr<InfoBar> infobar(delegate->CreateInfoBar());
  InfoBarController* controller = infobar->controller();
  [controller setContainerController:self];
  [[controller animatableView] setResizeDelegate:self];
  [[self view] addSubview:[controller view]];
  [infobarControllers_ addObject:[controller autorelease]];

  if (animate)
    [controller animateOpen];
  else
    [controller open];
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

- (void)replaceInfoBarsForDelegate:(InfoBarDelegate*)old_delegate
                              with:(InfoBarDelegate*)new_delegate {
  [self closeInfoBarsForDelegate:old_delegate animate:NO];
  [self addInfoBar:new_delegate animate:NO];
}

- (void)removeAllInfoBars {
  for (InfoBarController* controller in infobarControllers_.get()) {
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
    frame.size.width = NSWidth(containerBounds);
    frame.origin.y = minY;
    minY += frame.size.height - infobars::kAntiSpoofHeight;
    [view setFrame:frame];
  }

  [resizeDelegate_ resizeView:[self view] newHeight:[self desiredHeight]];
}

@end
