// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"

#include <cmath>

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller_target.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMFadeTruncatingTextFieldCell.h"
#include "ui/base/l10n/l10n_util_mac.h"

@implementation TabController

@synthesize action = action_;
@synthesize app = app_;
@synthesize loadingState = loadingState_;
@synthesize mini = mini_;
@synthesize pinned = pinned_;
@synthesize projecting = projecting_;
@synthesize target = target_;
@synthesize url = url_;

namespace TabControllerInternal {

// A C++ delegate that handles enabling/disabling menu items and handling when
// a menu command is chosen. Also fixes up the menu item label for "pin/unpin
// tab".
class MenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(id<TabControllerTarget> target, TabController* owner)
      : target_(target),
        owner_(owner) {}

  // Overridden from ui::SimpleMenuModel::Delegate
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(command_id);
    return [target_ isCommandEnabled:command forController:owner_];
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE { return false; }
  virtual void ExecuteCommand(int command_id) OVERRIDE {
    TabStripModel::ContextMenuCommand command =
        static_cast<TabStripModel::ContextMenuCommand>(command_id);
    [target_ commandDispatch:command forController:owner_];
  }

 private:
  id<TabControllerTarget> target_;  // weak
  TabController* owner_;  // weak, owns me
};

}  // TabControllerInternal namespace

// The min widths is the smallest number at which the right edge of the right
// tab border image is not visibly clipped.  It is a bit smaller than the sum
// of the two tab edge bitmaps because these bitmaps have a few transparent
// pixels on the side.  The selected tab width includes the close button width.
+ (CGFloat)minTabWidth { return 36; }
+ (CGFloat)minSelectedTabWidth { return 52; }
+ (CGFloat)maxTabWidth { return 214; }
+ (CGFloat)miniTabWidth { return 58; }
+ (CGFloat)appTabWidth { return 66; }

- (TabView*)tabView {
  DCHECK([[self view] isKindOfClass:[TabView class]]);
  return static_cast<TabView*>([self view]);
}

- (id)init {
  if ((self = [super init])) {
    // Icon.
    // Remember the icon's frame, so that if the icon is ever removed, a new
    // one can later replace it in the proper location.
    originalIconFrame_ = NSMakeRect(19, 5, 16, 16);
    iconView_.reset([[NSImageView alloc] initWithFrame:originalIconFrame_]);
    [iconView_ setAutoresizingMask:NSViewMaxXMargin];

    // When the icon is removed, the title expands to the left to fill the
    // space left by the icon.  When the close button is removed, the title
    // expands to the right to fill its space.  These are the amounts to expand
    // and contract titleView_ under those conditions. We don't have to
    // explicilty save the offset between the title and the close button since
    // we can just get that value for the close button's frame.
    NSRect titleFrame = NSMakeRect(35, 6, 92, 14);
    iconTitleXOffset_ = NSMinX(titleFrame) - NSMinX(originalIconFrame_);

    // Label.
    titleView_.reset([[NSTextField alloc] initWithFrame:titleFrame]);
    [titleView_ setAutoresizingMask:NSViewWidthSizable];
    scoped_nsobject<GTMFadeTruncatingTextFieldCell> labelCell(
        [[GTMFadeTruncatingTextFieldCell alloc] initTextCell:@"Label"]);
    [labelCell setControlSize:NSSmallControlSize];
    CGFloat fontSize = [NSFont systemFontSizeForControlSize:NSSmallControlSize];
    NSFont* font = [NSFont fontWithName:
        [[labelCell font] fontName] size:fontSize];
    [labelCell setFont:font];
    [titleView_ setCell:labelCell];

    // Close button.
    closeButton_.reset([[HoverCloseButton alloc] initWithFrame:
        NSMakeRect(127, 4, 18, 18)]);
    [closeButton_ setAutoresizingMask:NSViewMinXMargin];
    [closeButton_ setTarget:self];
    [closeButton_ setAction:@selector(closeTab:)];

    scoped_nsobject<TabView> view([[TabView alloc] initWithFrame:
        NSMakeRect(0, 0, 160, 25) controller:self closeButton:closeButton_]);
    [view setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
    [view addSubview:iconView_];
    [view addSubview:titleView_];
    [view addSubview:closeButton_];
    [super setView:view];

    isIconShowing_ = YES;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeChangedNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];

    [self internalSetSelected:selected_];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[self tabView] setController:nil];
  [super dealloc];
}

