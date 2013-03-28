// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/action_box_menu_bubble_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/native_theme/native_theme.h"

@interface ActionBoxMenuBubbleController (Private)
- (id)highlightedItem;
- (void)keyDown:(NSEvent*)theEvent;
- (void)moveDown:(id)sender;
- (void)moveUp:(id)sender;
- (void)highlightNextItemByDelta:(NSInteger)delta;
- (void)highlightItem:(ActionBoxMenuItemController*)newItem;
@end

@interface ActionBoxMenuItemView (Private)
- (NSColor*)highlightedMenuItemBackgroundColor;
@end

namespace {

// Some reasonable values for the menu geometry.
const CGFloat kBubbleMinWidth = 175;
const CGFloat kBubbleMaxWidth = 800;

// Distance between the top/bottom of the bubble and the first/last menu item.
const CGFloat kVerticalPadding = 7.0;

// Minimum distance between the right of a menu item and the right border.
const CGFloat kRightMargin = 20.0;

// Alpha of the black rectangle overlayed on the item hovered over.
const CGFloat kSelectionAlpha = 0.06;

}  // namespace

// extension Icon Loader Bridge ////////////////////////////////////////////////

class ExtensionIconLoaderBridge : public extensions::IconImage::Observer {
 public:
  ExtensionIconLoaderBridge(Profile* profile,
                            const extensions::Extension* extension,
                            ActionBoxMenuItemController* controller)
      : controller_(controller) {
    const extensions::ActionInfo* page_launcher_info =
        extensions::ActionInfo::GetPageLauncherInfo(extension);
    DCHECK(page_launcher_info);
    icon_loader_.reset(new extensions::IconImage(
        profile,
        extension,
        page_launcher_info->default_icon,
        extension_misc::EXTENSION_ICON_ACTION,
        extensions::IconsInfo::GetDefaultAppIcon(),
        this));
    OnExtensionIconImageChanged(icon_loader_.get());
  }

 private:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE {
    [controller_ onExtensionIconImageChanged:
        gfx::NSImageFromImageSkia(image->image_skia())];
  }

  scoped_ptr<extensions::IconImage> icon_loader_;
  ActionBoxMenuItemController* controller_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconLoaderBridge);
};

@implementation ActionBoxMenuBubbleController

- (id)initWithModel:(scoped_ptr<ActionBoxMenuModel>)model
       parentWindow:(NSWindow*)parent
         anchoredAt:(NSPoint)point
            profile:(Profile*)profile {
  // Use an arbitrary height because it will reflect the size of the content.
  NSRect contentRect = NSMakeRect(0, 0, kBubbleMinWidth, 150);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];
  if (self = [super initWithWindow:window
                      parentWindow:parent
                        anchoredAt:point]) {
    profile_ = profile;
    model_.reset(model.release());

    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    [[self bubble] setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground))];
    [self performLayout];
  }
  return self;
}

- (ActionBoxMenuModel*)model {
  return model_.get();
}

- (NSMutableArray*)items {
  return items_;
}

- (IBAction)itemSelected:(id)sender {
  // If Enter is pressed but nothing is highlighted, don't activate anything.
  if (!sender)
    return;

  // Close the current window and activate the parent browser window, otherwise
  // the bookmark popup refuses to show.
  [self close];
  [BrowserWindowUtils
      activateWindowForController:[[self parentWindow] windowController]];
  size_t modelIndex = [sender modelIndex];
  DCHECK(model_.get());
  int eventFlags = event_utils::EventFlagsFromNSEvent([NSApp currentEvent]);
  model_->ActivatedAt(modelIndex, eventFlags);
}

// Private /////////////////////////////////////////////////////////////////////

