// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <string>

#include "app/resource_bundle.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#import "chrome/browser/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/cocoa/menu_button.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_names.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

extern const CGFloat kBrowserActionButtonPadding = 3;

extern const NSString* kBrowserActionVisibilityChangedNotification =
    @"BrowserActionVisibilityChangedNotification";

namespace {
const CGFloat kAnimationDuration = 0.2;
const CGFloat kButtonOpacityLeadPadding = 5.0;
const CGFloat kChevronHeight = 28.0;
const CGFloat kChevronLowerPadding = 5.0;
const CGFloat kChevronRightPadding = 5.0;
const CGFloat kChevronWidth = 14.0;
const CGFloat kContainerPadding = 2.0;
const CGFloat kGrippyXOffset = 8.0;

}  // namespace

@interface BrowserActionsController(Private)
- (void)createButtons;
- (void)createActionButtonForExtension:(Extension*)extension
                             withIndex:(NSUInteger)index;
- (void)removeActionButtonForExtension:(Extension*)extension;
- (CGFloat)containerWidthWithButtonCount:(NSUInteger)buttonCount;
- (NSUInteger)containerButtonCapacity;
- (void)repositionActionButtons;
- (void)updateButtonOpacityAndDragAbilities;
- (void)containerFrameChanged:(NSNotification*)notification;
- (void)containerDragStart:(NSNotification*)notification;
- (void)containerDragging:(NSNotification*)notification;
- (void)containerDragFinished:(NSNotification*)notification;
- (void)actionButtonUpdated:(NSNotification*)notification;
- (void)browserActionClicked:(BrowserActionButton*)button;
- (int)currentTabId;
- (bool)shouldDisplayBrowserAction:(Extension*)extension;
- (void)showChevronIfNecessaryWithAnimation:(BOOL)animation;
- (void)updateChevronPosition;
- (void)updateOverflowMenu;
- (void)chevronItemSelected:(BrowserActionButton*)button;
- (BrowserActionButton*)buttonForExtension:(Extension*)extension;
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
           selector:@selector(containerFrameChanged:)
               name:NSViewFrameDidChangeNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragStart:)
               name:kBrowserActionGrippyDragStartedNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragging:)
               name:kBrowserActionGrippyDraggingNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragFinished:)
               name:kBrowserActionGrippyDragFinishedNotification
             object:containerView_];

    animation_.reset([[NSViewAnimation alloc] init]);
    [animation_ gtm_setDuration:kAnimationDuration
                      eventMask:NSLeftMouseDownMask];
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];

    hiddenButtons_.reset([[NSMutableArray alloc] init]);
    buttons_.reset([[NSMutableDictionary alloc] init]);
    [self createButtons];
    [self showChevronIfNecessaryWithAnimation:NO];
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

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(actionButtonUpdated:)
             name:kBrowserActionButtonUpdatedNotification
           object:nil];

  CGFloat width = [self savedWidth];
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
  if (index < [self containerButtonCapacity]) {
    [containerView_ addSubview:newButton];
  } else {
    [hiddenButtons_ addObject:newButton];
    [newButton setAlphaValue:0.0];
    [self updateOverflowMenu];
  }

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
  // It may or may not be hidden, but it won't matter to NSMutableArray either
  // way.
  [hiddenButtons_ removeObject:button];
  [self updateOverflowMenu];

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
    BrowserActionButton* button = [self buttonForExtension:(*iter)];
    if (!button)
      continue;
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
  // Make room for the chevron if any buttons are hidden.
  if ([self buttonCount] != [self visibleButtonCount]) {
    width += kChevronWidth + kBrowserActionButtonPadding;
    // Add extra padding if all buttons are hidden.
    if ([self visibleButtonCount] == 0)
      width += 3 * kBrowserActionButtonPadding;
  }
  return width;
}

- (NSUInteger)containerButtonCapacity {
  CGFloat containerWidth = [self savedWidth];
  return (containerWidth - kGrippyXOffset + kContainerPadding) /
      (kBrowserActionWidth + kBrowserActionButtonPadding);
}

