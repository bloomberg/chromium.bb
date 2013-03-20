// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

@interface AvatarMenuBubbleController (Private)
- (AvatarMenuModel*)model;
- (NSButton*)configureNewUserButton:(CGFloat)yOffset;
- (void)keyDown:(NSEvent*)theEvent;
- (void)moveDown:(id)sender;
- (void)moveUp:(id)sender;
- (void)insertNewline:(id)sender;
- (void)highlightNextItemByDelta:(NSInteger)delta;
- (void)highlightItem:(AvatarMenuItemController*)newItem;
@end

namespace {

// Constants taken from the Windows/Views implementation at:
//    chrome/browser/ui/views/avatar_menu_bubble_view.cc
const CGFloat kBubbleMinWidth = 175;
const CGFloat kBubbleMaxWidth = 800;

// Values derived from the XIB.
const CGFloat kVerticalSpacing = 10.0;
const CGFloat kLinkSpacing = 15.0;
const CGFloat kLabelInset = 49.0;

}  // namespace

@implementation AvatarMenuBubbleController

- (id)initWithBrowser:(Browser*)parentBrowser
           anchoredAt:(NSPoint)point {

  // Pass in a NULL observer. Rebuilding while the bubble is open will cause it
  // to be positioned incorrectly. Since the bubble will be dismissed on losing
  // key status, it's impossible for the user to edit the information in a
  // meaningful way such that it would need to be redrawn.
  AvatarMenuModel* model = new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      NULL, parentBrowser);

  if ((self = [self initWithModel:model
                     parentWindow:parentBrowser->window()->GetNativeWindow()
                       anchoredAt:point])) {
  }
  return self;
}

- (IBAction)newProfile:(id)sender {
  model_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_ICON);
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be crated.
  bool always_create = event_utils::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  model_->SwitchToProfile([sender modelIndex], always_create);
}

- (IBAction)editProfile:(id)sender {
  model_->EditProfile([sender modelIndex]);
}

// Private /////////////////////////////////////////////////////////////////////

- (id)initWithModel:(AvatarMenuModel*)model
       parentWindow:(NSWindow*)parent
         anchoredAt:(NSPoint)point {
  // Use an arbitrary height because it will reflect the size of the content.
  NSRect contentRect = NSMakeRect(0, 0, kBubbleMinWidth, 150);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:parent
                         anchoredAt:point])) {
    model_.reset(model);

    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_BUBBLE_ACCESSIBLE_NAME)
                             forAttribute:NSAccessibilityTitleAttribute];
    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_BUBBLE_ACCESSIBLE_DESCRIPTION)
                             forAttribute:NSAccessibilityHelpAttribute];

    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [self performLayout];
  }
  return self;
}

- (void)performLayout {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSView* contentView = [[self window] contentView];

  // Reset the array of controllers and remove all the views.
  items_.reset([[NSMutableArray alloc] init]);
  [contentView setSubviews:[NSArray array]];

  // |yOffset| is the next position at which to draw in contentView coordinates.
  // Use a little more vertical spacing because the items have padding built-
  // into the xib, and this gives a little more space to visually match.
  CGFloat yOffset = kLinkSpacing;

  // Since drawing happens bottom-up, start with the "New User" link.
  NSButton* newButton = [self configureNewUserButton:yOffset];
  [contentView addSubview:newButton];
  yOffset += NSHeight([newButton frame]) + kVerticalSpacing;

  NSBox* separator = [self separatorWithFrame:
      NSMakeRect(10, yOffset, NSWidth([contentView frame]) - 20, 0)];
  [separator setAutoresizingMask:NSViewWidthSizable];
  [contentView addSubview:separator];

  yOffset += NSHeight([separator frame]);

  // Loop over the profiles in reverse, constructing the menu items.
  CGFloat widthAdjust = 0;
  for (int i = model_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenuModel::Item& item = model_->GetItemAt(i);

    // Create the item view controller. Autorelease it because it will be owned
    // by the |items_| array.
    AvatarMenuItemController* itemView =
        [[[AvatarMenuItemController alloc] initWithModelIndex:item.model_index
                                              menuController:self] autorelease];
    itemView.iconView.image = item.icon.ToNSImage();

    // Adjust the name field to fit the string. If it overflows, record by how
    // much the window needs to grow to accomodate the new size of the field.
    NSTextField* nameField = itemView.nameField;
    nameField.stringValue = base::SysUTF16ToNSString(item.name);
    NSSize delta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:nameField];
    if (delta.width > 0)
      widthAdjust = std::max(widthAdjust, delta.width);

    // Repeat for the sync state/email.
    NSTextField* emailField = itemView.emailField;
    emailField.stringValue = base::SysUTF16ToNSString(item.sync_state);
    delta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:emailField];
    if (delta.width > 0)
      widthAdjust = std::max(widthAdjust, delta.width);

    if (!item.active) {
      // In the inactive case, hide additional UI.
      [itemView.activeView setHidden:YES];
      [itemView.editButton setHidden:YES];
    } else {
      // Otherwise, set up the edit button and its three interaction states.
      itemView.activeView.image =
          rb.GetImageNamed(IDR_PROFILE_SELECTED).ToNSImage();
    }

    // Add the item to the content view.
    [[itemView view] setFrameOrigin:NSMakePoint(0, yOffset)];
    [contentView addSubview:[itemView view]];
    yOffset += NSHeight([[itemView view] frame]);

    // Keep track of the view controller.
    [items_ addObject:itemView];
  }

  yOffset += kVerticalSpacing * 1.5;

  // Set the window frame, clamping the width at a sensible max.
  NSRect frame = [[self window] frame];
  frame.size.height = yOffset;
  frame.size.width = kBubbleMinWidth + widthAdjust;
  frame.size.width = std::min(NSWidth(frame), kBubbleMaxWidth);
  [[self window] setFrame:frame display:YES];
}