- (void)performLayout {
  NSView* contentView = [[self window] contentView];

  // Reset the array of controllers and remove all the views.
  items_.reset([[NSMutableArray alloc] init]);
  [contentView setSubviews:[NSArray array]];

  // Leave some space at the bottom of the menu.
  CGFloat yOffset = kVerticalPadding;

  // Keep track of a potential separator to resize it when we know the width.
  scoped_nsobject<NSBox> separatorView;

  // Loop over the items in reverse, constructing the menu items.
  CGFloat width = kBubbleMinWidth;
  CGFloat minX = NSMinX([contentView bounds]);
  for (int i = model_->GetItemCount() - 1; i >= 0; --i) {
    if (model_->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      const CGFloat kSeparatorHeight = 1.0;
      // Only supports one separator.
      DCHECK(!separatorView);
      yOffset += kVerticalPadding + kSeparatorHeight;
      separatorView.reset([[NSBox alloc]
          initWithFrame:NSMakeRect(0, yOffset, width, kSeparatorHeight)]);
      [separatorView setBoxType:NSBoxCustom];
      ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
      [separatorView setBorderColor:
          gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
              ui::NativeTheme::kColorId_MenuSeparatorColor))];
      [contentView addSubview:separatorView];
      yOffset += kVerticalPadding;
    } else {
      // Create the item controller. Autorelease it because it will be owned
      // by the |items_| array.
      scoped_nsobject<ActionBoxMenuItemController> itemController(
          [[ActionBoxMenuItemController alloc]
              initWithModelIndex:i
                  menuController:self
                         profile:profile_]);

      // Adjust the name field to fit the string.
      [GTMUILocalizerAndLayoutTweaker sizeToFitView:[itemController nameField]];

      // Expand the size of the window if required to fit the menu item.
      width = std::max(width,
          NSMaxX([[itemController nameField] frame]) - minX + kRightMargin);

      // Add the item to the content view.
      [[itemController view] setFrameOrigin:NSMakePoint(0, yOffset)];
      [contentView addSubview:[itemController view]];
      yOffset += NSHeight([[itemController view] frame]);

      // Keep track of the view controller.
      [items_ addObject:itemController.get()];
    }
  }

  // Leave some space at the top of the menu.
  yOffset += kVerticalPadding;

  // Set the window frame, clamping the width at a sensible max.
  NSRect frame = [[self window] frame];
  frame.size.height = yOffset;
  frame.size.width = std::min(width, kBubbleMaxWidth);

  // Resize the separator to full width.
  if (separatorView) {
    NSRect separatorFrame = [separatorView frame];
    separatorFrame.size.width = width;
    [separatorView setFrame:separatorFrame];
  }

  [[self window] setFrame:frame display:YES];
}

- (id)highlightedItem {
  for (ActionBoxMenuItemController* item in items_.get()) {
    if ([item isHighlighted]) {
      return item;
    }
  }
  return nil;
}

- (void)keyDown:(NSEvent*)theEvent {
  // Interpret all possible key events. In particular, this will answer
  // moveDown, moveUp and insertNewline so that the menu can be navigated
  // with keystrokes.
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
}

- (void)moveDown:(id)sender {
  [self highlightNextItemByDelta:-1];
}

- (void)moveUp:(id)sender {
  [self highlightNextItemByDelta:1];
}

- (void)insertNewline:(id)sender {
  [self itemSelected:[self highlightedItem]];
}

- (void)highlightNextItemByDelta:(NSInteger)delta {
  NSUInteger count = [items_ count];
  if (count == 0)
    return;

  // If nothing is selected, select the first (resp. last) item when going up
  // (resp. going down). Otherwise selects the next (resp. previous) item.
  // This code does not wrap around if something is already selected.

  // First assumes nothing is selected.
  NSUInteger newIndex = delta < 0 ? (count - 1) : 0;
  NSUInteger oldIndex = [items_ indexOfObject:[self highlightedItem]];
  // oldIndex will be NSNotFound when [self highlightedItem] returns nil.
  if (oldIndex != NSNotFound) {
    // Something is selected, move only if not already at top/bottom.
    newIndex = oldIndex;
    if (newIndex != (delta < 0 ? 0 : count - 1))
      newIndex += delta;
  }

  [self highlightItem:[items_ objectAtIndex:newIndex]];
}

