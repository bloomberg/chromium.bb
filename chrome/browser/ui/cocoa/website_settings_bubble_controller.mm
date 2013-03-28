// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings_bubble_controller.h"

#include <cmath>

#import <AppKit/AppKit.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#import "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// The default width of the window, in view coordinates. It may be larger to
// fit the content.
const CGFloat kDefaultWindowWidth = 310;

// Spacing in between sections.
const CGFloat kVerticalSpacing = 10;

// Padding between the window frame and content.
const CGFloat kFramePadding = 20;

// Padding between the window frame and content for the internal page bubble.
const CGFloat kInternalPageFramePadding = 10;

// Spacing between the headlines and description text on the Connection tab.
const CGFloat kConnectionHeadlineSpacing = 2;

// Spacing between images on the Connection tab and the text.
const CGFloat kConnectionImageSpacing = 10;

// Spacing between the image and text for internal pages.
const CGFloat kInternalPageImageSpacing = 10;

// Square size of the images on the Connections tab.
const CGFloat kConnectionImageSize = 30;

// Square size of the image that is shown for internal pages.
const CGFloat kInternalPageImageSize = 26;

// Square size of the permission images.
const CGFloat kPermissionImageSize = 19;

// Vertical adjustment for the permission images.
// They have an extra pixel of padding on the bottom edge.
const CGFloat kPermissionImageYAdjust = 1;

// Spacing between a permission image and the text.
const CGFloat kPermissionImageSpacing = 3;

// The spacing between individual items in the Permissions tab.
const CGFloat kPermissionsTabSpacing = 12;

// Extra spacing after a headline on the Permissions tab.
const CGFloat kPermissionsHeadlineSpacing = 2;

// The amount of horizontal space between a permission label and the popup.
const CGFloat kPermissionPopUpXSpacing = 3;

// The amount of horizontal space between the permission popup title and the
// arrow icon.
const CGFloat kPermissionButtonTitleRightPadding = 4;

// The extra space to the left of the first tab in the tab strip.
const CGFloat kTabStripXPadding = kFramePadding;

// The amount of space between the visual borders of adjacent tabs.
const CGFloat kTabSpacing = 4;

// The amount of space above the tab strip.
const CGFloat kTabStripTopSpacing = 14;

// The height of the clickable area of the tab.
const CGFloat kTabHeight = 28;

// The amount of space above tab labels.
const CGFloat kTabLabelTopPadding = 6;

// The amount of padding to leave on either side of the tab label.
const CGFloat kTabLabelXPadding = 12;

// Return the text color to use for the indentity status when the site's
// identity has been verified.
NSColor* IdentityVerifiedTextColor() {
  // RGB components are specified using integer RGB [0-255] values for easy
  // comparison to other platforms.
  return [NSColor colorWithCalibratedRed:0x07/255.0
                                   green:0x95/255.0
                                    blue:0
                                   alpha:1.0];
}

}  // namespace

@interface WebsiteSettingsTabSegmentedCell : NSSegmentedCell {
 @private
  scoped_nsobject<NSImage> tabstripCenterImage_;
  scoped_nsobject<NSImage> tabstripLeftImage_;
  scoped_nsobject<NSImage> tabstripRightImage_;

  scoped_nsobject<NSImage> tabCenterImage_;
  scoped_nsobject<NSImage> tabLeftImage_;
  scoped_nsobject<NSImage> tabRightImage_;

  // Key track of the index of segment which has keyboard focus. This is not
  // the same as the currently selected segment.
  NSInteger keySegment_;
}
@end

@implementation WebsiteSettingsTabSegmentedCell
- (id)init {
  if ((self = [super init])) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    tabstripCenterImage_.reset(rb.GetNativeImageNamed(
        IDR_WEBSITE_SETTINGS_TABSTRIP_CENTER).CopyNSImage());
    tabstripLeftImage_.reset(rb.GetNativeImageNamed(
        IDR_WEBSITE_SETTINGS_TABSTRIP_LEFT).CopyNSImage());
    tabstripRightImage_.reset(rb.GetNativeImageNamed(
        IDR_WEBSITE_SETTINGS_TABSTRIP_RIGHT).CopyNSImage());

    tabCenterImage_.reset(
        rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_CENTER2).CopyNSImage());
    tabLeftImage_.reset(
        rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_LEFT2).CopyNSImage());
    tabRightImage_.reset(
        rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TAB_RIGHT2).CopyNSImage());
  }
  return self;
}

// Called when keyboard focus in the segmented control is moved forward.
- (void)makeNextSegmentKey {
  [super makeNextSegmentKey];
  keySegment_ = (keySegment_ + 1) % [self segmentCount];
}

// Called when keyboard focus in the segmented control is moved backwards.
- (void)makePreviousSegmentKey {
  [super makePreviousSegmentKey];
  if (--keySegment_ < 0)
    keySegment_ += [self segmentCount];
}