// Resizes the container given the number of visible buttons in the container,
// taking into account the size of the grippy. Also updates the persistent
// width preference.
- (void)resizeContainerWithAnimation:(BOOL)animate {
  CGFloat width =
      [self containerWidthWithButtonCount:[self visibleButtonCount]];
  [containerView_ resizeToWidth:width animate:animate];

  [self showChevronIfNecessaryWithAnimation:YES];

  if (!profile_->IsOffTheRecord())
    toolbarModel_->SetVisibleIconCount([self visibleButtonCount]);

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

- (void)containerFrameChanged:(NSNotification*)notification {
  [self updateButtonOpacityAndDragAbilities];
  [self updateChevronPosition];
}

- (void)containerDragStart:(NSNotification*)notification {
  [self setChevronHidden:YES animate:YES];
  while([hiddenButtons_ count] > 0) {
    [containerView_ addSubview:[hiddenButtons_ objectAtIndex:0]];
    [hiddenButtons_ removeObjectAtIndex:0];
  }
}

- (void)containerDragging:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDraggingNotification
                    object:self];
}

// Handles when a user initiated drag to resize the container has finished.
- (void)containerDragFinished:(NSNotification*)notification {
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    BrowserActionButton* button = [self buttonForExtension:(*iter)];
    NSRect buttonFrame = [button frame];
    if (NSContainsRect([containerView_ bounds], buttonFrame))
      continue;

    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
    // Pad the threshold by 5 pixels in order to have the buttons hide more
    // easily.
    if (([containerView_ grippyPinned] && intersectionWidth > 0) ||
        (intersectionWidth <= (NSWidth(buttonFrame) / 2) + 5.0)) {
      [button setAlphaValue:0.0];
      [button removeFromSuperview];
      [hiddenButtons_ addObject:button];
    }
  }
  [self updateOverflowMenu];
  [self resizeContainerWithAnimation:NO];
}

- (void)actionButtonUpdated:(NSNotification*)notification {
  BrowserActionButton* button = [notification object];
  if (![hiddenButtons_ containsObject:button])
    return;

  // +1 item because of the title placeholder. See |updateOverflowMenu|.
  NSUInteger menuIndex = [hiddenButtons_ indexOfObject:button] + 1;
  NSMenuItem* item = [[chevronMenuButton_ attachedMenu] itemAtIndex:menuIndex];
  DCHECK(button == [item representedObject]);
  [item setImage:[button compositedImage]];
}

- (NSUInteger)buttonCount {
  return [buttons_ count];
}

- (NSUInteger)visibleButtonCount {
  return [buttons_ count] - [hiddenButtons_ count];
}

- (MenuButton*)chevronMenuButton {
  return chevronMenuButton_.get();
}

