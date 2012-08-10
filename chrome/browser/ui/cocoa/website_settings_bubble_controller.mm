// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings_bubble_controller.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/cert_store.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// The width of the window, in view coordinates. The height will be determined
// by the content.
const CGFloat kWindowWidth = 380;

// Spacing in between sections.
const CGFloat kVerticalSpacing = 10;

// Padding between the window frame and content.
const CGFloat kFramePadding = 10;

// Spacing between the optional headline and description text views.
const CGFloat kHeadlineSpacing = 2;

// Spacing between the image and the text.
const CGFloat kImageSpacing = 10;

// Square size of the image.
const CGFloat kImageSize = 30;

// The X position of the text fields. Variants for with and without an image.
const CGFloat kTextXPositionNoImage = kFramePadding;
const CGFloat kTextXPosition = kTextXPositionNoImage + kImageSize +
    kImageSpacing;

// Width of the text fields.
const CGFloat kTextWidth = kWindowWidth - (kImageSize + kImageSpacing +
    kFramePadding * 2);

// The amount of padding given to tab view contents.
const CGFloat kTabViewContentsPadding = kFramePadding;

// The spacing between individual items in the Permissions tab.
const CGFloat kPermissionsTabSpacing = 8;

// The extra space to the left of the first tab in the tab strip.
const CGFloat kTabStripXPadding = 19;

// The amount of space between the visual borders of adjacent tabs.
const CGFloat kTabSpacing = 4;

// The extra width outside the hit rect used by the visual borders of the tab.
const CGFloat kTabBorderExtraWidth = 16;

// The height of the clickable area of the tab strip.
const CGFloat kTabHeight = 27;

// The height of the background image for the tab strip.
const CGFloat kTabStripHeight = 44;

// The amount of space above tab labels.
const CGFloat kTabLabelTopPadding = 20;

// The amount of padding to leave on either side of the tab label.
const CGFloat kTabLabelXPadding = 12;

// In the permission changing menu, the order of the menu items (which
// correspond to different content settings).
const ContentSetting kPermissionsMenuSettings[] = {
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_DEFAULT
};

}  // namespace

// A simple container to hold all the contents of the website settings bubble.
// It uses a flipped coordinate system to make text layout easier.
@interface WebsiteSettingsContentView : NSView
@end
@implementation WebsiteSettingsContentView
- (BOOL)isFlipped {
  return YES;
}
@end

@interface WebsiteSettingsTabSegmentedCell : NSSegmentedCell {
 @private
  scoped_nsobject<NSImage> tabBackgroundImage_;
  scoped_nsobject<NSImage> tabCenterImage_;
  scoped_nsobject<NSImage> tabLeftImage_;
  scoped_nsobject<NSImage> tabRightImage_;
}
@end

