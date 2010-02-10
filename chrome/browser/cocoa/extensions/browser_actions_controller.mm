// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <string>

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/extensions/browser_action_button.h"
#include "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#include "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

// The padding between browser action buttons.
extern const CGFloat kBrowserActionButtonPadding = 3;

NSString* const kBrowserActionsChangedNotification = @"BrowserActionsChanged";

@interface BrowserActionsController(Private)
- (void)createActionButtonForExtension:(Extension*)extension
                             withIndex:(int)index;
- (void)removeActionButtonForExtension:(Extension*)extension;
- (void)repositionActionButtons;
- (int)currentTabId;
@end

// A helper class to proxy extension notifications to the view controller's
// appropriate methods.
class ExtensionsServiceObserverBridge : public NotificationObserver,
                                        public ExtensionToolbarModel::Observer {
 public:
  ExtensionsServiceObserverBridge(BrowserActionsController* owner,
                                  Profile* profile) : owner_(owner) {
    registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                   Source<Profile>(profile));
  }

  // Overridden from NotificationObserver.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
        ExtensionPopupController* popup = [ExtensionPopupController popup];
        if (popup && ![popup isClosing])
          [popup close];

        break;
      }
      default:
        NOTREACHED() << L"Unexpected notification";
    }
  }

  // ExtensionToolbarModel::Observer implementation.
  void BrowserActionAdded(Extension* extension, int index) {
    [owner_ createActionButtonForExtension:extension withIndex:index];
  }

  void BrowserActionRemoved(Extension* extension) {
    [owner_ removeActionButtonForExtension:extension];
  }

 private:
  // The object we need to inform when we get a notification. Weak. Owns us.
  BrowserActionsController* owner_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceObserverBridge);
};

@implementation BrowserActionsController

- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    profile_ = browser->profile();

    observer_.reset(new ExtensionsServiceObserverBridge(self, profile_));
    ExtensionsService* extensionsService = profile_->GetExtensionsService();
    // |extensionsService| can be NULL in Incognito.
    if (extensionsService) {
      toolbarModel_ = extensionsService->toolbar_model();
      toolbarModel_->AddObserver(observer_.get());
    }

    containerView_ = container;
    [containerView_ setHidden:YES];

    buttons_.reset([[NSMutableDictionary alloc] init]);
    buttonOrder_.reset([[NSMutableArray alloc] init]);
  }

  return self;
}

- (void)dealloc {
  if (toolbarModel_)
    toolbarModel_->RemoveObserver(observer_.get());

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)update {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    [button setTabId:[self currentTabId]];
    [button updateState];
  }
}

- (void)createButtons {
  // No extensions in incognito mode.
  if (!toolbarModel_)
    return;

  int i = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    [self createActionButtonForExtension:*iter withIndex:i++];
  }
}

- (void)createActionButtonForExtension:(Extension*)extension
                             withIndex:(int)index {
  if (!extension->browser_action())
    return;

  // Show the container if it's the first button. Otherwise it will be shown
  // already.
  if ([buttons_ count] == 0)
    [containerView_ setHidden:NO];

  BrowserActionButton* newButton = [[[BrowserActionButton alloc]
      initWithExtension:extension
                profile:profile_
                  tabId:[self currentTabId]] autorelease];
  [newButton setTarget:self];
  [newButton setAction:@selector(browserActionClicked:)];
  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());
  [buttons_ setObject:newButton forKey:buttonKey];
  [buttonOrder_ insertObject:newButton atIndex:index];
  [containerView_ addSubview:newButton];
  [self repositionActionButtons];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
  [containerView_ setNeedsDisplay:YES];
}

- (void)removeActionButtonForExtension:(Extension*)extension {
  if (!extension->browser_action())
    return;

  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());

  BrowserActionButton* button = [buttons_ objectForKey:buttonKey];
  if (!button) {
    NOTREACHED();
    return;
  }
  [button removeFromSuperview];
  [buttons_ removeObjectForKey:buttonKey];
  [buttonOrder_ removeObject:button];
  if ([buttons_ count] == 0) {
    // No more buttons? Hide the container.
    [containerView_ setHidden:YES];
  } else {
    [self repositionActionButtons];
  }
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
  [containerView_ setNeedsDisplay:YES];
}

- (void)repositionActionButtons {
  for (NSUInteger i = 0; i < [buttonOrder_ count]; ++i) {
    CGFloat xOffset = i * (kBrowserActionWidth + kBrowserActionButtonPadding);
    BrowserActionButton* button = [buttonOrder_ objectAtIndex:i];
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.x = xOffset;
    [button setFrame:buttonFrame];
  }
}

- (int)buttonCount {
  return [buttons_ count];
}

- (int)visibleButtonCount {
  int count = 0;
  for (BrowserActionButton* button in [buttons_ allValues]) {
    if (![button isHidden])
      ++count;
  }
  return count;
}

- (void)browserActionClicked:(BrowserActionButton*)sender {
  int tabId = [self currentTabId];
  if (tabId < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  ExtensionAction* action = [sender extension]->browser_action();
  if (action->HasPopup(tabId)) {
    NSString* extensionId = base::SysUTF8ToNSString([sender extension]->id());
    // If the extension ID is not valid UTF-8, then the NSString will be nil
    // and an exception will be thrown when calling objectForKey below, hosing
    // the browser. Check it.
    DCHECK(extensionId);
    if (!extensionId)
      return;
    BrowserActionButton* actionButton = [buttons_ objectForKey:extensionId];
    NSRect relativeButtonBounds = [[[actionButton window] contentView]
        convertRect:[actionButton bounds]
           fromView:actionButton];
    NSPoint arrowPoint = [[actionButton window] convertBaseToScreen:NSMakePoint(
        NSMinX(relativeButtonBounds),
        NSMinY(relativeButtonBounds))];
    // Adjust the anchor point to be at the center of the browser action button.
    arrowPoint.x += kBrowserActionWidth / 2;

    [ExtensionPopupController showURL:action->GetPopupUrl(tabId)
                            inBrowser:browser_
                           anchoredAt:arrowPoint
                        arrowLocation:kTopRight];
  } else {
    ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
       profile_, action->extension_id(), browser_);
  }
}

- (int)currentTabId {
  TabContents* selected_tab = browser_->GetSelectedTabContents();
  if (!selected_tab)
    return -1;

  return selected_tab->controller().session_id().id();
}

- (NSView*)browserActionViewForExtension:(Extension*)extension {
  for (BrowserActionButton* button in buttonOrder_.get()) {
    if ([button extension] == extension)
      return button;
  }
  NOTREACHED();
  return nil;
}

- (NSButton*)buttonWithIndex:(int)index {
  return [buttonOrder_ objectAtIndex:(NSUInteger)index];
}

@end