- (void)setSelectedSegment:(NSInteger)selectedSegment {
  keySegment_ = selectedSegment;
  [super setSelectedSegment:selectedSegment];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  CGFloat tabstripHeight = [tabCenterImage_ size].height;

  // Draw the tab for the selected segment.
  NSRect tabRect = [self hitRectForSegment:[self selectedSegment]];
  tabRect.origin.y = 0;
  tabRect.size.height = tabstripHeight;

  NSDrawThreePartImage(tabRect,
                       tabLeftImage_,
                       tabCenterImage_,
                       tabRightImage_,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Draw the background to the left of the selected tab.
  NSRect backgroundRect = NSMakeRect(0, 0, NSMinX(tabRect), tabstripHeight);
  NSDrawThreePartImage(backgroundRect,
                       nil,
                       tabstripCenterImage_,
                       tabstripLeftImage_,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Draw the background to the right of the selected tab.
  backgroundRect.origin.x = NSMaxX(tabRect);
  backgroundRect.size.width = NSMaxX(cellFrame) - NSMaxX(tabRect);
  NSDrawThreePartImage(backgroundRect,
                       tabstripRightImage_,
                       tabstripCenterImage_,
                       nil,
                       /*vertical=*/ NO,
                       NSCompositeSourceOver,
                       1,
                       /*flipped=*/ YES);

  // Call the superclass method to trigger drawing of the tab labels.
  [self drawInteriorWithFrame:cellFrame inView:controlView];
  if ([[controlView window] firstResponder] == controlView)
    [self drawFocusRect];
}

- (void)drawFocusRect {
  gfx::ScopedNSGraphicsContextSaveGState scoped_state;
  NSSetFocusRingStyle(NSFocusRingOnly);
  [[NSColor keyboardFocusIndicatorColor] set];
  NSFrameRect([self hitRectForSegment:keySegment_]);
}

// Return the hit rect (i.e., the visual bounds of the tab) for
// the given segment.
- (NSRect)hitRectForSegment:(NSInteger)segment {
  CGFloat tabstripHeight = [tabCenterImage_ size].height;
  DCHECK_GT(tabstripHeight, kTabHeight);

  NSRect rect = NSMakeRect(0, tabstripHeight - kTabHeight,
                           [self widthForSegment:segment], kTabHeight);
  for (NSInteger i = 0; i < segment; ++i) {
    rect.origin.x += [self widthForSegment:i];
  }
  int xAdjust = segment == 0 ? kTabStripXPadding : 0;
  rect.size.width = std::floor(
      [self widthForSegment:segment] - kTabSpacing - xAdjust);
  rect.origin.x = std::floor(rect.origin.x + kTabSpacing / 2 + xAdjust);

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
// segmented control. Otherwise, clicks on the lower part of a tab will be
// ignored.
- (NSSize)cellSizeForBounds:(NSRect)aRect {
  return NSMakeSize([super cellSizeForBounds:aRect].width,
                    [tabstripCenterImage_ size].height);
}

// Returns the minimum size required to display this cell.
// It should always be exactly as tall as the tabstrip background image.
- (NSSize)cellSize {
  return NSMakeSize([super cellSize].width,
                    [tabstripCenterImage_ size].height);
}
@end

@implementation WebsiteSettingsBubbleController

- (CGFloat)defaultWindowWidth {
  return kDefaultWindowWidth;
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
   websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge
               webContents:(content::WebContents*)webContents
            isInternalPage:(BOOL)isInternalPage {
  DCHECK(parentWindow);

  webContents_ = webContents;

  // Use an arbitrary height; it will be changed in performLayout.
  NSRect contentRect = NSMakeRect(0, 0, [self defaultWindowWidth], 1);
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

    // Create the container view that uses flipped coordinates.
    NSRect contentFrame = NSMakeRect(0, 0, [self defaultWindowWidth], 300);
    contentView_.reset(
        [[FlippedView alloc] initWithFrame:contentFrame]);

    // Replace the window's content.
    [[[self window] contentView] setSubviews:
        [NSArray arrayWithObject:contentView_.get()]];

    if (isInternalPage)
      [self initializeContentsForInternalPage];
    else
      [self initializeContents];

    bridge_.reset(bridge);
    bridge_->set_bubble_controller(self);
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  if (presenter_.get())
    presenter_->OnUIClosing();
  [super windowWillClose:notification];
}

- (void)setPresenter:(WebsiteSettings*)presenter {
  presenter_.reset(presenter);
}

// Create the subviews for the bubble for internal Chrome pages.
- (void)initializeContentsForInternalPage {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  NSPoint controlOrigin = NSMakePoint(
      kInternalPageFramePadding,
      kInternalPageFramePadding + info_bubble::kBubbleArrowHeight);
  NSSize imageSize = NSMakeSize(kInternalPageImageSize,
                                kInternalPageImageSize);
  NSImageView* imageView = [self addImageWithSize:imageSize
                                           toView:contentView_
                                          atPoint:controlOrigin];
  [imageView setImage:rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_26).ToNSImage()];

  controlOrigin.x += NSWidth([imageView frame]) + kInternalPageImageSpacing;
  string16 text = l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
  NSTextField* textField = [self addText:text
                                withSize:[NSFont smallSystemFontSize]
                                    bold:NO
                                  toView:contentView_
                                 atPoint:controlOrigin];
  // Center the text vertically with the image.
  NSRect textFrame = [textField frame];
  textFrame.origin.y += (imageSize.height - NSHeight(textFrame)) / 2;
  [textField setFrame:textFrame];

  // Adjust the contentView to fit everything.
  CGFloat maxY = std::max(NSMaxY([imageView frame]), NSMaxY(textFrame));
  [contentView_ setFrame:NSMakeRect(
      0, 0, [self defaultWindowWidth], maxY + kInternalPageFramePadding)];

  [self sizeAndPositionWindow];
}

// Create the subviews for the website settings bubble.
- (void)initializeContents {
  // Keeps track of the position that the next control should be drawn at.
  NSPoint controlOrigin = NSMakePoint(
      kFramePadding,
      kFramePadding + info_bubble::kBubbleArrowHeight);

  // Create a text field (empty for now) to show the site identity.
  identityField_ = [self addText:string16()
                        withSize:[NSFont systemFontSize]
                            bold:YES
                          toView:contentView_
                         atPoint:controlOrigin];
  controlOrigin.y +=
      NSHeight([identityField_ frame]) + kConnectionHeadlineSpacing;

  // Create a text field to identity status (e.g. verified, not verified).
  identityStatusField_ = [self addText:string16()
                              withSize:[NSFont smallSystemFontSize]
                                  bold:NO
                                toView:contentView_
                               atPoint:controlOrigin];

  // Create the tab view and its two tabs.

  scoped_nsobject<WebsiteSettingsTabSegmentedCell> cell(
      [[WebsiteSettingsTabSegmentedCell alloc] init]);
  CGFloat tabstripHeight = [cell cellSize].height;
  NSRect tabstripFrame = NSMakeRect(
      0, 0, [self defaultWindowWidth], tabstripHeight);
  segmentedControl_.reset(
      [[NSSegmentedControl alloc] initWithFrame:tabstripFrame]);
  [segmentedControl_ setCell:cell];
  [segmentedControl_ setSegmentCount:WebsiteSettingsUI::NUM_TAB_IDS];
  [segmentedControl_ setTarget:self];
  [segmentedControl_ setAction:@selector(tabSelected:)];
  [segmentedControl_ setAutoresizingMask:NSViewWidthSizable];

  NSFont* smallSystemFont =
      [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  NSDictionary* textAttributes =
      [NSDictionary dictionaryWithObject:smallSystemFont
                                  forKey:NSFontAttributeName];

  // Create the "Permissions" tab.
  NSString* label = l10n_util::GetNSString(
      IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS);
  [segmentedControl_ setLabel:label
                   forSegment:WebsiteSettingsUI::TAB_ID_PERMISSIONS];
  NSSize textSize = [label sizeWithAttributes:textAttributes];
  CGFloat tabWidth = textSize.width + 2 * kTabLabelXPadding;
  [segmentedControl_ setWidth:tabWidth + kTabStripXPadding
                   forSegment:WebsiteSettingsUI::TAB_ID_PERMISSIONS];

  // Create the "Connection" tab.
  label = l10n_util::GetNSString(IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION);
  textSize = [label sizeWithAttributes:textAttributes];
  [segmentedControl_ setLabel:label
                   forSegment:WebsiteSettingsUI::TAB_ID_CONNECTION];

  DCHECK_EQ([segmentedControl_ segmentCount], WebsiteSettingsUI::NUM_TAB_IDS);

  // Make both tabs the width of the widest. The first segment has some
  // additional padding that is not part of the tab, which is used for drawing
  // the background of the tab strip.
  tabWidth = std::max(tabWidth,
                      textSize.width + 2 * kTabLabelXPadding);
  [segmentedControl_ setWidth:tabWidth + kTabStripXPadding
                   forSegment:WebsiteSettingsUI::TAB_ID_PERMISSIONS];
  [segmentedControl_ setWidth:tabWidth
                   forSegment:WebsiteSettingsUI::TAB_ID_CONNECTION];

  [segmentedControl_ setFont:smallSystemFont];
  [contentView_ addSubview:segmentedControl_];

  NSRect tabFrame = NSMakeRect(0, 0, [self defaultWindowWidth], 300);
  tabView_.reset([[NSTabView alloc] initWithFrame:tabFrame]);
  [tabView_ setTabViewType:NSNoTabsNoBorder];
  [tabView_ setDrawsBackground:NO];
  [tabView_ setControlSize:NSSmallControlSize];
  [contentView_ addSubview:tabView_.get()];

  permissionsTabContentView_ = [self addPermissionsTabToTabView:tabView_];
  connectionTabContentView_ = [self addConnectionTabToTabView:tabView_];
  [self setSelectedTab:WebsiteSettingsUI::TAB_ID_PERMISSIONS];

  [self performLayout];
}

// Create the contents of the Permissions tab and add it to the given tab view.
// Returns a weak reference to the tab view item's view.
- (NSView*)addPermissionsTabToTabView:(NSTabView*)tabView {
  scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  [tabView_ insertTabViewItem:item.get()
                      atIndex:WebsiteSettingsUI::TAB_ID_PERMISSIONS];
  scoped_nsobject<NSView> contentView([[FlippedView alloc]
      initWithFrame:[tabView_ contentRect]]);
  [contentView setAutoresizingMask:NSViewWidthSizable];
  [item setView:contentView.get()];

  // Initialize the two containers that hold the controls. The initial frames
  // are arbitrary, and will be adjusted after the controls are laid out.
  cookiesView_ = [[[FlippedView alloc]
      initWithFrame:[tabView_ contentRect]] autorelease];
  [cookiesView_ setAutoresizingMask:NSViewWidthSizable];

  permissionsView_ = [[[FlippedView alloc]
      initWithFrame:[tabView_ contentRect]] autorelease];

  [contentView addSubview:cookiesView_];
  [contentView addSubview:permissionsView_];

  // Create the link button to view cookies and site data.
  // Its position will be set in performLayout.
  NSString* cookieButtonText = l10n_util::GetNSString(
      IDS_WEBSITE_SETTINGS_SHOW_SITE_DATA);
  cookiesButton_ = [self addLinkButtonWithText:cookieButtonText
                                        toView:contentView];
  [cookiesButton_ setTarget:self];
  [cookiesButton_ setAction:@selector(showCookiesAndSiteData:)];

  return contentView.get();
}

// Handler for the link button below the list of cookies.
- (void)showCookiesAndSiteData:(id)sender {
  DCHECK(webContents_);
  content::RecordAction(
      content::UserMetricsAction("WebsiteSettings_CookiesDialogOpened"));
  chrome::ShowCollectedCookiesDialog(webContents_);
}

// Handler for the link button to show certificate information.
- (void)showCertificateInfo:(id)sender {
  DCHECK(certificateId_);
  ShowCertificateViewerByID(webContents_,
                            [self parentWindow],
                            certificateId_);
}

// Handler for the link to show help information about the connection tab.
- (void)showHelpPage:(id)sender {
  webContents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kPageInfoHelpCenterURL),
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK,
      false));
}

// Create the contents of the Connection tab and add it to the given tab view.
// Returns a weak reference to the tab view item's view.
- (NSView*)addConnectionTabToTabView:(NSTabView*)tabView {
  scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  scoped_nsobject<NSView> contentView([[FlippedView alloc]
      initWithFrame:[tabView_ contentRect]]);
  [contentView setAutoresizingMask:NSViewWidthSizable];

  // Place all the text and images at the same position. The positions will be
  // adjusted in performLayout.
  NSPoint textPosition = NSMakePoint(
      kFramePadding + kConnectionImageSize + kConnectionImageSpacing,
      kFramePadding);
  NSPoint imagePosition = NSMakePoint(kFramePadding, kFramePadding);
  NSSize imageSize = NSMakeSize(kConnectionImageSize, kConnectionImageSize);

  identityStatusIcon_ = [self addImageWithSize:imageSize
                                        toView:contentView
                                       atPoint:imagePosition];
  identityStatusDescriptionField_ =
      [self addText:string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];

  separatorAfterIdentity_ = [self addSeparatorToView:contentView];
  [separatorAfterIdentity_ setAutoresizingMask:NSViewWidthSizable];

  connectionStatusIcon_ = [self addImageWithSize:imageSize
                                          toView:contentView
                                         atPoint:imagePosition];
  connectionStatusDescriptionField_ =
      [self addText:string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];
  certificateInfoButton_ = nil;  // This will be created only if necessary.
  separatorAfterConnection_ = [self addSeparatorToView:contentView];
  [separatorAfterConnection_ setAutoresizingMask:NSViewWidthSizable];

  firstVisitIcon_ = [self addImageWithSize:imageSize
                                    toView:contentView
                                   atPoint:imagePosition];
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

  separatorAfterFirstVisit_ = [self addSeparatorToView:contentView];
  [separatorAfterFirstVisit_ setAutoresizingMask:NSViewWidthSizable];

  NSString* helpButtonText = l10n_util::GetNSString(
      IDS_PAGE_INFO_HELP_CENTER_LINK);
  helpButton_ = [self addLinkButtonWithText:helpButtonText
                                     toView:contentView];
  [helpButton_ setTarget:self];
  [helpButton_ setAction:@selector(showHelpPage:)];

  [item setView:contentView.get()];
  [tabView_ insertTabViewItem:item.get()
                      atIndex:WebsiteSettingsUI::TAB_ID_CONNECTION];

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

- (void)setWidthOfView:(NSView*)view to:(CGFloat)width {
  [view setFrameSize:NSMakeSize(width, NSHeight([view frame]))];
}

// Layout all of the controls in the window. This should be called whenever
// the content has changed.
- (void)performLayout {
  // Make the content at least as wide as the permissions view.
  CGFloat contentWidth = std::max([self defaultWindowWidth],
                                  NSWidth([permissionsView_ frame]));

  // Set the width of the content view now, so that all the text fields will
  // be sized to fit before their heights and vertical positions are adjusted.
  // The tab view will only resize the currently selected tab, so resize both
  // tab content views manually.
  [self setWidthOfView:contentView_ to:contentWidth];
  [self setWidthOfView:permissionsTabContentView_ to:contentWidth];
  [self setWidthOfView:connectionTabContentView_ to:contentWidth];

  // Place the identity status immediately below the identity.
  [self sizeTextFieldHeightToFit:identityField_];
  [self sizeTextFieldHeightToFit:identityStatusField_];
  CGFloat yPos = NSMaxY([identityField_ frame]) + kConnectionHeadlineSpacing;
  yPos = [self setYPositionOfView:identityStatusField_ to:yPos];

  // Lay out the Permissions tab.

  yPos = [self setYPositionOfView:cookiesView_ to:kFramePadding];

  // Put the link button for cookies and site data just below the cookie info.
  NSRect cookiesButtonFrame = [cookiesButton_ frame];
  cookiesButtonFrame.origin.y = yPos;
  cookiesButtonFrame.origin.x = kFramePadding;
  [cookiesButton_ setFrame:cookiesButtonFrame];

  // Put the permission info just below the link button.
  [self setYPositionOfView:permissionsView_
                        to:NSMaxY(cookiesButtonFrame) + kFramePadding];

  // Lay out the Connection tab.

  // Lay out the identity status section.
  [self sizeTextFieldHeightToFit:identityStatusDescriptionField_];
  yPos = std::max(NSMaxY([identityStatusDescriptionField_ frame]),
                  NSMaxY([identityStatusIcon_ frame]));
  if (certificateInfoButton_) {
    NSRect certificateButtonFrame = [certificateInfoButton_ frame];
    certificateButtonFrame.origin.x = NSMinX(
        [identityStatusDescriptionField_ frame]);
    certificateButtonFrame.origin.y = yPos + kVerticalSpacing;
    [certificateInfoButton_ setFrame:certificateButtonFrame];
    yPos = NSMaxY(certificateButtonFrame);
  }
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
  yPos += kConnectionHeadlineSpacing;
  [self sizeTextFieldHeightToFit:firstVisitDescriptionField_];
  yPos = [self setYPositionOfView:firstVisitDescriptionField_ to:yPos];
  yPos = [self setYPositionOfView:separatorAfterFirstVisit_
                               to:yPos + kVerticalSpacing];
  yPos += kVerticalSpacing;
  [self setYPositionOfView:helpButton_ to:yPos];

  // Adjust the tab view size and place it below the identity status.

  yPos = NSMaxY([identityStatusField_ frame]) + kTabStripTopSpacing;
  yPos = [self setYPositionOfView:segmentedControl_ to:yPos];

  CGFloat connectionTabHeight = NSMaxY([helpButton_ frame]) + kVerticalSpacing;

  NSRect tabViewFrame = [tabView_ frame];
  tabViewFrame.origin.y = yPos;
  tabViewFrame.size.height =
      std::max(connectionTabHeight, NSMaxY([permissionsView_ frame]));
  tabViewFrame.size.width = contentWidth;
  [tabView_ setFrame:tabViewFrame];

  // Adjust the contentView to fit everything.
  [contentView_ setFrame:NSMakeRect(
      0, 0, NSWidth(tabViewFrame), NSMaxY(tabViewFrame))];

  [self sizeAndPositionWindow];
}

// Adjust the size of the window to match the size of the content, and position
// the bubble anchor appropriately.
- (void)sizeAndPositionWindow {
  NSRect windowFrame = [contentView_ frame];
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
  NSRect frame = NSMakeRect(point.x, point.y, width, 100);
  scoped_nsobject<NSTextField> textField(
     [[NSTextField alloc] initWithFrame:frame]);
  [self configureTextFieldAsLabel:textField.get()];
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  NSFont* font = bold ? [NSFont boldSystemFontOfSize:fontSize]
                      : [NSFont systemFontOfSize:fontSize];
  [textField setFont:font];
  [self sizeTextFieldHeightToFit:textField];
  [textField setAutoresizingMask:NSViewWidthSizable];
  [view addSubview:textField.get()];
  return textField.get();
}

// Add an image as a subview of the given view, placed at a pre-determined x
// position and the given y position. Return the new NSImageView.
- (NSImageView*)addImageWithSize:(NSSize)size
                          toView:(NSView*)view
                         atPoint:(NSPoint)point {
  NSRect frame = NSMakeRect(point.x, point.y, size.width, size.height);
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

// Add a link button with the given text to |view|.
- (NSButton*)addLinkButtonWithText:(NSString*)text toView:(NSView*)view {
  // Frame size is arbitrary; it will be adjusted by the layout tweaker.
  NSRect frame = NSMakeRect(kFramePadding, 0, 100, 10);
  scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:frame]);
  scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:text]);
  [cell setControlSize:NSSmallControlSize];
  [button setCell:cell.get()];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [view addSubview:button.get()];

  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];
  return button.get();
}