// The internals of |-setSelected:| and |-setActive:| but doesn't set the
// backing variables. This updates the drawing state and marks self as needing
// a re-draw.
- (void)internalSetSelected:(BOOL)selected {
  TabView* tabView = [self tabView];
  DCHECK([tabView isKindOfClass:[TabView class]]);
  [tabView setState:selected];
  if ([self active])
    [tabView cancelAlert];
  [self updateVisibility];
  [self updateTitleColor];
}

// Called when Cocoa wants to display the context menu. Lazily instantiate
// the menu based off of the cross-platform model. Re-create the menu and
// model every time to get the correct labels and enabling.
- (NSMenu*)menu {
  contextMenuDelegate_.reset(
      new TabControllerInternal::MenuDelegate(target_, self));
  contextMenuModel_.reset(
      [target_ contextMenuModelForController:self
                                menuDelegate:contextMenuDelegate_.get()]);
  contextMenuController_.reset(
      [[MenuController alloc] initWithModel:contextMenuModel_.get()
                     useWithPopUpButtonCell:NO]);
  return [contextMenuController_ menu];
}

- (void)closeTab:(id)sender {
  if ([[self target] respondsToSelector:@selector(closeTab:)]) {
    [[self target] performSelector:@selector(closeTab:)
                        withObject:[self view]];
  }
}

- (void)selectTab:(id)sender {
  if ([[self tabView] isClosing])
    return;
  if ([[self target] respondsToSelector:[self action]]) {
    [[self target] performSelector:[self action]
                        withObject:[self view]];
  }
}

- (void)setTitle:(NSString*)title {
  [titleView_ setStringValue:title];
  [[self view] setToolTip:title];
  if ([self mini] && ![self selected]) {
    TabView* tabView = static_cast<TabView*>([self view]);
    DCHECK([tabView isKindOfClass:[TabView class]]);
    [tabView startAlert];
  }
  [super setTitle:title];
}

- (void)setActive:(BOOL)active {
  if (active != active_) {
    active_ = active;
    [self internalSetSelected:[self selected]];
  }
}

- (BOOL)active {
  return active_;
}

- (void)setSelected:(BOOL)selected {
  if (selected_ != selected) {
    selected_ = selected;
    [self internalSetSelected:[self selected]];
  }
}

- (BOOL)selected {
  return selected_ || active_;
}

- (NSView*)iconView {
  return iconView_;
}

- (void)setIconView:(NSView*)iconView {
  [iconView_ removeFromSuperview];
  iconView_.reset([iconView retain]);
  if ([self projecting] && [self loadingState] == kTabDone) {
    // When projecting we have bigger iconView to accommodate the glow
    // animation, so this frame should be double the size of a favicon.
    NSRect iconFrame = [iconView frame];

    // Center the iconView given it's double regular size.
    if ([self app] || [self mini]) {
      const CGFloat tabWidth = [self app] ? [TabController appTabWidth]
                                          : [TabController miniTabWidth];
      iconFrame.origin.x = std::floor((tabWidth - NSWidth(iconFrame)) / 2.0);
    } else {
      iconFrame.origin.x = std::floor(originalIconFrame_.origin.x / 2.0);
    }

    iconFrame.origin.y = -std::ceil(originalIconFrame_.origin.y / 2.0);

    [iconView_ setFrame:iconFrame];
  } else if ([self app] || [self mini]) {
    NSRect appIconFrame = [iconView frame];
    appIconFrame.origin = originalIconFrame_.origin;

    const CGFloat tabWidth = [self app] ? [TabController appTabWidth]
                                        : [TabController miniTabWidth];

    // Center the icon.
    appIconFrame.origin.x =
        std::floor((tabWidth - NSWidth(appIconFrame)) / 2.0);
    [iconView_ setFrame:appIconFrame];
  } else {
    [iconView_ setFrame:originalIconFrame_];
  }
  // Ensure that the icon is suppressed if no icon is set or if the tab is too
  // narrow to display one.
  [self updateVisibility];

  if (iconView_)
    [[self view] addSubview:iconView_];
}

