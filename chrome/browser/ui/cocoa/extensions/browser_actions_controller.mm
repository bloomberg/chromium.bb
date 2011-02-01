// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <cmath>
#include <string>

#include "app/mac/nsimage_cache.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/chevron_menu_button.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

NSString* const kBrowserActionVisibilityChangedNotification =
    @"BrowserActionVisibilityChangedNotification";

namespace {
const CGFloat kAnimationDuration = 0.2;

const CGFloat kChevronWidth = 14.0;

// Image used for the overflow button.
NSString* const kOverflowChevronsName =
    @"browser_actions_overflow_Template.pdf";

// Since the container is the maximum height of the toolbar, we have
// to move the buttons up by this amount in order to have them look
// vertically centered within the toolbar.
const CGFloat kBrowserActionOriginYOffset = 5.0;

// The size of each button on the toolbar.
const CGFloat kBrowserActionHeight = 29.0;
const CGFloat kBrowserActionWidth = 29.0;

// The padding between browser action buttons.
const CGFloat kBrowserActionButtonPadding = 2.0;

// Padding between Omnibox and first button.  Since the buttons have a
// pixel of internal padding, this needs an extra pixel.
const CGFloat kBrowserActionLeftPadding = kBrowserActionButtonPadding + 1.0;

// How far to inset from the bottom of the view to get the top border
// of the popup 2px below the bottom of the Omnibox.
const CGFloat kBrowserActionBubbleYOffset = 3.0;

}  // namespace

@interface BrowserActionsController(Private)
// Used during initialization to create the BrowserActionButton objects from the
// stored toolbar model.
- (void)createButtons;

// Creates and then adds the given extension's action button to the container
// at the given index within the container. It does not affect the toolbar model
// object since it is called when the toolbar model changes.
- (void)createActionButtonForExtension:(const Extension*)extension
                             withIndex:(NSUInteger)index;

// Removes an action button for the given extension from the container. This
// method also does not affect the underlying toolbar model since it is called
// when the toolbar model changes.
- (void)removeActionButtonForExtension:(const Extension*)extension;

// Useful in the case of a Browser Action being added/removed from the middle of
// the container, this method repositions each button according to the current
// toolbar model.
- (void)positionActionButtonsAndAnimate:(BOOL)animate;

// During container resizing, buttons become more transparent as they are pushed
// off the screen. This method updates each button's opacity determined by the
// position of the button.
- (void)updateButtonOpacity;

// Returns the existing button with the given extension backing it; nil if it
// cannot be found or the extension's ID is invalid.
- (BrowserActionButton*)buttonForExtension:(const Extension*)extension;

// Returns the preferred width of the container given the number of visible
// buttons |buttonCount|.
- (CGFloat)containerWidthWithButtonCount:(NSUInteger)buttonCount;

// Returns the number of buttons that can fit in the container according to its
// current size.
- (NSUInteger)containerButtonCapacity;

// Notification handlers for events registered by the class.

// Updates each button's opacity, the cursor rects and chevron position.
- (void)containerFrameChanged:(NSNotification*)notification;

// Hides the chevron and unhides every hidden button so that dragging the
// container out smoothly shows the Browser Action buttons.
- (void)containerDragStart:(NSNotification*)notification;

// Sends a notification for the toolbar to reposition surrounding UI elements.
- (void)containerDragging:(NSNotification*)notification;

// Determines which buttons need to be hidden based on the new size, hides them
// and updates the chevron overflow menu. Also fires a notification to let the
// toolbar know that the drag has finished.
- (void)containerDragFinished:(NSNotification*)notification;

// Updates the image associated with the button should it be within the chevron
// menu.
- (void)actionButtonUpdated:(NSNotification*)notification;

// Adjusts the position of the surrounding action buttons depending on where the
// button is within the container.
- (void)actionButtonDragging:(NSNotification*)notification;

// Updates the position of the Browser Actions within the container. This fires
// when _any_ Browser Action button is done dragging to keep all open windows in
// sync visually.
- (void)actionButtonDragFinished:(NSNotification*)notification;

// Moves the given button both visually and within the toolbar model to the
// specified index.
- (void)moveButton:(BrowserActionButton*)button
           toIndex:(NSUInteger)index
           animate:(BOOL)animate;

// Handles when the given BrowserActionButton object is clicked.
- (void)browserActionClicked:(BrowserActionButton*)button;

