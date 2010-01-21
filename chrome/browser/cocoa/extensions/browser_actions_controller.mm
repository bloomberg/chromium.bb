// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <string>

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/extensions/browser_action_button.h"
#include "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

// The padding between browser action buttons.
extern const CGFloat kBrowserActionButtonPadding = 3;

NSString* const kBrowserActionsChangedNotification = @"BrowserActionsChanged";

@interface BrowserActionsController(Private)
- (void)createActionButtonForExtension:(Extension*)extension;
- (void)removeActionButtonForExtension:(Extension*)extension;
- (void)repositionActionButtons;
- (int)currentTabId;
@end

// A helper class to proxy extension notifications to the view controller's
// appropriate methods.
class ExtensionsServiceObserverBridge : public NotificationObserver {
 public:
  ExtensionsServiceObserverBridge(BrowserActionsController* owner,
                                  Profile* profile) : owner_(owner) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                   Source<Profile>(profile));
  }

  // Runs |owner_|'s method corresponding to the event type received from the
  // notification system.
  // Overridden from NotificationObserver.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED: {
        Extension* extension = Details<Extension>(details).ptr();
        [owner_ createActionButtonForExtension:extension];
        [owner_ browserActionVisibilityHasChanged];
        break;
      }
      case NotificationType::EXTENSION_UNLOADED:
      case NotificationType::EXTENSION_UNLOADED_DISABLED: {
        Extension* extension = Details<Extension>(details).ptr();
        [owner_ removeActionButtonForExtension:extension];
        [owner_ browserActionVisibilityHasChanged];
        break;
      }
      case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
        ExtensionPopupController* popup = [owner_ popup];
        if (popup && Details<ExtensionHost>([popup host]) == details)
          [[owner_ popup] close];

        break;
      }
      default:
        NOTREACHED() << L"Unexpected notification";
    }
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
        containerView:(NSView*)container {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    profile_ = browser->profile();

    containerView_ = container;
    [containerView_ setHidden:YES];
    observer_.reset(new ExtensionsServiceObserverBridge(self, profile_));
    buttons_.reset([[NSMutableDictionary alloc] init]);
    buttonOrder_.reset([[NSMutableArray alloc] init]);
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)update {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    [button setTabId:[self currentTabId]];
    [button updateState];
  }
}

- (ExtensionPopupController*)popup {
  return popupController_;
}

- (void)browserActionVisibilityHasChanged {
  [containerView_ setNeedsDisplay:YES];
}

- (void)createButtons {
  ExtensionsService* extensionsService = profile_->GetExtensionsService();
  if (!extensionsService)  // |extensionsService| can be NULL in Incognito.
    return;

  for (size_t i = 0; i < extensionsService->extensions()->size(); ++i) {
    Extension* extension = extensionsService->GetExtensionById(
        extensionsService->extensions()->at(i)->id(), false);
    if (extension->browser_action()) {
      [self createActionButtonForExtension:extension];
    }
  }
}

- (void)createActionButtonForExtension:(Extension*)extension {
  if (!extension->browser_action())
    return;

  if ([buttons_ count] == 0) {
    // Only call if we're adding our first button, otherwise it will be shown
    // already.
    [containerView_ setHidden:NO];
  }

  int xOffset =
      [buttons_ count] * (kBrowserActionWidth + kBrowserActionButtonPadding);
  BrowserActionButton* newButton =
      [[[BrowserActionButton alloc] initWithExtension:extension
                                                tabId:[self currentTabId]
                                              xOffset:xOffset] autorelease];
  [newButton setTarget:self];
  [newButton setAction:@selector(browserActionClicked:)];
  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());
  [buttons_ setObject:newButton forKey:buttonKey];
  [buttonOrder_ addObject:newButton];
  [containerView_ addSubview:newButton];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
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
    // repositionActionButtons only needs to be called if removing a browser
    // action button because adding one will always append to the end of the
    // container, while removing one may require that those to the right of it
    // be shifted to the left.
    [self repositionActionButtons];
  }
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
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
  ExtensionAction* action = [sender extension]->browser_action();
  if (action->has_popup() && !popupController_) {
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

    popupController_ = [ExtensionPopupController showURL:action->popup_url()
                                               inBrowser:browser_
                                              anchoredAt:arrowPoint
                                           arrowLocation:kTopRight];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(popupWillClose:)
               name:NSWindowWillCloseNotification
             object:[popupController_ window]];
  } else {
    ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
       profile_, action->extension_id(), browser_);
  }
}

// Nil out the weak popup controller reference.
- (void)popupWillClose:(NSNotification*)notification {
  DCHECK([notification object] == [popupController_ window]);
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:[popupController_ window]];
  popupController_ = nil;
}

- (int)currentTabId {
  TabContents* selected_tab = browser_->GetSelectedTabContents();
  if (!selected_tab)
    return -1;

  return selected_tab->controller().session_id().id();
}

- (NSButton*)buttonWithIndex:(int)index {
  return [buttonOrder_ objectAtIndex:(NSUInteger)index];
}

@end