@implementation WebsiteSettingsTabSegmentedCell
- (id)init {
  if ((self = [super init])) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    tabBackgroundImage_.reset(
        [rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_BACKGROUND) retain]);
    tabCenterImage_.reset(
        [rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_CENTER) retain]);
    tabLeftImage_.reset(
        [rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_LEFT) retain]);
    tabRightImage_.reset(
        [rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_RIGHT) retain]);
  }
  return self;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Draw the tab for the selected segment. The drawing area is slightly
  // larger than the hit rect for the tab.
  NSRect tabRect = [self hitRectForSegment:[self selectedSegment]];
  tabRect.origin.x -= kTabBorderExtraWidth;
  tabRect.size.width += 2 * kTabBorderExtraWidth;

  tabRect.origin.y = 0;
  tabRect.size.height = kTabStripHeight;

  NSDrawThreePartImage(tabRect,
                       tabLeftImage_,
                       tabCenterImage_,
                       tabRightImage_,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Draw the background to the left of the selected tab.
  NSRect backgroundRect = NSMakeRect(0, 0, NSMinX(tabRect), kTabStripHeight);
  NSDrawThreePartImage(backgroundRect,
                       nil,
                       tabBackgroundImage_,
                       nil,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Draw the background to the right of the selected tab.
  backgroundRect.origin.x = NSMaxX(tabRect);
  backgroundRect.size.width = kWindowWidth - NSMaxX(tabRect);
  NSDrawThreePartImage(backgroundRect,
                       nil,
                       tabBackgroundImage_,
                       nil,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Call the superclass method to trigger drawing of the tab labels.
  [self drawInteriorWithFrame:cellFrame inView:controlView];
}

// Return the hit rect (i.e., the visual bounds of the tab) for
// the given segment.
- (NSRect)hitRectForSegment:(NSInteger)segment {
  NSRect rect = NSMakeRect(0, kTabStripHeight - kTabHeight,
                           [self widthForSegment:segment], kTabHeight);
  for (NSInteger i = 0; i < segment; ++i) {
    rect.origin.x += [self widthForSegment:i];
  }
  int xAdjust = segment == 0 ? kTabStripXPadding : 0;
  rect.size.width = [self widthForSegment:segment] - kTabSpacing - xAdjust;
  rect.origin.x += kTabSpacing / 2 + xAdjust;

  return rect;
}

- (void)drawSegment:(NSInteger)segment
            inFrame:(NSRect)frame
           withView:(NSView*)controlView {
  // Call the superclass to draw the label, adjusting the rectangle so that
  // the label appears centered in the tab.
  if (segment == 0) {
    frame.origin.x += kTabStripXPadding / 2;
    frame.size.width -= kTabStripXPadding;
  }
  frame.origin.y += kTabLabelTopPadding;
  frame.size.height -= kTabLabelTopPadding;
  [super drawSegment:segment inFrame:frame withView:controlView];
}

// Overrides the default tracking behavior to only respond to clicks inside the
// visual borders of the tab.
- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView {
  NSInteger segmentCount = [self segmentCount];
  for (NSInteger i = 0; i < segmentCount; ++i) {
    if (NSPointInRect(startPoint, [self hitRectForSegment:i]))
      return YES;
  }
  return NO;
}

// Overrides the default cell height to take up the full height of the
// segmented control. Otherwise, cliks on the lower part of a tab will be
// ignored.
- (NSSize)cellSizeForBounds:(NSRect)aRect {
  return NSMakeSize([super cellSizeForBounds:aRect].width, NSHeight(aRect));
}
@end

@implementation WebsiteSettingsBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
   websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge {
  DCHECK(parentWindow);

  // Use an arbitrary height; it will be changed in performLayout.
  NSRect contentRect = NSMakeRect(0, 0, kWindowWidth, 1);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);

  if ((self = [super initWithWindow:window.get()
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    [self initializeContents];

    bridge_.reset(bridge);
    bridge_->set_bubble_controller(self);
  }
  return self;
}

- (void)setPresenter:(WebsiteSettings*)presenter {
  presenter_.reset(presenter);
}

// Create the subviews for the website settings bubble.
- (void)initializeContents {
  // Keeps track of the position that the next control should be drawn at.
  NSPoint controlOrigin = NSMakePoint(
      kTextXPositionNoImage,
      kFramePadding + info_bubble::kBubbleArrowHeight);

  // Create the container view that uses flipped coordinates.
  NSRect contentFrame = NSMakeRect(0, 0, kWindowWidth, 300);
  contentView_.reset(
      [[WebsiteSettingsContentView alloc] initWithFrame:contentFrame]);

  // Create a text field (empty for now) to show the site identity.
  identityField_ = [self addText:string16()
                        withSize:[NSFont systemFontSize]
                            bold:YES
                          toView:contentView_
                         atPoint:controlOrigin];
  controlOrigin.y += NSHeight([identityField_ frame]) + kHeadlineSpacing;

  // Create a text field to identity status (e.g. verified, not verified).
  identityStatusField_ = [self addText:string16()
                              withSize:[NSFont smallSystemFontSize]
                                  bold:NO
                                toView:contentView_
                               atPoint:controlOrigin];

  // Create the tab view and its two tabs.

  NSRect initialFrame = NSMakeRect(0, 0, kWindowWidth, kTabStripHeight);
  segmentedControl_.reset(
      [[NSSegmentedControl alloc] initWithFrame:initialFrame]);
  [segmentedControl_ setCell:
      [[[WebsiteSettingsTabSegmentedCell alloc] init] autorelease]];
  [segmentedControl_ setSegmentCount:2];
  [segmentedControl_ setTarget:self];
  [segmentedControl_ setAction:@selector(tabSelected:)];

  NSFont* smallSystemFont =
      [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  NSDictionary* textAttributes =
      [NSDictionary dictionaryWithObject:smallSystemFont
                                  forKey:NSFontAttributeName];

  // Create the "Permissions" tab.
  NSString* label = l10n_util::GetNSString(
      IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS);
  NSSize textSize = [label sizeWithAttributes:textAttributes];
  CGFloat tabWidth = textSize.width + 2 * kTabLabelXPadding;
  [segmentedControl_ setLabel:label forSegment:0];
  [segmentedControl_ setWidth:tabWidth + kTabStripXPadding forSegment:0];

  // Create the "Connection" tab.
  label = l10n_util::GetNSString(IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION);
  textSize = [label sizeWithAttributes:textAttributes];
  [segmentedControl_ setLabel:label forSegment:1];

  // Make both tabs the width of the widest. The first segment has some
  // additional padding that is not part of the tab, which is used for drawing
  // the background of the tab strip.
  tabWidth = std::max(tabWidth,
                      textSize.width + 2 * kTabLabelXPadding);
  [segmentedControl_ setWidth:tabWidth + kTabStripXPadding forSegment:0];
  [segmentedControl_ setWidth:tabWidth forSegment:1];

  [segmentedControl_ setFont:smallSystemFont];
  [segmentedControl_ setSelectedSegment:0];
  [contentView_ addSubview:segmentedControl_];

  NSRect tabFrame = NSMakeRect(0, 0, kWindowWidth, 300);
  tabView_.reset([[NSTabView alloc] initWithFrame:tabFrame]);
  [tabView_ setTabViewType:NSNoTabsNoBorder];
  [tabView_ setDrawsBackground:NO];
  [tabView_ setControlSize:NSSmallControlSize];
  [contentView_ addSubview:tabView_.get()];

  permissionsContentView_ = [self addPermissionsTabToTabView:tabView_];
  [self addConnectionTabToTabView:tabView_];

  // Replace the window's content.
  [[[self window] contentView] setSubviews:
      [NSArray arrayWithObject:contentView_.get()]];

  [self performLayout];
}

// Create the contents of the Permissions tab and add it to the given tab view.
// Returns a weak reference to the tab view item's view.
- (NSView*)addPermissionsTabToTabView:(NSTabView*)tabView {
  scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  [tabView_ addTabViewItem:item.get()];
  scoped_nsobject<NSView> contentView([[WebsiteSettingsContentView alloc]
      initWithFrame:[tabView_ contentRect]]);
  [item setView:contentView.get()];
  return contentView.get();
}

// Create the contents of the Connection tab and add it to the given tab view.
// Returns a weak reference to the tab view item's view.
- (NSView*)addConnectionTabToTabView:(NSTabView*)tabView {
  scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  scoped_nsobject<NSView> contentView([[WebsiteSettingsContentView alloc]
      initWithFrame:[tabView_ contentRect]]);

  // Place all the text at the same position. It will be adjusted in
  // performLayout.
  NSPoint textPosition = NSMakePoint(
      kTabViewContentsPadding + kImageSize + kImageSpacing,
      kTabViewContentsPadding);

  identityStatusIcon_ =
      [self addImageToView:contentView atOffset:kTabViewContentsPadding];
  identityStatusDescriptionField_ =
      [self addText:string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];
  separatorAfterIdentity_ = [self addSeparatorToView:contentView];

  connectionStatusIcon_ =
      [self addImageToView:contentView atOffset:kTabViewContentsPadding];
  connectionStatusDescriptionField_ =
      [self addText:string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];
  separatorAfterConnection_ = [self addSeparatorToView:contentView];

  firstVisitIcon_ =
      [self addImageToView:contentView atOffset:kTabViewContentsPadding];
  firstVisitHeaderField_ =
      [self addText:l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_INFO_TITLE)
           withSize:[NSFont smallSystemFontSize]
               bold:YES
             toView:contentView.get()
            atPoint:textPosition];
  firstVisitDescriptionField_ =
      [self addText:string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];

  [item setView:contentView.get()];
  [tabView_ addTabViewItem:item.get()];

  return contentView.get();
}

// Set the Y position of |view| to the given position, and return the position
// of its bottom edge.
- (CGFloat)setYPositionOfView:(NSView*)view to:(CGFloat)position {
  NSRect frame = [view frame];
  frame.origin.y = position;
  [view setFrame:frame];
  return position + NSHeight(frame);
}

// Layout all of the controls in the window. This should be called whenever
// the content has changed.
- (void)performLayout {
  // Place the identity status immediately below the identity.
  [self sizeTextFieldHeightToFit:identityField_];
  [self sizeTextFieldHeightToFit:identityStatusField_];
  CGFloat yPos = NSMaxY([identityField_ frame]) + kHeadlineSpacing;
  yPos = [self setYPositionOfView:identityStatusField_ to:yPos];

  // Lay out the connection tab.

  // Lay out the identity status section.
  [self sizeTextFieldHeightToFit:identityStatusDescriptionField_];
  yPos = std::max(NSMaxY([identityStatusDescriptionField_ frame]),
                  NSMaxY([identityStatusIcon_ frame]));
  yPos = [self setYPositionOfView:separatorAfterIdentity_
                               to:yPos + kVerticalSpacing];
  yPos += kVerticalSpacing;

  // Lay out the connection status section.
  [self sizeTextFieldHeightToFit:connectionStatusDescriptionField_];
  [self setYPositionOfView:connectionStatusIcon_ to:yPos];
  [self setYPositionOfView:connectionStatusDescriptionField_ to:yPos];
  yPos = std::max(NSMaxY([connectionStatusDescriptionField_ frame]),
                  NSMaxY([connectionStatusIcon_ frame]));
  yPos = [self setYPositionOfView:separatorAfterConnection_
                               to:yPos + kVerticalSpacing];
  yPos += kVerticalSpacing;

  // Lay out the last visit section.
  [self setYPositionOfView:firstVisitIcon_ to:yPos];
  [self sizeTextFieldHeightToFit:firstVisitHeaderField_];
  yPos = [self setYPositionOfView:firstVisitHeaderField_ to:yPos];
  yPos += kHeadlineSpacing;
  [self sizeTextFieldHeightToFit:firstVisitDescriptionField_];
  [self setYPositionOfView:firstVisitDescriptionField_ to:yPos];

  // Adjust the tab view size and place it below the identity status.

  NSRect segmentedControlFrame = [segmentedControl_ frame];
  segmentedControlFrame.origin.y =
      NSMaxY([identityStatusField_ frame]);
  [segmentedControl_ setFrame:segmentedControlFrame];

  NSRect tabViewFrame = [tabView_ frame];
  tabViewFrame.origin.y = NSMaxY(segmentedControlFrame);

  CGFloat connectionTabHeight = std::max(
      NSMaxY([firstVisitDescriptionField_ frame]),
      NSMaxY([firstVisitIcon_ frame ]));
  connectionTabHeight += kVerticalSpacing;

  CGFloat tabContentHeight = std::max(connectionTabHeight,
                                      permissionsTabHeight_);
  tabViewFrame.size.height = tabContentHeight +
      NSHeight(tabViewFrame) - NSHeight([tabView_ contentRect]);
  [tabView_ setFrame:tabViewFrame];

  // Adjust the contentView to fit everything.
  [contentView_ setFrame:NSMakeRect(
      0, 0, kWindowWidth, NSMaxY([tabView_ frame]))];

  // Now adjust and position the window.

  NSRect windowFrame =
      NSMakeRect(0, 0, kWindowWidth, NSHeight([contentView_ frame]));
  windowFrame.size = [[[self window] contentView] convertSize:windowFrame.size
                                                       toView:nil];
  // Adjust the origin by the difference in height.
  windowFrame.origin = [[self window] frame].origin;
  windowFrame.origin.y -= NSHeight(windowFrame) -
      NSHeight([[self window] frame]);

  // Resize the window. Only animate if the window is visible, otherwise it
  // could be "growing" while it's opening, looking awkward.
  [[self window] setFrame:windowFrame
                  display:YES
                  animate:[[self window] isVisible]];

  // Adjust the anchor for the bubble.
  NSPoint anchorPoint =
      [self anchorPointForWindowWithHeight:NSHeight(windowFrame)
                              parentWindow:[self parentWindow]];
  [self setAnchorPoint:anchorPoint];
}

// Takes in the bubble's height and the parent window, which should be a
// BrowserWindow, and gets the proper anchor point for the bubble. The returned
// point is in screen coordinates.
- (NSPoint)anchorPointForWindowWithHeight:(CGFloat)bubbleHeight
                             parentWindow:(NSWindow*)parent {
  BrowserWindowController* controller = [parent windowController];
  NSPoint origin = NSZeroPoint;
  if ([controller isKindOfClass:[BrowserWindowController class]]) {
    LocationBarViewMac* locationBar = [controller locationBarBridge];
    if (locationBar) {
      NSPoint bubblePoint = locationBar->GetPageInfoBubblePoint();
      origin = [parent convertBaseToScreen:bubblePoint];
    }
  }
  return origin;
}

// Sets properties on the given |field| to act as the title or description
// labels in the bubble.
- (void)configureTextFieldAsLabel:(NSTextField*)textField {
  [textField setEditable:NO];
  [textField setSelectable:YES];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
}

// Adjust the height of the given text field to match its text.
- (void)sizeTextFieldHeightToFit:(NSTextField*)textField {
  NSRect frame = [textField frame];
  frame.size.height +=
     [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
         textField];
  [textField setFrame:frame];
}

// Create a new text field and add it to the given array of subviews.
// The array will retain a reference to the object.
- (NSTextField*)addText:(const string16&)text
               withSize:(CGFloat)fontSize
                   bold:(BOOL)bold
                 toView:(NSView*)view
                atPoint:(NSPoint)point {
  // Size the text to take up the full available width, with some padding.
  // The height is arbitrary as it will be adjusted later.
  CGFloat width = NSWidth([view frame]) - point.x - kFramePadding;
  NSRect frame = NSMakeRect(point.x, point.y, width, kImageSize);
  scoped_nsobject<NSTextField> textField(
     [[NSTextField alloc] initWithFrame:frame]);
  [self configureTextFieldAsLabel:textField.get()];
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  NSFont* font = bold ? [NSFont boldSystemFontOfSize:fontSize]
                      : [NSFont systemFontOfSize:fontSize];
  [textField setFont:font];
  [self sizeTextFieldHeightToFit:textField];
  [view addSubview:textField.get()];
  return textField.get();
}

// Add an image as a subview of the given view, placed at a pre-determined x
// position and the given y position. Return the new NSImageView.
- (NSImageView*)addImageToView:(NSView*)view
                      atOffset:(CGFloat)offset {
  NSRect frame = NSMakeRect(kFramePadding, offset, kImageSize, kImageSize);
  scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:frame]);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [view addSubview:imageView.get()];
  return imageView.get();
}

// Add a separator as a subview of the given view. Return the new view.
- (NSView*)addSeparatorToView:(NSView*)view {
  // Take up almost the full width of the container's frame.
  CGFloat width = NSWidth([view frame]) - 2 * kFramePadding;

  // Use an arbitrary position; it will be adjusted in performLayout.
  NSBox* spacer =
      [self separatorWithFrame:NSMakeRect(kFramePadding, 0, width, 0)];
  [view addSubview:spacer];
  return spacer;
}

// Sets the button title appropriately based on the current setting and the
// default setting.
- (void)setTitleOfButton:(NSPopUpButton*)button {
  ContentSetting setting = [self contentSettingForButton:button];

  // All menu items are tagged with the default setting.
  ContentSetting defaultSetting = static_cast<ContentSetting>(
      [[button selectedItem] tag]);

  // Update the title to match the current permission setting.
  scoped_nsobject<NSMenuItem> titleItem([[NSMenuItem alloc] init]);
  [titleItem setTitle:base::SysUTF16ToNSString(
      WebsiteSettingsUI::PermissionActionToUIString(
          setting, defaultSetting, content_settings::SETTING_SOURCE_USER))];
  [[button cell] setUsesItemFromMenu:NO];
  [[button cell] setMenuItem:titleItem.get()];
  [button sizeToFit];
}

- (ContentSetting)contentSettingForButton:(NSPopUpButton*)button {
  // Determine the setting based on the index of the selected menu item.
  NSUInteger buttonIndex = [button indexOfSelectedItem];
  if (buttonIndex >= arraysize(kPermissionsMenuSettings)) {
    NOTREACHED();
    return CONTENT_SETTING_DEFAULT;
  }
  return kPermissionsMenuSettings[buttonIndex];
}

// Add a pop-up button for |permissionInfo| to the given view.
- (NSPopUpButton*)addPopUpButtonForPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                                       toView:(NSView*)view
                                      atPoint:(NSPoint)point {
  // Use an arbitrary width and height; it will be sized to fit.
  NSRect frame = NSMakeRect(point.x, point.y, 1, 1);
  scoped_nsobject<NSPopUpButton> button(
      [[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO]);

  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRoundRectBezelStyle];
  [button setShowsBorderOnlyWhileMouseInside:YES];
  [[button cell] setHighlightsBy:NSCellLightsByGray];

  // Create the popup menu.
  for (unsigned int i = 0; i < arraysize(kPermissionsMenuSettings); ++i) {
    ContentSetting setting = kPermissionsMenuSettings[i];
    if (setting == CONTENT_SETTING_DEFAULT) {
      [button addItemWithTitle:l10n_util::GetNSStringF(
          IDS_WEBSITE_SETTINGS_DEFAULT_PERMISSION_LABEL,
          WebsiteSettingsUI::PermissionValueToUIString(
              permissionInfo.default_setting))];
    } else {
      [button addItemWithTitle:l10n_util::GetNSStringF(
          IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
          WebsiteSettingsUI::PermissionValueToUIString(setting))];
    }
    // Tag all of the menu items with the default setting. It's required to
    // generate the button title (e.g. "Allowed by default").
    [[button lastItem] setTag:permissionInfo.default_setting];

    // Select the item corresponding to the current setting.
    if (setting == permissionInfo.setting)
      [button selectItem:[button lastItem]];
  }
  [button setTag:permissionInfo.type];

  [button setAction:@selector(permissionValueChanged:)];
  [button setTarget:self];

  [self setTitleOfButton:button];

  [view addSubview:button.get()];
  return button.get();
}

- (void)tabSelected:(id)sender {
  [tabView_ selectTabViewItemAtIndex:[segmentedControl_ selectedSegment]];
}

// Handler for the permission-changing menus.
- (void)permissionValueChanged:(id)sender {
  DCHECK([sender isKindOfClass:[NSPopUpButton class]]);
  NSPopUpButton* button = static_cast<NSPopUpButton*>(sender);
  ContentSettingsType type = static_cast<ContentSettingsType>([button tag]);
  ContentSetting newSetting = [self contentSettingForButton:button];

  [self setTitleOfButton:button];
  if (presenter_.get())
    presenter_->OnSitePermissionChanged(type, newSetting);
}

// Adds a new row to the UI listing the permissions. Returns the amount of
// vertical space that was taken up by the row.
- (CGFloat)addPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                  toView:(NSView*)view
                 atPoint:(NSPoint)point {
  string16 labelText =
      WebsiteSettingsUI::PermissionTypeToUIString(permissionInfo.type) +
      ASCIIToUTF16(":");

  NSTextField* label = [self addText:labelText
                            withSize:[NSFont smallSystemFontSize]
                                bold:NO
                              toView:view
                             atPoint:point];

  // Shrink the label to fit the text width.
  NSSize requiredSize = [[label cell] cellSizeForBounds:[label frame]];
  [label setFrameSize:requiredSize];

  NSPoint popUpPosition = NSMakePoint(NSMaxX([label frame]), point.y);
  NSPopUpButton* button = [self addPopUpButtonForPermission:permissionInfo
                                                     toView:view
                                                    atPoint:popUpPosition];

  // Adjust the vertical position of the button so that its title text is
  // aligned with the label. Assumes that the text is the same size in both.
  NSRect titleRect = [[button cell] titleRectForBounds:[button bounds]];
  popUpPosition.y -= titleRect.origin.y;
  [button setFrameOrigin:popUpPosition];

  return std::max(NSHeight([label frame]), NSHeight([button frame]));
}

// Set the content of the identity and identity status fields.
- (void)setIdentityInfo:(const WebsiteSettingsUI::IdentityInfo&)identityInfo {
  [identityField_ setStringValue:
      base::SysUTF8ToNSString(identityInfo.site_identity)];
  [identityStatusField_ setStringValue:
      base::SysUTF16ToNSString(identityInfo.GetIdentityStatusText())];

  [identityStatusIcon_ setImage:WebsiteSettingsUI::GetIdentityIcon(
      identityInfo.identity_status).ToNSImage()];
  [identityStatusDescriptionField_ setStringValue:
      base::SysUTF8ToNSString(identityInfo.identity_status_description)];

  [connectionStatusIcon_ setImage:WebsiteSettingsUI::GetConnectionIcon(
      identityInfo.connection_status).ToNSImage()];
  [connectionStatusDescriptionField_ setStringValue:
      base::SysUTF8ToNSString(identityInfo.connection_status_description)];

  [self performLayout];
}

- (void)setFirstVisit:(const string16&)firstVisit {
  [firstVisitIcon_ setImage:
      WebsiteSettingsUI::GetFirstVisitIcon(firstVisit).ToNSImage()];
  [firstVisitDescriptionField_ setStringValue:
      base::SysUTF16ToNSString(firstVisit)];
  [self performLayout];
}

- (void)setPermissionInfo:(const PermissionInfoList&)permissionInfoList {
  [permissionsContentView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kFramePadding, kFramePadding);

  for (PermissionInfoList::const_iterator permission =
           permissionInfoList.begin();
       permission != permissionInfoList.end();
       ++permission) {
    CGFloat rowHeight = [self addPermission:*permission
                                     toView:permissionsContentView_
                                    atPoint:controlOrigin];
    controlOrigin.y += rowHeight + kPermissionsTabSpacing;
  }
  permissionsTabHeight_ = controlOrigin.y;
}

@end

WebsiteSettingsUIBridge::WebsiteSettingsUIBridge()
  : bubble_controller_(nil) {
}

WebsiteSettingsUIBridge::~WebsiteSettingsUIBridge() {
}

void WebsiteSettingsUIBridge::set_bubble_controller(
    WebsiteSettingsBubbleController* controller) {
  bubble_controller_ = controller;
}

void WebsiteSettingsUIBridge::Show(gfx::NativeWindow parent,
                                   Profile* profile,
                                   TabContents* tab_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) {
  // Create the bridge. This will be owned by the bubble controller.
  WebsiteSettingsUIBridge* bridge = new WebsiteSettingsUIBridge();

  // Create the bubble controller. It will dealloc itself when it closes.
  WebsiteSettingsBubbleController* bubble_controller =
      [[WebsiteSettingsBubbleController alloc] initWithParentWindow:parent
          websiteSettingsUIBridge:bridge];

  // Initialize the presenter, which holds the model and controls the UI.
  // This is also owned by the bubble controller.
  WebsiteSettings* presenter = new WebsiteSettings(
      bridge,
      profile,
      tab_contents->content_settings(),
      tab_contents->infobar_tab_helper(),
      url,
      ssl,
      content::CertStore::GetInstance());
  [bubble_controller setPresenter:presenter];

  [bubble_controller showWindow:nil];
}

void WebsiteSettingsUIBridge::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
}

void WebsiteSettingsUIBridge::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  [bubble_controller_ setPermissionInfo:permission_info_list];
}

void WebsiteSettingsUIBridge::SetIdentityInfo(
    const WebsiteSettingsUI::IdentityInfo& identity_info) {
  [bubble_controller_ setIdentityInfo:identity_info];
}

void WebsiteSettingsUIBridge::SetFirstVisit(const string16& first_visit) {
  [bubble_controller_ setFirstVisit:first_visit];
}