// Determine the size of a popup button with the given title.
- (NSSize)sizeForPopUpButton:(NSPopUpButton*)button
                   withTitle:(NSString*)title {
  NSDictionary* textAttributes =
      [NSDictionary dictionaryWithObject:[button font]
                                  forKey:NSFontAttributeName];
  NSSize titleSize = [title sizeWithAttributes:textAttributes];

  NSRect buttonFrame = [button frame];
  NSRect titleRect = [[button cell] titleRectForBounds:buttonFrame];
  CGFloat width = titleSize.width + NSWidth(buttonFrame) - NSWidth(titleRect);

  return NSMakeSize(width + kPermissionButtonTitleRightPadding,
                    NSHeight(buttonFrame));
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
  [button setBordered:NO];
  [[button cell] setControlSize:NSSmallControlSize];
  [button setTag:permissionInfo.type];
  [button setAction:@selector(permissionValueChanged:)];
  [button setTarget:self];

  // Create the popup menu.
  // TODO(dubroy): Refactor this code to use PermissionMenuModel.

  // Media stream permission only support "Always allow" for https.
  if (permissionInfo.type != CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
      (webContents_ && webContents_->GetURL().SchemeIsSecure())) {
    [button addItemWithTitle:
        l10n_util::GetNSString(IDS_WEBSITE_SETTINGS_MENU_ITEM_ALLOW)];
    [[button lastItem] setTag:CONTENT_SETTING_ALLOW];
  }

  // Fullscreen does not support "Always block".
  if (permissionInfo.type != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    [button addItemWithTitle:
        l10n_util::GetNSString(IDS_WEBSITE_SETTINGS_MENU_ITEM_BLOCK)];
    [[button lastItem] setTag:CONTENT_SETTING_BLOCK];
  }

  [button addItemWithTitle:l10n_util::GetNSStringF(
      IDS_WEBSITE_SETTINGS_DEFAULT_PERMISSION_LABEL,
      WebsiteSettingsUI::PermissionValueToUIString(
          permissionInfo.default_setting))];
  [[button lastItem] setTag:CONTENT_SETTING_DEFAULT];

  [button selectItemWithTag:permissionInfo.setting];

  // Set the button title.
  scoped_nsobject<NSMenuItem> titleItem([[NSMenuItem alloc] init]);
  string16 buttonTitle = WebsiteSettingsUI::PermissionActionToUIString(
      permissionInfo.setting,
      permissionInfo.default_setting,
      permissionInfo.source);
  [titleItem setTitle:base::SysUTF16ToNSString(buttonTitle)];
  [[button cell] setUsesItemFromMenu:NO];
  [[button cell] setMenuItem:titleItem.get()];
  [button sizeToFit];

  // Determine the largest possible size for this button.
  CGFloat maxTitleWidth = 0;
  for (NSInteger i = 0; i < [button numberOfItems]; ++i) {
    string16 title = WebsiteSettingsUI::PermissionActionToUIString(
        static_cast<ContentSetting>([[button itemAtIndex:i] tag]),
        permissionInfo.default_setting,
        content_settings::SETTING_SOURCE_USER);
    NSSize size = [self sizeForPopUpButton:button
                                 withTitle:base::SysUTF16ToNSString(title)];
    maxTitleWidth = std::max(maxTitleWidth, size.width);
  }
  // Ensure the containing view is large enough to contain the button with its
  // widest possible title.
  NSRect containerFrame = [view frame];
  containerFrame.size.width = std::max(
      NSWidth(containerFrame), point.x + maxTitleWidth + kFramePadding);
  [view setFrame:containerFrame];

  // Size the button to just fit the title.
  [button setFrameSize:[self sizeForPopUpButton:button
                                      withTitle:[button title]]];

  [view addSubview:button.get()];
  return button.get();
}