- (NSButton*)configureNewUserButton:(CGFloat)yOffset {
  scoped_nsobject<NSButton> newButton(
      [[NSButton alloc] initWithFrame:NSMakeRect(kLabelInset, yOffset,
                                                 90, 16)]);
  scoped_nsobject<HyperlinkButtonCell> buttonCell(
      [[HyperlinkButtonCell alloc] initTextCell:
          l10n_util::GetNSString(IDS_PROFILES_CREATE_NEW_PROFILE_LINK)]);
  [newButton setCell:buttonCell.get()];
  [newButton setFont:[NSFont labelFontOfSize:12.0]];
  [newButton setBezelStyle:NSRegularSquareBezelStyle];
  [newButton setTarget:self];
  [newButton setAction:@selector(newProfile:)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:newButton];
  return [newButton.release() autorelease];
}

- (NSMutableArray*)items {
  return items_.get();
}

- (void)keyDown:(NSEvent*)theEvent {
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
}

- (void)moveDown:(id)sender {
  [self highlightNextItemByDelta:-1];
}

- (void)moveUp:(id)sender {
  [self highlightNextItemByDelta:1];
}

- (void)insertNewline:(id)sender {
  for (AvatarMenuItemController* item in items_.get()) {
    if ([item isHighlighted]) {
      [self switchToProfile:item];
      return;
    }
  }
}

- (void)highlightNextItemByDelta:(NSInteger)delta {
  NSUInteger count = [items_ count];
  if (count == 0)
    return;

  NSInteger old_index = -1;
  for (NSUInteger i = 0; i < count; ++i) {
    if ([[items_ objectAtIndex:i] isHighlighted]) {
      old_index = i;
      break;
    }
  }

  NSInteger new_index;
  // If nothing is selected then start at the top if we're going down and start
  // at the bottom if we're going up.
  if (old_index == -1)
    new_index = delta < 0 ? (count - 1) : 0;
  else
    new_index = old_index + delta;

  // Cap the index. We don't wrap around to match the behavior of Mac menus.
  new_index =
      std::min(std::max(static_cast<NSInteger>(0), new_index),
               static_cast<NSInteger>(count - 1));

  [self highlightItem:[items_ objectAtIndex:new_index]];
}

- (void)highlightItem:(AvatarMenuItemController*)newItem {
  AvatarMenuItemController* oldItem = nil;
  for (AvatarMenuItemController* item in items_.get()) {
    if ([item isHighlighted]) {
      oldItem = item;
      break;
    }
  }

  if (oldItem == newItem)
    return;

  [oldItem setIsHighlighted:NO];
  [newItem setIsHighlighted:YES];
}


@end

// Menu Item Controller ////////////////////////////////////////////////////////

@interface AvatarMenuItemController (Private)
- (void)animateFromView:(NSView*)outView toView:(NSView*)inView;
@end

@implementation AvatarMenuItemController

@synthesize modelIndex = modelIndex_;
@synthesize isHighlighted = isHighlighted_;
@synthesize iconView = iconView_;
@synthesize activeView = activeView_;
@synthesize nameField = nameField_;
@synthesize emailField = emailField_;
@synthesize editButton = editButton_;

- (id)initWithModelIndex:(size_t)modelIndex
          menuController:(AvatarMenuBubbleController*)controller {
  if ((self = [super initWithNibName:@"AvatarMenuItem"
                              bundle:base::mac::FrameworkBundle()])) {
    modelIndex_ = modelIndex;
    controller_ = controller;
    [self loadView];
  }
  return self;
}

- (void)dealloc {
  static_cast<AvatarMenuItemView*>(self.view).viewController = nil;
  [linkAnimation_ stopAnimation];
  [linkAnimation_ setDelegate:nil];
  [super dealloc];
}

- (void)awakeFromNib {
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:self.editButton];
  self.editButton.hidden = YES;
}

- (IBAction)switchToProfile:(id)sender {
  [controller_ switchToProfile:self];
}

- (IBAction)editProfile:(id)sender {
  [controller_ editProfile:self];
}

