// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <string>

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#import "chrome/browser/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_names.h"

extern const CGFloat kBrowserActionButtonPadding = 3;

extern const NSString* kBrowserActionVisibilityChangedNotification =
    @"BrowserActionVisibilityChangedNotification";

namespace {
const CGFloat kAnimationDuration = 0.2;
const CGFloat kContainerPadding = 2.0;
const CGFloat kGrippyXOffset = 8.0;
const CGFloat kButtonOpacityLeadPadding = 5.0;
}  // namespace

@interface BrowserActionsController(Private)
- (void)createButtons;
- (void)createActionButtonForExtension:(Extension*)extension
                             withIndex:(NSUInteger)index;
- (void)removeActionButtonForExtension:(Extension*)extension;
- (CGFloat)containerWidthWithButtonCount:(NSUInteger)buttonCount;
- (void)repositionActionButtons;
- (void)updateButtonOpacityAndDragAbilities;
- (void)containerFrameChanged;
- (void)containerDragging;
- (void)containerDragFinished;
- (int)currentTabId;
- (bool)shouldDisplayBrowserAction:(Extension*)extension;
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
    [owner_ resizeContainerWithAnimation:NO];
  }

  void BrowserActionRemoved(Extension* extension) {
    [owner_ removeActionButtonForExtension:extension];
    [owner_ resizeContainerWithAnimation:NO];
  }

 private:
  // The object we need to inform when we get a notification. Weak. Owns us.
  BrowserActionsController* owner_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceObserverBridge);
};

@implementation BrowserActionsController

@synthesize containerView = containerView_;

- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    profile_ = browser->profile();

    if (!profile_->GetPrefs()->FindPreference(
        prefs::kBrowserActionContainerWidth))
      [BrowserActionsController registerUserPrefs:profile_->GetPrefs()];

    observer_.reset(new ExtensionsServiceObserverBridge(self, profile_));
    ExtensionsService* extensionsService = profile_->GetExtensionsService();
    // |extensionsService| can be NULL in Incognito.
    if (extensionsService) {
      toolbarModel_ = extensionsService->toolbar_model();
      toolbarModel_->AddObserver(observer_.get());
    }

    containerView_ = container;
    [containerView_ setHidden:YES];
    [containerView_ setCanDragLeft:YES];
    [containerView_ setCanDragRight:YES];
    [containerView_ setPostsFrameChangedNotifications:YES];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerFrameChanged)
               name:NSViewFrameDidChangeNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragging)
               name:kBrowserActionGrippyDraggingNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragFinished)
               name:kBrowserActionGrippyDragFinishedNotification
             object:containerView_];

    buttons_.reset([[NSMutableDictionary alloc] init]);
    [self createButtons];
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

  NSUInteger i = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    if (![self shouldDisplayBrowserAction:*iter])
      continue;

    [self createActionButtonForExtension:*iter withIndex:i++];
  }

  CGFloat width = [self savedWidth];
  // The width will never be 0 (due to the container's minimum size restriction)
  // except when no width has been saved. In this case, set the width to be as
  // if all buttons are shown.
  if (width == 0)
    width = [self containerWidthWithButtonCount:[self buttonCount]];

  [containerView_ resizeToWidth:width animate:NO];
}

- (void)createActionButtonForExtension:(Extension*)extension
                             withIndex:(NSUInteger)index {
  if (!extension->browser_action())
    return;

  if (![self shouldDisplayBrowserAction:extension])
    return;

  if (profile_->IsOffTheRecord())
    index = toolbarModel_->OriginalIndexToIncognito(index);

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
  if (!buttonKey)
    return;
  [buttons_ setObject:newButton forKey:buttonKey];
  [containerView_ addSubview:newButton];
  if (index >= [self visibleButtonCount])
    [newButton setAlphaValue:0.0];

  [self repositionActionButtons];
  [containerView_ setNeedsDisplay:YES];
}

- (void)removeActionButtonForExtension:(Extension*)extension {
  if (!extension->browser_action())
    return;

  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());
  if (!buttonKey)
    return;

  BrowserActionButton* button = [buttons_ objectForKey:buttonKey];
  // This could be the case in incognito, where only a subset of extensions are
  // shown.
  if (!button)
    return;

  [button removeFromSuperview];
  [buttons_ removeObjectForKey:buttonKey];
  if ([buttons_ count] == 0) {
    // No more buttons? Hide the container.
    [containerView_ setHidden:YES];
  } else {
    [self repositionActionButtons];
  }
  [containerView_ setNeedsDisplay:YES];
}