// Called when the user changes the selected segment in the segmented control.
- (void)tabSelected:(id)sender {
  [tabView_ selectTabViewItemAtIndex:[segmentedControl_ selectedSegment]];
}

// Handler for the permission-changing menus.
- (void)permissionValueChanged:(id)sender {
  DCHECK([sender isKindOfClass:[NSPopUpButton class]]);
  NSPopUpButton* button = static_cast<NSPopUpButton*>(sender);
  ContentSettingsType type = static_cast<ContentSettingsType>([button tag]);
  ContentSetting newSetting = static_cast<ContentSetting>(
      [[button selectedItem] tag]);
  if (presenter_.get())
    presenter_->OnSitePermissionChanged(type, newSetting);
}

// Adds a new row to the UI listing the permissions. Returns the amount of
// vertical space that was taken up by the row.
- (NSPoint)addPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                  toView:(NSView*)view
                 atPoint:(NSPoint)point {
  // TODO(dubroy): Remove this check by refactoring GetPermissionIcon to take
  // the PermissionInfo object as its argument.
  ContentSetting setting = permissionInfo.setting == CONTENT_SETTING_DEFAULT ?
      permissionInfo.default_setting : permissionInfo.setting;
  NSImage* image = WebsiteSettingsUI::GetPermissionIcon(
      permissionInfo.type, setting).ToNSImage();
  NSImageView* imageView = [self addImageWithSize:[image size]
                                           toView:view
                                          atPoint:point];
  [imageView setImage:image];
  point.x += kPermissionImageSize + kPermissionImageSpacing;

  string16 labelText =
      WebsiteSettingsUI::PermissionTypeToUIString(permissionInfo.type) +
      ASCIIToUTF16(":");

  NSTextField* label = [self addText:labelText
                            withSize:[NSFont smallSystemFontSize]
                                bold:NO
                              toView:view
                             atPoint:point];
  [label sizeToFit];
  NSPoint popUpPosition = NSMakePoint(NSMaxX([label frame]), point.y);
  NSPopUpButton* button = [self addPopUpButtonForPermission:permissionInfo
                                                     toView:view
                                                    atPoint:popUpPosition];

  // Adjust the vertical position of the button so that its title text is
  // aligned with the label. Assumes that the text is the same size in both.
  // Also adjust the horizontal position to remove excess space due to the
  // invisible bezel.
  NSRect titleRect = [[button cell] titleRectForBounds:[button bounds]];
  popUpPosition.x -= titleRect.origin.x - kPermissionPopUpXSpacing;
  popUpPosition.y -= titleRect.origin.y;
  [button setFrameOrigin:popUpPosition];

  // Align the icon with the text.
  [self alignPermissionIcon:imageView withTextField:label];

  // Permissions specified by policy or an extension cannot be changed.
  if (permissionInfo.source == content_settings::SETTING_SOURCE_EXTENSION ||
      permissionInfo.source == content_settings::SETTING_SOURCE_POLICY) {
    [button setEnabled:NO];
  }

  NSRect buttonFrame = [button frame];
  return NSMakePoint(NSMaxX(buttonFrame), NSMaxY(buttonFrame));
}