// Returns whether the given extension should be displayed. Only displays
// incognito-enabled extensions in incognito mode. Otherwise returns YES.
- (BOOL)shouldDisplayBrowserAction:(const Extension*)extension;

// The reason |frame| is specified in these chevron functions is because the
// container may be animating and the end frame of the animation should be
// passed instead of the current frame (which may be off and cause the chevron
// to jump at the end of its animation).

// Shows the overflow chevron button depending on whether there are any hidden
// extensions within the frame given.
- (void)showChevronIfNecessaryInFrame:(NSRect)frame animate:(BOOL)animate;

// Moves the chevron to its correct position within |frame|.
- (void)updateChevronPositionInFrame:(NSRect)frame;

// Shows or hides the chevron, animating as specified by |animate|.
- (void)setChevronHidden:(BOOL)hidden
                 inFrame:(NSRect)frame
                 animate:(BOOL)animate;

// Handles when a menu item within the chevron overflow menu is selected.
- (void)chevronItemSelected:(id)menuItem;

// Clears and then populates the overflow menu based on the contents of
// |hiddenButtons_|.
- (void)updateOverflowMenu;

// Updates the container's grippy cursor based on the number of hidden buttons.
- (void)updateGrippyCursors;

// Returns the ID of the currently selected tab or -1 if none exists.
- (int)currentTabId;
@end

// A helper class to proxy extension notifications to the view controller's
// appropriate methods.
class ExtensionServiceObserverBridge : public NotificationObserver,
                                        public ExtensionToolbarModel::Observer {
 public:
  ExtensionServiceObserverBridge(BrowserActionsController* owner,
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
  void BrowserActionAdded(const Extension* extension, int index) {
    [owner_ createActionButtonForExtension:extension withIndex:index];
    [owner_ resizeContainerAndAnimate:NO];
  }

  void BrowserActionRemoved(const Extension* extension) {
    [owner_ removeActionButtonForExtension:extension];
    [owner_ resizeContainerAndAnimate:NO];
  }

 private:
  // The object we need to inform when we get a notification. Weak. Owns us.
  BrowserActionsController* owner_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionServiceObserverBridge);
};

@implementation BrowserActionsController

@synthesize containerView = containerView_;

#pragma mark -
#pragma mark Public Methods

- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    profile_ = browser->profile();

    if (!profile_->GetPrefs()->FindPreference(
        prefs::kBrowserActionContainerWidth))
      [BrowserActionsController registerUserPrefs:profile_->GetPrefs()];

    observer_.reset(new ExtensionServiceObserverBridge(self, profile_));
    ExtensionService* extensionsService = profile_->GetExtensionService();
    // |extensionsService| can be NULL in Incognito.
    if (extensionsService) {
      toolbarModel_ = extensionsService->toolbar_model();
      toolbarModel_->AddObserver(observer_.get());
    }

    containerView_ = container;
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
    // Listen for a finished drag from any button to make sure each open window
    // stays in sync.
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(actionButtonDragFinished:)
             name:kBrowserActionButtonDragEndNotification
           object:nil];

    chevronAnimation_.reset([[NSViewAnimation alloc] init]);
    [chevronAnimation_ gtm_setDuration:kAnimationDuration
                             eventMask:NSLeftMouseUpMask];
    [chevronAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];

    hiddenButtons_.reset([[NSMutableArray alloc] init]);
    buttons_.reset([[NSMutableDictionary alloc] init]);
    [self createButtons];
    [self showChevronIfNecessaryInFrame:[containerView_ frame] animate:NO];
    [self updateGrippyCursors];
    [container setResizable:!profile_->IsOffTheRecord()];
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

- (NSUInteger)buttonCount {
  return [buttons_ count];
}

- (NSUInteger)visibleButtonCount {
  return [self buttonCount] - [hiddenButtons_ count];
}

- (MenuButton*)chevronMenuButton {
  return chevronMenuButton_.get();
}

- (void)resizeContainerAndAnimate:(BOOL)animate {
  int iconCount = toolbarModel_->GetVisibleIconCount();
  if (iconCount < 0)  // If no buttons are hidden.
    iconCount = [self buttonCount];

  [containerView_ resizeToWidth:[self containerWidthWithButtonCount:iconCount]
                        animate:animate];
  NSRect frame = animate ? [containerView_ animationEndFrame] :
                           [containerView_ frame];

  [self showChevronIfNecessaryInFrame:frame animate:animate];

  if (!animate) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kBrowserActionVisibilityChangedNotification
                      object:self];
  }
}