- (void)highlightItem:(ActionBoxMenuItemController*)newItem {
  ActionBoxMenuItemController* oldItem = [self highlightedItem];
  if (oldItem == newItem)
    return;
  [oldItem setIsHighlighted:NO];
  [newItem setIsHighlighted:YES];
}

@end

// Menu Item Controller ////////////////////////////////////////////////////////

@implementation ActionBoxMenuItemController

@synthesize modelIndex = modelIndex_;
@synthesize isHighlighted = isHighlighted_;
@synthesize iconView = iconView_;
@synthesize nameField = nameField_;

- (id)initWithModelIndex:(size_t)modelIndex
          menuController:(ActionBoxMenuBubbleController*)controller
                profile:(Profile*)profile {
  if ((self = [super initWithNibName:@"ActionBoxMenuItem"
                              bundle:base::mac::FrameworkBundle()])) {
    modelIndex_ = modelIndex;
    controller_ = controller;

    [self loadView];

    gfx::Image icon = gfx::Image();
    ActionBoxMenuModel* model = [controller model];
    if (model->GetIconAt(modelIndex_, &icon)) {
      extensionIconLoaderBridge_.reset();
      [iconView_ setImage:icon.ToNSImage()];
    } else if (model->GetTypeAt(modelIndex_) == ui::MenuModel::TYPE_COMMAND &&
               model->IsItemExtension(modelIndex_)) {
      // Creating an ExtensionIconLoaderBridge will call
      // onExtensionIconImageChanged and set the |iconView_|.
      [iconView_ setImage:nil];
      extensionIconLoaderBridge_.reset(new ExtensionIconLoaderBridge(
          profile,
          model->GetExtensionAt(modelIndex_),
          self));
    } else {
      extensionIconLoaderBridge_.reset();
      [iconView_ setImage:nil];
    }

    nameField_.stringValue = base::SysUTF16ToNSString(
        controller.model->GetLabelAt(modelIndex_));
  }
  return self;
}

- (void)dealloc {
  base::mac::ObjCCastStrict<ActionBoxMenuItemView>(
      self.view).viewController = nil;
  [super dealloc];
}

- (void)highlightForEvent:(NSEvent*)event {
  switch ([event type]) {
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

- (IBAction)itemSelected:(id)sender {
  [controller_ itemSelected:self];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (isHighlighted_ == isHighlighted)
    return;

  isHighlighted_ = isHighlighted;
  [[self view] setNeedsDisplay:YES];
}

- (void)onExtensionIconImageChanged:(NSImage*)image {
  [iconView_ setImage:image];
}

@end

// Items from the action box menu //////////////////////////////////////////////

@implementation ActionBoxMenuItemView

@synthesize viewController = viewController_;

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

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)mouseUp:(NSEvent*)theEvent {
  [viewController_ itemSelected:self];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [viewController_ highlightForEvent:theEvent];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [viewController_ highlightForEvent:theEvent];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = nil;
  if ([viewController_ isHighlighted]) {
    ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
    backgroundColor = gfx::SkColorToCalibratedNSColor(
        nativeTheme->GetSystemColor(
            ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
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
      [[[super accessibilityAttributeNames] mutableCopy] autorelease];
  [attributes addObject:NSAccessibilityTitleAttribute];
  [attributes addObject:NSAccessibilityEnabledAttribute];

  return attributes;
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

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:YES];

  return [super accessibilityAttributeValue:attribute];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqual:NSAccessibilityPressAction]) {
    [viewController_ itemSelected:self];
    return;
  }

  [super accessibilityPerformAction:action];
}

- (NSColor*)highlightedMenuItemBackgroundColor {
  ui::NativeTheme* nativeTheme = ui::NativeTheme::instance();
  return gfx::SkColorToCalibratedNSColor(nativeTheme->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
}

@end