- (void)highlightForEventType:(NSEventType)type {
  switch (type) {
    case NSMouseEntered:
      [controller_ highlightItem:self];
      break;

    case NSMouseExited:
      [controller_ highlightItem:nil];
      break;

    default:
      NOTREACHED();
  };
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (isHighlighted_ == isHighlighted)
    return;

  isHighlighted_ = isHighlighted;
  [[self view] setNeedsDisplay:YES];

  // Cancel any running animation.
  if (linkAnimation_.get()) {
    [NSObject cancelPreviousPerformRequestsWithTarget:linkAnimation_
                                             selector:@selector(startAnimation)
                                               object:nil];
  }

  // Fade the edit link in or out only if this is the active view.
  if (self.activeView.isHidden)
    return;

  if (isHighlighted_) {
    [self animateFromView:self.emailField toView:self.editButton];
  } else {
    // If the edit button is visible or the animation to make it so is
    // running, stop the animation and fade it back to the email. If not, then
    // don't run an animation to prevent flickering.
    if (!self.editButton.isHidden || [linkAnimation_ isAnimating]) {
      [linkAnimation_ stopAnimation];
      linkAnimation_.reset();
      [self animateFromView:self.editButton toView:self.emailField];
    }
  }
}

- (void)animateFromView:(NSView*)outView toView:(NSView*)inView {
  const NSTimeInterval kAnimationDuration = 0.175;

  NSDictionary* outDict = [NSDictionary dictionaryWithObjectsAndKeys:
      outView, NSViewAnimationTargetKey,
      NSViewAnimationFadeOutEffect, NSViewAnimationEffectKey,
      nil
  ];
  NSDictionary* inDict = [NSDictionary dictionaryWithObjectsAndKeys:
      inView, NSViewAnimationTargetKey,
      NSViewAnimationFadeInEffect, NSViewAnimationEffectKey,
      nil
  ];

  linkAnimation_.reset([[NSViewAnimation alloc] initWithViewAnimations:
      [NSArray arrayWithObjects:outDict, inDict, nil]]);
  [linkAnimation_ setDelegate:self];
  [linkAnimation_ setDuration:kAnimationDuration];

  [self willStartAnimation:linkAnimation_];

  [linkAnimation_ performSelector:@selector(startAnimation)
                       withObject:nil
                       afterDelay:0.2];
}

- (void)willStartAnimation:(NSAnimation*)animation {
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (animation == linkAnimation_.get())
    linkAnimation_.reset();
}

- (void)animationDidStop:(NSAnimation*)animation {
  if (animation == linkAnimation_.get())
    linkAnimation_.reset();
}

@end

// Profile Switch Button ///////////////////////////////////////////////////////

@implementation AvatarMenuItemView

@synthesize viewController = viewController_;

- (void)awakeFromNib {
  [self updateTrackingAreas];
}

- (void)updateTrackingAreas {
  if (trackingArea_.get())
    [self removeTrackingArea:trackingArea_.get()];

  trackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:[self bounds]
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil]);
  [self addTrackingArea:trackingArea_.get()];

  [super updateTrackingAreas];
}

- (void)mouseEntered:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
  [self setNeedsDisplay:YES];
}

- (void)mouseUp:(id)sender {
  [viewController_ switchToProfile:self];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = nil;
  if ([viewController_ isHighlighted]) {
    backgroundColor = [NSColor colorWithCalibratedRed:223.0/255
                                                green:238.0/255
                                                 blue:246.0/255
                                                alpha:1.0];
  } else {
    backgroundColor = [NSColor clearColor];
  }

  [backgroundColor set];
  [NSBezierPath fillRect:[self bounds]];
}

// Make sure the element is focusable for accessibility.
- (BOOL)canBecomeKeyView {
  return YES;
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* attributes =
      [[super accessibilityAttributeNames] mutableCopy];
  [attributes addObject:NSAccessibilityTitleAttribute];
  [attributes addObject:NSAccessibilityEnabledAttribute];

  return [attributes autorelease];
}

- (NSArray*)accessibilityActionNames {
  NSArray* parentActions = [super accessibilityActionNames];
  return [parentActions arrayByAddingObject:NSAccessibilityPressAction];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
    return NSAccessibilityButtonRole;

  if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute])
    return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil);

  if ([attribute isEqual:NSAccessibilityTitleAttribute]) {
    return l10n_util::GetNSStringF(
        IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME,
        base::SysNSStringToUTF16(self.viewController.nameField.stringValue));
  }

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:YES];

  return [super accessibilityAttributeValue:attribute];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqual:NSAccessibilityPressAction]) {
    [viewController_ switchToProfile:self];
    return;
  }

  [super accessibilityPerformAction:action];
}

@end

////////////////////////////////////////////////////////////////////////////////

@implementation AccessibilityIgnoredImageCell
- (BOOL)accessibilityIsIgnored {
  return YES;
}
@end

@implementation AccessibilityIgnoredTextFieldCell
- (BOOL)accessibilityIsIgnored {
  return YES;
}
@end