- (NSView*)browserActionViewForExtension:(const Extension*)extension {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    if ([button extension] == extension)
      return button;
  }
  NOTREACHED();
  return nil;
}

- (CGFloat)savedWidth {
  if (!toolbarModel_)
    return 0;
  if (!profile_->GetPrefs()->HasPrefPath(prefs::kExtensionToolbarSize)) {
    // Migration code to the new VisibleIconCount pref.
    // TODO(mpcomplete): remove this at some point.
    double predefinedWidth =
        profile_->GetPrefs()->GetDouble(prefs::kBrowserActionContainerWidth);
    if (predefinedWidth != 0) {
      int iconWidth = kBrowserActionWidth + kBrowserActionButtonPadding;
      int extraWidth = kChevronWidth;
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

- (NSPoint)popupPointForBrowserAction:(const Extension*)extension {
  if (!extension->browser_action())
    return NSZeroPoint;

  NSButton* button = [self buttonForExtension:extension];
  if (!button)
    return NSZeroPoint;

  if ([hiddenButtons_ containsObject:button])
    button = chevronMenuButton_.get();

  // Anchor point just above the center of the bottom.
  const NSRect bounds = [button bounds];
  DCHECK([button isFlipped]);
  NSPoint anchor = NSMakePoint(NSMidX(bounds),
                               NSMaxY(bounds) - kBrowserActionBubbleYOffset);
  return [button convertPoint:anchor toView:nil];
}

- (BOOL)chevronIsHidden {
  if (!chevronMenuButton_.get())
    return YES;

  if (![chevronAnimation_ isAnimating])
    return [chevronMenuButton_ isHidden];

  DCHECK([[chevronAnimation_ viewAnimations] count] > 0);

  // The chevron is animating in or out. Determine which one and have the return
  // value reflect where the animation is headed.
  NSString* effect = [[[chevronAnimation_ viewAnimations] objectAtIndex:0]
      valueForKey:NSViewAnimationEffectKey];
  if (effect == NSViewAnimationFadeInEffect) {
    return NO;
  } else if (effect == NSViewAnimationFadeOutEffect) {
    return YES;
  }

  NOTREACHED();
  return YES;
}

+ (void)registerUserPrefs:(PrefService*)prefs {
  prefs->RegisterDoublePref(prefs::kBrowserActionContainerWidth, 0);
}

#pragma mark -
#pragma mark Private Methods

- (void)createButtons {
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

- (void)createActionButtonForExtension:(const Extension*)extension
                             withIndex:(NSUInteger)index {
  if (!extension->browser_action())
    return;

  if (![self shouldDisplayBrowserAction:extension])
    return;

  if (profile_->IsOffTheRecord())
    index = toolbarModel_->OriginalIndexToIncognito(index);

  // Show the container if it's the first button. Otherwise it will be shown
  // already.
  if ([self buttonCount] == 0)
    [containerView_ setHidden:NO];

  NSRect buttonFrame = NSMakeRect(0.0, kBrowserActionOriginYOffset,
                                  kBrowserActionWidth, kBrowserActionHeight);
  BrowserActionButton* newButton =
      [[[BrowserActionButton alloc]
         initWithFrame:buttonFrame
             extension:extension
               profile:profile_
                 tabId:[self currentTabId]] autorelease];
  [newButton setTarget:self];
  [newButton setAction:@selector(browserActionClicked:)];
  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());
  if (!buttonKey)
    return;
  [buttons_ setObject:newButton forKey:buttonKey];

  [self positionActionButtonsAndAnimate:NO];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(actionButtonDragging:)
             name:kBrowserActionButtonDraggingNotification
           object:newButton];


  [containerView_ setMaxWidth:
      [self containerWidthWithButtonCount:[self buttonCount]]];
  [containerView_ setNeedsDisplay:YES];
}

- (void)removeActionButtonForExtension:(const Extension*)extension {
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
  if ([self buttonCount] == 0) {
    // No more buttons? Hide the container.
    [containerView_ setHidden:YES];
  } else {
    [self positionActionButtonsAndAnimate:NO];
  }
  [containerView_ setMaxWidth:
      [self containerWidthWithButtonCount:[self buttonCount]]];
  [containerView_ setNeedsDisplay:YES];
}

- (void)positionActionButtonsAndAnimate:(BOOL)animate {
  NSUInteger i = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    if (![self shouldDisplayBrowserAction:*iter])
      continue;
    BrowserActionButton* button = [self buttonForExtension:(*iter)];
    if (!button)
      continue;
    if (![button isBeingDragged])
      [self moveButton:button toIndex:i animate:animate];
    ++i;
  }
}

- (void)updateButtonOpacity {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    NSRect buttonFrame = [button frame];
    if (NSContainsRect([containerView_ bounds], buttonFrame)) {
      if ([button alphaValue] != 1.0)
        [button setAlphaValue:1.0];

      continue;
    }
    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
    CGFloat alpha = std::max(0.0f, intersectionWidth / NSWidth(buttonFrame));
    [button setAlphaValue:alpha];
    [button setNeedsDisplay:YES];
  }
}