- (void)browserActionClicked:(BrowserActionButton*)button {
  int tabId = [self currentTabId];
  if (tabId < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  ExtensionAction* action = [button extension]->browser_action();
  if (action->HasPopup(tabId)) {
    NSPoint arrowPoint = [self popupPointForBrowserAction:[button extension]];
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
  if (!toolbarModel_)
    return 0;
  if (!profile_->GetPrefs()->HasPrefPath(prefs::kExtensionToolbarSize)) {
    // Migration code to the new VisibleIconCount pref.
    // TODO(mpcomplete): remove this at some point.
    double predefinedWidth =
        profile_->GetPrefs()->GetReal(prefs::kBrowserActionContainerWidth);
    if (predefinedWidth != 0) {
      int iconWidth = kBrowserActionWidth + kBrowserActionButtonPadding;
      int extraWidth = kContainerPadding + kChevronWidth;
      toolbarModel_->SetVisibleIconCount(
          (predefinedWidth - extraWidth) / iconWidth);
    }
  }

  int savedButtonCount = toolbarModel_->GetVisibleIconCount();
  if (savedButtonCount < 0 ||  // all icons are visible
      static_cast<NSUInteger>(savedButtonCount) > [self buttonCount])
    savedButtonCount = [self buttonCount];
  return [self containerWidthWithButtonCount:savedButtonCount];
}

- (void)showChevronIfNecessaryWithAnimation:(BOOL)animation {
  [self setChevronHidden:([self buttonCount] == [self visibleButtonCount])
                 animate:animation];
}

- (NSPoint)popupPointForBrowserAction:(Extension*)extension {
  if (!extension->browser_action())
    return NSZeroPoint;
  BrowserActionButton* button = [self buttonForExtension:extension];
  if (!button)
    return NSZeroPoint;

  NSView* view = button;
  BOOL isHidden = [hiddenButtons_ containsObject:button];
  if (isHidden)
    view = chevronMenuButton_.get();

  NSPoint arrowPoint = [view frame].origin;
  // Adjust the anchor point to be at the center of the browser action button
  // or chevron.
  arrowPoint.x += NSWidth([view frame]) / 2;
  // Move the arrow up a bit in the case that it's pointing to the chevron.
  if (isHidden)
    arrowPoint.y += NSHeight([view frame]) / 4;

  return [[view superview] convertPoint:arrowPoint toView:nil];
}

- (BOOL)chevronIsHidden {
  if (!chevronMenuButton_.get())
    return YES;

  if (![animation_ isAnimating])
    return [chevronMenuButton_ isHidden];

  DCHECK([[animation_ viewAnimations] count] > 0);

  // The chevron is animating in or out. Determine which one and have the return
  // value reflect where the animation is headed.
  NSString* effect = [[[animation_ viewAnimations] objectAtIndex:0]
      valueForKey:NSViewAnimationEffectKey];
  if (effect == NSViewAnimationFadeInEffect) {
    return NO;
  } else if (effect == NSViewAnimationFadeOutEffect) {
    return YES;
  }

  NOTREACHED();
  return YES;
}

- (void)setChevronHidden:(BOOL)hidden animate:(BOOL)animate {
  if (hidden == [self chevronIsHidden])
    return;

  if (!chevronMenuButton_.get()) {
    chevronMenuButton_.reset([[MenuButton alloc] init]);
    [chevronMenuButton_ setBordered:NO];
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    [chevronMenuButton_ setImage:rb.GetNSImageNamed(IDR_BOOKMARK_BAR_CHEVRONS)];
    [containerView_ addSubview:chevronMenuButton_];
  }

  if (!hidden)
    [self updateOverflowMenu];

  [self updateChevronPosition];

  // Stop any running animation.
  [animation_ stopAnimation];

  if (!animate) {
    [chevronMenuButton_ setHidden:hidden];
    return;
  }

  NSDictionary* animationDictionary;
  if (hidden) {
    animationDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        chevronMenuButton_.get(), NSViewAnimationTargetKey,
        NSViewAnimationFadeOutEffect, NSViewAnimationEffectKey,
        nil];
  } else {
    [chevronMenuButton_ setHidden:NO];
    animationDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        chevronMenuButton_.get(), NSViewAnimationTargetKey,
        NSViewAnimationFadeInEffect, NSViewAnimationEffectKey,
        nil];
  }
  [animation_ setViewAnimations:
      [NSArray arrayWithObjects:animationDictionary, nil]];
  [animation_ startAnimation];
}

- (void)updateChevronPosition {
  CGFloat xPos = NSWidth([containerView_ frame]) - kChevronWidth -
      kChevronRightPadding;
  NSRect buttonFrame = NSMakeRect(xPos,
                                  kChevronLowerPadding,
                                  kChevronWidth,
                                  kChevronHeight);
  [chevronMenuButton_ setFrame:buttonFrame];
}

- (void)updateOverflowMenu {
  overflowMenu_.reset([[NSMenu alloc] initWithTitle:@""]);
  // See menu_button.h for documentation on why this is needed.
  [overflowMenu_ addItemWithTitle:@"" action:nil keyEquivalent:@""];

  for (BrowserActionButton* button in hiddenButtons_.get()) {
    NSString* name = base::SysUTF8ToNSString([button extension]->name());
    NSMenuItem* item =
        [overflowMenu_ addItemWithTitle:name
                                 action:@selector(chevronItemSelected:)
                          keyEquivalent:@""];
    [item setRepresentedObject:button];
    [item setImage:[button compositedImage]];
    [item setTarget:self];
  }
  [chevronMenuButton_ setAttachedMenu:overflowMenu_];
}

- (void)chevronItemSelected:(id)menuItem {
  [self browserActionClicked:[menuItem representedObject]];
}

- (BrowserActionButton*)buttonForExtension:(Extension*)extension {
  NSString* extensionId = base::SysUTF8ToNSString(extension->id());
  DCHECK(extensionId);
  if (!extensionId)
    return nil;
  return [buttons_ objectForKey:extensionId];
}

+ (void)registerUserPrefs:(PrefService*)prefs {
  prefs->RegisterRealPref(prefs::kBrowserActionContainerWidth, 0);
}

@end