- (void)repositionActionButtons {
  NSUInteger i = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    if (![self shouldDisplayBrowserAction:*iter])
      continue;

    CGFloat xOffset = kGrippyXOffset +
        (i * (kBrowserActionWidth + kBrowserActionButtonPadding));
    NSString* extensionId = base::SysUTF8ToNSString((*iter)->id());
    DCHECK(extensionId);
    if (!extensionId)
      continue;
    BrowserActionButton* button = [buttons_ objectForKey:extensionId];
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.x = xOffset;
    [button setFrame:buttonFrame];
    ++i;
  }
}

- (CGFloat)containerWidthWithButtonCount:(NSUInteger)buttonCount {
  CGFloat width = 0.0;
  if (buttonCount > 0) {
    width = kGrippyXOffset + kContainerPadding +
        (buttonCount * (kBrowserActionWidth + kBrowserActionButtonPadding));
  }
  return width;
}

// Resizes the container given the number of visible buttons in the container,
// taking into account the size of the grippy. Also updates the persistent
// width preference.
- (void)resizeContainerWithAnimation:(BOOL)animate {
  CGFloat width =
      [self containerWidthWithButtonCount:[self visibleButtonCount]];
  [containerView_ resizeToWidth:width animate:animate];

  if (!profile_->IsOffTheRecord())
    profile_->GetPrefs()->SetReal(prefs::kBrowserActionContainerWidth,
        NSWidth([containerView_ frame]));

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionVisibilityChangedNotification
                    object:self];
}

- (void)updateButtonOpacityAndDragAbilities {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.x += kButtonOpacityLeadPadding;
    if (NSContainsRect([containerView_ bounds], buttonFrame)) {
      if ([button alphaValue] != 1.0)
        [button setAlphaValue:1.0];

      continue;
    }
    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
    CGFloat alpha = std::max(0.0f,
        (intersectionWidth - kButtonOpacityLeadPadding) / NSWidth(buttonFrame));
    [button setAlphaValue:alpha];
    [button setNeedsDisplay:YES];
  }

  // Updates the drag direction constraint variables based on relevant metrics.
  [containerView_ setCanDragLeft:
      ([self visibleButtonCount] != [self buttonCount])];
  [[containerView_ window] invalidateCursorRectsForView:containerView_];
}

- (void)containerFrameChanged {
  [self updateButtonOpacityAndDragAbilities];
}

- (void)containerDragging {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDraggingNotification
                    object:self];
}

// Handles when a user initiated drag to resize the container has finished.
- (void)containerDragFinished {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    NSRect buttonFrame = [button frame];
    if (NSContainsRect([containerView_ bounds], buttonFrame))
      continue;

    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
    if (([containerView_ grippyPinned] && intersectionWidth > 0) ||
        (intersectionWidth <= (NSWidth(buttonFrame) / 2)))
      [button setAlphaValue:0.0];
  }

  [self resizeContainerWithAnimation:NO];
}

- (NSUInteger)buttonCount {
  return [buttons_ count];
}

- (NSUInteger)visibleButtonCount {
  int count = 0;
  for (BrowserActionButton* button in [buttons_ allValues]) {
    if ([button alphaValue] > 0.0)
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
    NSPoint arrowPoint = [actionButton frame].origin;
    // Adjust the anchor point to be at the center of the browser action button.
    arrowPoint.x += kBrowserActionWidth / 2;
    arrowPoint = [[actionButton superview] convertPoint:arrowPoint toView:nil];
    arrowPoint = [[actionButton window] convertBaseToScreen:arrowPoint];
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
  for (BrowserActionButton* button in [buttons_ allValues]) {
    if ([button extension] == extension)
      return button;
  }
  NOTREACHED();
  return nil;
}

- (NSButton*)buttonWithIndex:(NSUInteger)index {
  NSUInteger i = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    if (i == index)
      return [buttons_ objectForKey:base::SysUTF8ToNSString((*iter)->id())];

    ++i;
  }
  return nil;
}

- (bool)shouldDisplayBrowserAction:(Extension*)extension {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          profile_->GetExtensionsService()->IsIncognitoEnabled(extension));
}

- (CGFloat)savedWidth {
  // Don't use the standard saved width for incognito until a separate pref is
  // added.
  if (profile_->IsOffTheRecord())
    return 0.0;

  return profile_->GetPrefs()->GetReal(prefs::kBrowserActionContainerWidth);
}

+ (void)registerUserPrefs:(PrefService*)prefs {
  prefs->RegisterRealPref(prefs::kBrowserActionContainerWidth, 0);
}

@end