- (BrowserActionButton*)buttonForExtension:(const Extension*)extension {
  NSString* extensionId = base::SysUTF8ToNSString(extension->id());
  DCHECK(extensionId);
  if (!extensionId)
    return nil;
  return [buttons_ objectForKey:extensionId];
}

- (CGFloat)containerWidthWithButtonCount:(NSUInteger)buttonCount {
  // Left-side padding which works regardless of whether a button or
  // chevron leads.
  CGFloat width = kBrowserActionLeftPadding;

  // Include the buttons and padding between.
  if (buttonCount > 0) {
    width += buttonCount * kBrowserActionWidth;
    width += (buttonCount - 1) * kBrowserActionButtonPadding;
  }

  // Make room for the chevron if any buttons are hidden.
  if ([self buttonCount] != [self visibleButtonCount]) {
    // Chevron and buttons both include 1px padding w/in their bounds,
    // so this leaves 2px between the last browser action and chevron,
    // and also works right if the chevron is the only button.
    width += kChevronWidth;
  }

  return width;
}

- (NSUInteger)containerButtonCapacity {
  // Edge-to-edge span of the browser action buttons.
  CGFloat actionSpan = [self savedWidth] - kBrowserActionLeftPadding;

  // Add in some padding for the browser action on the end, then
  // divide out to get the number of action buttons that fit.
  return (actionSpan + kBrowserActionButtonPadding) /
      (kBrowserActionWidth + kBrowserActionButtonPadding);
}

- (void)containerFrameChanged:(NSNotification*)notification {
  [self updateButtonOpacity];
  [[containerView_ window] invalidateCursorRectsForView:containerView_];
  [self updateChevronPositionInFrame:[containerView_ frame]];
}

- (void)containerDragStart:(NSNotification*)notification {
  [self setChevronHidden:YES inFrame:[containerView_ frame] animate:YES];
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
  [self updateGrippyCursors];

  if (!profile_->IsOffTheRecord())
    toolbarModel_->SetVisibleIconCount([self visibleButtonCount]);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDragFinishedNotification
                    object:self];
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

- (void)actionButtonDragging:(NSNotification*)notification {
  if (![self chevronIsHidden])
    [self setChevronHidden:YES inFrame:[containerView_ frame] animate:YES];

  // Determine what index the dragged button should lie in, alter the model and
  // reposition the buttons.
  CGFloat dragThreshold = std::floor(kBrowserActionWidth / 2);
  BrowserActionButton* draggedButton = [notification object];
  NSRect draggedButtonFrame = [draggedButton frame];

  NSUInteger index = 0;
  for (ExtensionList::iterator iter = toolbarModel_->begin();
       iter != toolbarModel_->end(); ++iter) {
    BrowserActionButton* button = [self buttonForExtension:(*iter)];
    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect(draggedButtonFrame, [button frame]));

    if (intersectionWidth > dragThreshold && button != draggedButton &&
        ![button isAnimating] && index < [self visibleButtonCount]) {
      toolbarModel_->MoveBrowserAction([draggedButton extension], index);
      [self positionActionButtonsAndAnimate:YES];
      return;
    }
    ++index;
  }
}

- (void)actionButtonDragFinished:(NSNotification*)notification {
  [self showChevronIfNecessaryInFrame:[containerView_ frame] animate:YES];
  [self positionActionButtonsAndAnimate:YES];
}