- (NSTextField*)titleView {
  return titleView_;
}

- (HoverCloseButton*)closeButton {
  return closeButton_;
}

- (NSString*)toolTip {
  return [[self tabView] toolTipText];
}

// Return a rough approximation of the number of icons we could fit in the
// tab. We never actually do this, but it's a helpful guide for determining
// how much space we have available.
- (int)iconCapacity {
  CGFloat width = NSMaxX([closeButton_ frame]) - NSMinX(originalIconFrame_);
  CGFloat iconWidth = NSWidth(originalIconFrame_);

  return width / iconWidth;
}

// Returns YES if we should show the icon. When tabs get too small, we clip
// the favicon before the close button for selected tabs, and prefer the
// favicon for unselected tabs.  The icon can also be suppressed more directly
// by clearing iconView_.
- (BOOL)shouldShowIcon {
  if (!iconView_)
    return NO;

  if ([self mini])
    return YES;

  int iconCapacity = [self iconCapacity];
  if ([self selected])
    return iconCapacity >= 2;
  return iconCapacity >= 1;
}

// Returns YES if we should be showing the close button. The selected tab
// always shows the close button.
- (BOOL)shouldShowCloseButton {
  if ([self mini])
    return NO;
  return ([self selected] || [self iconCapacity] >= 3);
}

- (void)updateVisibility {
  // iconView_ may have been replaced or it may be nil, so [iconView_ isHidden]
  // won't work.  Instead, the state of the icon is tracked separately in
  // isIconShowing_.
  BOOL newShowIcon = [self shouldShowIcon];

  [iconView_ setHidden:!newShowIcon];
  isIconShowing_ = newShowIcon;

  // If the tab is a mini-tab, hide the title.
  [titleView_ setHidden:[self mini]];

  BOOL newShowCloseButton = [self shouldShowCloseButton];

  [closeButton_ setHidden:!newShowCloseButton];

  // Adjust the title view based on changes to the icon's and close button's
  // visibility.
  NSRect oldTitleFrame = [titleView_ frame];
  NSRect newTitleFrame;
  newTitleFrame.size.height = oldTitleFrame.size.height;
  newTitleFrame.origin.y = oldTitleFrame.origin.y;

  if (newShowIcon) {
    newTitleFrame.origin.x = originalIconFrame_.origin.x + iconTitleXOffset_;
  } else {
    newTitleFrame.origin.x = originalIconFrame_.origin.x;
  }

  if (newShowCloseButton) {
    newTitleFrame.size.width = NSMinX([closeButton_ frame]) -
                               newTitleFrame.origin.x;
  } else {
    newTitleFrame.size.width = NSMaxX([closeButton_ frame]) -
                               newTitleFrame.origin.x;
  }

  [titleView_ setFrame:newTitleFrame];
}

- (void)updateTitleColor {
  NSColor* titleColor = nil;
  ui::ThemeProvider* theme = [[[self view] window] themeProvider];
  if (theme && ![self selected]) {
    titleColor =
        theme->GetNSColor(ThemeProperties::COLOR_BACKGROUND_TAB_TEXT,
                          true);
  }
  // Default to the selected text color unless told otherwise.
  if (theme && !titleColor) {
    titleColor = theme->GetNSColor(ThemeProperties::COLOR_TAB_TEXT,
                                   true);
  }
  [titleView_ setTextColor:titleColor ? titleColor : [NSColor textColor]];
}

- (void)themeChangedNotification:(NSNotification*)notification {
  [self updateTitleColor];
}

// Called by the tabs to determine whether we are in rapid (tab) closure mode.
- (BOOL)inRapidClosureMode {
  if ([[self target] respondsToSelector:@selector(inRapidClosureMode)]) {
    return [[self target] performSelector:@selector(inRapidClosureMode)] ?
        YES : NO;
  }
  return NO;
}

// The following methods are invoked from the TabView and are forwarded to the
// TabStripDragController.
- (BOOL)tabCanBeDragged:(TabController*)controller {
  return [[target_ dragController] tabCanBeDragged:controller];
}

- (void)maybeStartDrag:(NSEvent*)event forTab:(TabController*)tab {
  [[target_ dragController] maybeStartDrag:event forTab:tab];
}

@end
