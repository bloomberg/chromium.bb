// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/hover_image_button.h"
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
- (void)configureEditButton:(HoverImageButton*)button;
@end

namespace AvatarMenuInternal {

class Bridge : public AvatarMenuModelObserver {
 public:
  Bridge(AvatarMenuBubbleController* controller) : controller_(controller) {}

  // AvatarMenuModelObserver:
  void OnAvatarMenuModelChanged(AvatarMenuModel* model) {
    [controller_ performLayout];
  }

 private:
  AvatarMenuBubbleController* controller_;
};

}  // namespace AvatarMenuInternal

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

  AvatarMenuInternal::Bridge* bridge = new AvatarMenuInternal::Bridge(self);
  AvatarMenuModel* model = new AvatarMenuModel(
        &g_browser_process->profile_manager()->GetProfileInfo(),
        bridge, parentBrowser);

  if ((self = [self initWithModel:model
                           bridge:bridge
                     parentWindow:parentBrowser->window()->GetNativeHandle()
                       anchoredAt:point])) {
  }
  return self;
}

- (IBAction)newProfile:(id)sender {
  model_->AddNewProfile();
}

- (IBAction)switchToProfile:(id)sender {
  model_->SwitchToProfile([sender modelIndex]);
}

- (IBAction)editProfile:(id)sender {
  model_->EditProfile([sender modelIndex]);
}

// Private /////////////////////////////////////////////////////////////////////

- (id)initWithModel:(AvatarMenuModel*)model
             bridge:(AvatarMenuModelObserver*)bridge
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
    bridge_.reset(bridge);
    model_.reset(model);
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
  yOffset += NSHeight([newButton frame]) + (kLinkSpacing - kVerticalSpacing);

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

    if (!item.active) {
      // In the inactive case, hide additional UI.
      [itemView.activeView setHidden:YES];
      [itemView.editButton setHidden:YES];
    } else {
      // Otherwise, set up the edit button and its three interaction states.
      [self configureEditButton:itemView.editButton];
      itemView.activeView.image =
          rb.GetImageNamed(IDR_PROFILE_SELECTED).ToNSImage();
    }

    // Force a highlight of the default state to get the text colored correctly.
    [itemView highlightForEventType:NSLeftMouseUp];

    // Add the item to the content view.
    [[itemView view] setFrameOrigin:NSMakePoint(0, yOffset)];
    [contentView addSubview:[itemView view]];
    yOffset += NSHeight([[itemView view] frame]);

    // Keep track of the view controller.
    [items_ addObject:itemView];
  }

  yOffset += kVerticalSpacing;

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

- (void)configureEditButton:(HoverImageButton*)button {
  [button setTrackingEnabled:YES];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  [button setDefaultImage:rb.GetImageNamed(IDR_PROFILE_EDIT).ToNSImage()];
  [button setDefaultOpacity:1.0];

  [button setHoverImage:rb.GetImageNamed(IDR_PROFILE_EDIT_HOVER).ToNSImage()];
  [button setHoverOpacity:1.0];

  [button setPressedImage:
      rb.GetImageNamed(IDR_PROFILE_EDIT_PRESSED).ToNSImage()];
  [button setPressedOpacity:1.0];

  [[button cell] setHighlightsBy:NSNoCellMask];
}

- (NSMutableArray*)items {
  return items_.get();
}

@end

// Menu Item Controller ////////////////////////////////////////////////////////

@implementation AvatarMenuItemController

@synthesize modelIndex = modelIndex_;
@synthesize iconView = iconView_;
@synthesize activeView = activeView_;
@synthesize nameField = nameField_;
@synthesize editButton = editButton_;

- (id)initWithModelIndex:(size_t)modelIndex
          menuController:(AvatarMenuBubbleController*)controller {
  if ((self = [super initWithNibName:@"AvatarMenuItem"
                              bundle:base::mac::MainAppBundle()])) {
    modelIndex_ = modelIndex;
    controller_ = controller;
    [self loadView];
  }
  return self;
}

- (IBAction)switchToProfile:(id)sender {
  [controller_ switchToProfile:self];
}

- (IBAction)editProfile:(id)sender {
  [controller_ editProfile:self];
}

- (void)highlightForEventType:(NSEventType)type {
  BOOL active = !self.activeView.isHidden;
  NSColor* color = nil;
  switch (type) {
    case NSMouseEntered:
    case NSLeftMouseDown:
      if (active) {
        color = [NSColor colorWithCalibratedRed:0 green:0 blue:0 alpha:1];
      } else {
        color = [NSColor colorWithCalibratedRed:0.25
                                          green:0.25
                                           blue:0.25
                                          alpha:1];
      }
      break;

    case NSMouseExited:
    case NSLeftMouseUp:
      if (active) {
        color = [NSColor colorWithCalibratedRed:0.12
                                          green:0.12
                                           blue:0.12
                                          alpha:1];
      } else {
        color = [NSColor colorWithCalibratedRed:0.5
                                          green:0.5
                                           blue:0.5
                                          alpha:1];
      }
      break;

    default:
      NOTREACHED();
  };

  DCHECK(color);
  self.nameField.textColor = color;
}

@end

// Profile Switch Button ///////////////////////////////////////////////////////

@implementation SwitchProfileButtonCell

@synthesize viewController = viewController_;

- (void)awakeFromNib {
  // Needed to get entered and exited events.
  self.showsBorderOnlyWhileMouseInside = YES;
}

- (void)mouseEntered:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
}

- (void)mouseExited:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
}

- (void)mouseDown:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
}

- (void)mouseUp:(id)sender {
  [viewController_ highlightForEventType:[[NSApp currentEvent] type]];
}

@end