- (void)moveButton:(BrowserActionButton*)button
           toIndex:(NSUInteger)index
           animate:(BOOL)animate {
  CGFloat xOffset = kBrowserActionLeftPadding +
      (index * (kBrowserActionWidth + kBrowserActionButtonPadding));
  NSRect buttonFrame = [button frame];
  buttonFrame.origin.x = xOffset;
  [button setFrame:buttonFrame animate:animate];

  if (index < [self containerButtonCapacity]) {
    // Make sure the button is within the visible container.
    if ([button superview] != containerView_) {
      [containerView_ addSubview:button];
      [button setAlphaValue:1.0];
      [hiddenButtons_ removeObjectIdenticalTo:button];
    }
  } else if (![hiddenButtons_ containsObject:button]) {
    [hiddenButtons_ addObject:button];
    [button removeFromSuperview];
    [button setAlphaValue:0.0];
    [self updateOverflowMenu];
  }
}

- (void)browserActionClicked:(BrowserActionButton*)button {
  int tabId = [self currentTabId];
  if (tabId < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  ExtensionAction* action = [button extension]->browser_action();
  if (action->HasPopup(tabId)) {
    GURL popupUrl = action->GetPopupUrl(tabId);
    // If a popup is already showing, check if the popup URL is the same. If so,
    // then close the popup.
    ExtensionPopupController* popup = [ExtensionPopupController popup];
    if (popup &&
        [[popup window] isVisible] &&
        [popup extensionHost]->GetURL() == popupUrl) {
      [popup close];
      return;
    }
    NSPoint arrowPoint = [self popupPointForBrowserAction:[button extension]];
    [ExtensionPopupController showURL:popupUrl
                            inBrowser:browser_
                           anchoredAt:arrowPoint
                        arrowLocation:info_bubble::kTopRight
                              devMode:NO];
  } else {
    ExtensionService* service = profile_->GetExtensionService();
    service->browser_event_router()->BrowserActionExecuted(
       profile_, action->extension_id(), browser_);
  }
}

- (BOOL)shouldDisplayBrowserAction:(const Extension*)extension {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          profile_->GetExtensionService()->IsIncognitoEnabled(extension));
}

- (void)showChevronIfNecessaryInFrame:(NSRect)frame animate:(BOOL)animate {
  [self setChevronHidden:([self buttonCount] == [self visibleButtonCount])
                 inFrame:frame
                 animate:animate];
}

- (void)updateChevronPositionInFrame:(NSRect)frame {
  CGFloat xPos = NSWidth(frame) - kChevronWidth;
  NSRect buttonFrame = NSMakeRect(xPos,
                                  kBrowserActionOriginYOffset,
                                  kChevronWidth,
                                  kBrowserActionHeight);
  [chevronMenuButton_ setFrame:buttonFrame];
}

- (void)setChevronHidden:(BOOL)hidden
                 inFrame:(NSRect)frame
                 animate:(BOOL)animate {
  if (hidden == [self chevronIsHidden])
    return;

  if (!chevronMenuButton_.get()) {
    chevronMenuButton_.reset([[ChevronMenuButton alloc] init]);
    [chevronMenuButton_ setBordered:NO];
    [chevronMenuButton_ setShowsBorderOnlyWhileMouseInside:YES];
    NSImage* chevronImage =
        app::mac::GetCachedImageWithName(kOverflowChevronsName);
    [chevronMenuButton_ setImage:chevronImage];
    [containerView_ addSubview:chevronMenuButton_];
  }

  if (!hidden)
    [self updateOverflowMenu];

  [self updateChevronPositionInFrame:frame];

  // Stop any running animation.
  [chevronAnimation_ stopAnimation];

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
  [chevronAnimation_ setViewAnimations:
      [NSArray arrayWithObject:animationDictionary]];
  [chevronAnimation_ startAnimation];
}

- (void)chevronItemSelected:(id)menuItem {
  [self browserActionClicked:[menuItem representedObject]];
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

- (void)updateGrippyCursors {
  [containerView_ setCanDragLeft:[hiddenButtons_ count] > 0];
  [containerView_ setCanDragRight:[self visibleButtonCount] > 0];
  [[containerView_ window] invalidateCursorRectsForView:containerView_];
}

- (int)currentTabId {
  TabContents* selected_tab = browser_->GetSelectedTabContents();
  if (!selected_tab)
    return -1;

  return selected_tab->controller().session_id().id();
}

#pragma mark -
#pragma mark Testing Methods

- (NSButton*)buttonWithIndex:(NSUInteger)index {
  if (profile_->IsOffTheRecord())
    index = toolbarModel_->IncognitoIndexToOriginal(index);
  if (index < toolbarModel_->size()) {
    const Extension* extension = toolbarModel_->GetExtensionByIndex(index);
    return [buttons_ objectForKey:base::SysUTF8ToNSString(extension->id())];
  }
  return nil;
}

@end