// Align an image with a text field by vertically centering the image on
// the cap height of the first line of text.
- (void)alignPermissionIcon:(NSImageView*)imageView
              withTextField:(NSTextField*)textField {
  NSFont* font = [textField font];

  // Calculate the offset from the top of the text field.
  CGFloat capHeight = [font capHeight];
  CGFloat offset = (kPermissionImageSize - capHeight) / 2 -
      ([font ascender] - capHeight) - kPermissionImageYAdjust;

  NSRect frame = [imageView frame];
  frame.origin.y -= offset;
  [imageView setFrame:frame];
}

- (CGFloat)addCookieInfo:
    (const WebsiteSettingsUI::CookieInfo&)cookieInfo
                  toView:(NSView*)view
                 atPoint:(NSPoint)point {
  NSImage* image = WebsiteSettingsUI::GetPermissionIcon(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_ALLOW).ToNSImage();
  NSImageView* imageView = [self addImageWithSize:[image size]
                                           toView:view
                                          atPoint:point];
  [imageView setImage:image];
  point.x += kPermissionImageSize + kPermissionImageSpacing;

  string16 labelText = l10n_util::GetStringFUTF16(
      IDS_WEBSITE_SETTINGS_SITE_DATA_STATS_LINE,
      UTF8ToUTF16(cookieInfo.cookie_source),
      base::IntToString16(cookieInfo.allowed),
      base::IntToString16(cookieInfo.blocked));

  NSTextField* label = [self addText:labelText
                            withSize:[NSFont smallSystemFontSize]
                                bold:NO
                              toView:view
                             atPoint:point];

  // Align the icon with the text.
  [self alignPermissionIcon:imageView withTextField:label];

  return NSHeight([label frame]);
}

// Set the content of the identity and identity status fields.
- (void)setIdentityInfo:(const WebsiteSettingsUI::IdentityInfo&)identityInfo {
  [identityField_ setStringValue:
      base::SysUTF8ToNSString(identityInfo.site_identity)];
  [identityStatusField_ setStringValue:
      base::SysUTF16ToNSString(identityInfo.GetIdentityStatusText())];

  WebsiteSettings::SiteIdentityStatus status = identityInfo.identity_status;
  if (status == WebsiteSettings::SITE_IDENTITY_STATUS_CERT ||
      status == WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT) {
    [identityStatusField_ setTextColor:IdentityVerifiedTextColor()];
  }
  // If there is a certificate, add a button for viewing the certificate info.
  certificateId_ = identityInfo.cert_id;
  if (certificateId_) {
    if (!certificateInfoButton_) {
      NSString* text = l10n_util::GetNSString(IDS_PAGEINFO_CERT_INFO_BUTTON);
      certificateInfoButton_ = [self addLinkButtonWithText:text
          toView:connectionTabContentView_];

      [certificateInfoButton_ setTarget:self];
      [certificateInfoButton_ setAction:@selector(showCertificateInfo:)];
    }
  } else {
    certificateInfoButton_ = nil;
  }

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

- (void)setCookieInfo:(const CookieInfoList&)cookieInfoList {
  // The contents of the permissions view can cause the whole window to get
  // bigger, but currently permissions are always set before cookie info.
  // Check to make sure that's still the case.
  DCHECK_GT([[permissionsView_ subviews] count], 0U);

  [cookiesView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kFramePadding, 0);

  string16 sectionTitle = l10n_util::GetStringUTF16(
      IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  NSTextField* header = [self addText:sectionTitle
                             withSize:[NSFont smallSystemFontSize]
                                 bold:YES
                               toView:cookiesView_
                              atPoint:controlOrigin];
  controlOrigin.y += NSHeight([header frame]) + kPermissionsHeadlineSpacing;

  for (CookieInfoList::const_iterator it = cookieInfoList.begin();
       it != cookieInfoList.end();
       ++it) {
    controlOrigin.y += kPermissionsTabSpacing;
    CGFloat rowHeight = [self addCookieInfo:*it
                                     toView:cookiesView_
                                    atPoint:controlOrigin];
    controlOrigin.y += rowHeight;
  }

  controlOrigin.y += kPermissionsTabSpacing;
  [cookiesView_ setFrameSize:
      NSMakeSize(NSWidth([cookiesView_ frame]), controlOrigin.y)];

  [self performLayout];
}

- (void)setPermissionInfo:(const PermissionInfoList&)permissionInfoList {
  [permissionsView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kFramePadding, 0);

  string16 sectionTitle = l10n_util::GetStringUTF16(
      IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  NSTextField* header = [self addText:sectionTitle
                             withSize:[NSFont smallSystemFontSize]
                                 bold:YES
                               toView:permissionsView_
                              atPoint:controlOrigin];
  [self sizeTextFieldHeightToFit:header];
  controlOrigin.y += NSHeight([header frame]) + kPermissionsHeadlineSpacing;

  for (PermissionInfoList::const_iterator permission =
           permissionInfoList.begin();
       permission != permissionInfoList.end();
       ++permission) {
    controlOrigin.y += kPermissionsTabSpacing;
    NSPoint rowBottomRight = [self addPermission:*permission
                                          toView:permissionsView_
                                         atPoint:controlOrigin];
    controlOrigin.y = rowBottomRight.y;
  }
  controlOrigin.y += kFramePadding;
  [permissionsView_ setFrameSize:
      NSMakeSize(NSWidth([permissionsView_ frame]), controlOrigin.y)];
  [self performLayout];
}

- (void)setFirstVisit:(const string16&)firstVisit {
  [firstVisitIcon_ setImage:
      WebsiteSettingsUI::GetFirstVisitIcon(firstVisit).ToNSImage()];
  [firstVisitDescriptionField_ setStringValue:
      base::SysUTF16ToNSString(firstVisit)];
  [self performLayout];
}

- (void)setSelectedTab:(WebsiteSettingsUI::TabId)tabId {
  NSInteger index = static_cast<NSInteger>(tabId);
  [segmentedControl_ setSelectedSegment:index];
  [tabView_ selectTabViewItemAtIndex:index];
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
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) {
  bool is_internal_page = InternalChromePage(url);

  // Create the bridge. This will be owned by the bubble controller.
  WebsiteSettingsUIBridge* bridge = new WebsiteSettingsUIBridge();

  // Create the bubble controller. It will dealloc itself when it closes.
  WebsiteSettingsBubbleController* bubble_controller =
      [[WebsiteSettingsBubbleController alloc]
          initWithParentWindow:parent
       websiteSettingsUIBridge:bridge
                   webContents:web_contents
                isInternalPage:is_internal_page];

  if (!is_internal_page) {
    // Initialize the presenter, which holds the model and controls the UI.
    // This is also owned by the bubble controller.
    WebsiteSettings* presenter = new WebsiteSettings(
        bridge,
        profile,
        TabSpecificContentSettings::FromWebContents(web_contents),
        InfoBarService::FromWebContents(web_contents),
        url,
        ssl,
        content::CertStore::GetInstance());
    [bubble_controller setPresenter:presenter];
  }

  [bubble_controller showWindow:nil];
}

void WebsiteSettingsUIBridge::SetIdentityInfo(
    const WebsiteSettingsUI::IdentityInfo& identity_info) {
  [bubble_controller_ setIdentityInfo:identity_info];
}

void WebsiteSettingsUIBridge::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  [bubble_controller_ setCookieInfo:cookie_info_list];
}

void WebsiteSettingsUIBridge::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  [bubble_controller_ setPermissionInfo:permission_info_list];
}

void WebsiteSettingsUIBridge::SetFirstVisit(const string16& first_visit) {
  [bubble_controller_ setFirstVisit:first_visit];
}

void WebsiteSettingsUIBridge::SetSelectedTab(TabId tab_id) {
  [bubble_controller_ setSelectedTab:tab_id];
}
