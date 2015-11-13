// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"

#include <cmath>

#import <AppKit/AppKit.h>

#include "base/i18n/rtl.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/certificate_viewer.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_selector_button.h"
#import "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/gfx/mac/coordinate_conversion.h"
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

// Vertical adjustment for the permission images. They have an extra pixel of
// padding on the bottom edge.
const CGFloat kPermissionImageYAdjust = 1;

// Spacing between a permission image and the text.
const CGFloat kPermissionImageSpacing = 3;

// The spacing between individual items in the Permissions tab.
const CGFloat kPermissionsTabSpacing = 12;

// Extra spacing after a headline on the Permissions tab.
const CGFloat kPermissionsHeadlineSpacing = 2;

// The amount of horizontal space between a permission label and the popup.
const CGFloat kPermissionPopUpXSpacing = 3;

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

// The amount of padding to *remove* when placing
// |IDS_WEBSITE_SETTINGS_{FIRST,THIRD}_PARTY_SITE_DATA| next to each other.
const CGFloat kTextLabelXPadding = 5;

// Takes in the parent window, which should be a BrowserWindow, and gets the
// proper anchor point for the bubble. The returned point is in screen
// coordinates.
NSPoint AnchorPointForWindow(NSWindow* parent) {
  BrowserWindowController* controller = [parent windowController];
  NSPoint origin = NSZeroPoint;
  if ([controller isKindOfClass:[BrowserWindowController class]]) {
    LocationBarViewMac* location_bar = [controller locationBarBridge];
    if (location_bar) {
      NSPoint bubble_point = location_bar->GetPageInfoBubblePoint();
      origin = [parent convertBaseToScreen:bubble_point];
    }
  }
  return origin;
}

}  // namespace

@interface WebsiteSettingsTabSegmentedCell : NSSegmentedCell {
 @private
  base::scoped_nsobject<NSImage> tabstripCenterImage_;
  base::scoped_nsobject<NSImage> tabstripLeftImage_;
  base::scoped_nsobject<NSImage> tabstripRightImage_;

  base::scoped_nsobject<NSImage> tabCenterImage_;
  base::scoped_nsobject<NSImage> tabLeftImage_;
  base::scoped_nsobject<NSImage> tabRightImage_;

  // Key track of the index of segment which has keyboard focus. This is not
  // the same as the currently selected segment.
  NSInteger keySegment_;
}

// The text attributes to use for the tab labels.
+ (NSDictionary*)textAttributes;

@end

@implementation WebsiteSettingsTabSegmentedCell

+ (NSDictionary*)textAttributes {
  NSFont* smallSystemFont =
      [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  return @{ NSFontAttributeName : smallSystemFont };
}

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
  NSRect interiorFrame = cellFrame;
  interiorFrame.size.width = 0;
  for (NSInteger i = 0; i < [self segmentCount]; ++i)
    interiorFrame.size.width += [self widthForSegment:i];
  [self drawInteriorWithFrame:interiorFrame inView:controlView];

  if ([[controlView window] firstResponder] == controlView)
    [self drawFocusRect];
}

- (void)drawFocusRect {
  gfx::ScopedNSGraphicsContextSaveGState scoped_state;
  NSSetFocusRingStyle(NSFocusRingOnly);
  [[NSColor keyboardFocusIndicatorColor] set];
  NSFrameRect([self hitRectForSegment:keySegment_]);
}

// Returns the segment number for the left-most positioned segment.
// On Right-to-Left languages, segment 0 is on the right.
- (NSInteger)leftSegment {
  BOOL isRTL = [self userInterfaceLayoutDirection] ==
                   NSUserInterfaceLayoutDirectionRightToLeft;
  return isRTL ? [self segmentCount] - 1 : 0;
}

// Return the hit rect (i.e., the visual bounds of the tab) for
// the given segment.
- (NSRect)hitRectForSegment:(NSInteger)segment {
  CGFloat tabstripHeight = [tabCenterImage_ size].height;
  DCHECK_GT(tabstripHeight, kTabHeight);
  DCHECK([self segmentCount] == 2);  // Assume 2 segments to keep things simple.
  NSInteger leftSegment = [self leftSegment];
  CGFloat xOrigin = segment == leftSegment ? kTabStripXPadding
                                           : [self widthForSegment:leftSegment];
  NSRect rect = NSMakeRect(xOrigin, tabstripHeight - kTabHeight,
                           [self widthForSegment:segment] - kTabStripXPadding,
                           kTabHeight);
  return NSInsetRect(rect, kTabSpacing / 2, 0);
}

- (void)drawSegment:(NSInteger)segment
            inFrame:(NSRect)tabFrame
           withView:(NSView*)controlView {
  // Adjust the tab's frame so that the label appears centered in the tab.
  if (segment == [self leftSegment])
    tabFrame.origin.x += kTabStripXPadding;
  tabFrame.size.width -= kTabStripXPadding;
  tabFrame.origin.y += kTabLabelTopPadding;
  tabFrame.size.height -= kTabLabelTopPadding;

  // Center the label's frame in the tab's frame.
  NSString* label = [self labelForSegment:segment];
  NSDictionary* textAttributes =
      [WebsiteSettingsTabSegmentedCell textAttributes];
  NSSize textSize = [label sizeWithAttributes:textAttributes];
  NSRect labelFrame;
  labelFrame.size = textSize;
  labelFrame.origin.x =
      tabFrame.origin.x + (NSWidth(tabFrame) - textSize.width) / 2.0;
  labelFrame.origin.y =
      tabFrame.origin.y + (NSHeight(tabFrame) - textSize.height) / 2.0;

  [label drawInRect:labelFrame withAttributes:textAttributes];
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
  base::scoped_nsobject<InfoBubbleWindow> window(
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
  presenter_.reset();
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
  base::string16 text = l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE);
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
  identityField_ = [self addText:base::string16()
                        withSize:[NSFont systemFontSize]
                            bold:YES
                          toView:contentView_
                         atPoint:controlOrigin];
  controlOrigin.y +=
      NSHeight([identityField_ frame]) + kConnectionHeadlineSpacing;

  // Create a text field to identity status (e.g. verified, not verified).
  identityStatusField_ = [self addText:base::string16()
                              withSize:[NSFont smallSystemFontSize]
                                  bold:NO
                                toView:contentView_
                               atPoint:controlOrigin];
  controlOrigin.y +=
      NSHeight([identityStatusField_ frame]) + kConnectionHeadlineSpacing;

  NSString* securityDetailsButtonText =
      l10n_util::GetNSString(IDS_WEBSITE_SETTINGS_DETAILS_LINK);
  securityDetailsButton_ = [self addLinkButtonWithText:securityDetailsButtonText
                                                toView:contentView_];
  [securityDetailsButton_ setTarget:self];
  [securityDetailsButton_ setAction:@selector(showSecurityPanel:)];

  // Create the tab view and its two tabs.

  base::scoped_nsobject<WebsiteSettingsTabSegmentedCell> cell(
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

  NSDictionary* textAttributes =
      [WebsiteSettingsTabSegmentedCell textAttributes];

  // Create the "Permissions" tab.
  NSString* label = l10n_util::GetNSString(
      IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS);
  [segmentedControl_ setLabel:label
                   forSegment:WebsiteSettingsUI::TAB_ID_PERMISSIONS];
  NSSize textSize = [label sizeWithAttributes:textAttributes];
  CGFloat tabWidth = textSize.width + 2 * kTabLabelXPadding;

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
  [segmentedControl_ setWidth:tabWidth + kTabStripXPadding
                   forSegment:WebsiteSettingsUI::TAB_ID_CONNECTION];

  [segmentedControl_ setFont:[textAttributes objectForKey:NSFontAttributeName]];
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
  base::scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  [tabView_ insertTabViewItem:item.get()
                      atIndex:WebsiteSettingsUI::TAB_ID_PERMISSIONS];
  base::scoped_nsobject<NSView> contentView(
      [[FlippedView alloc] initWithFrame:[tabView_ contentRect]]);
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

  // Create the link button to view site settings. Its position will be set in
  // performLayout.
  NSString* siteSettingsButtonText =
      l10n_util::GetNSString(IDS_PAGE_INFO_SITE_SETTINGS_LINK);
  siteSettingsButton_ =
      [self addLinkButtonWithText:siteSettingsButtonText toView:contentView];
  [siteSettingsButton_ setTarget:self];
  [siteSettingsButton_ setAction:@selector(showSiteSettingsData:)];

  return contentView.get();
}

// Handler for the link button below the list of cookies.
- (void)showCookiesAndSiteData:(id)sender {
  DCHECK(webContents_);
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_COOKIES_DIALOG_OPENED);
  TabDialogs::FromWebContents(webContents_)->ShowCollectedCookies();
}

// Handler for the site settings button below the list of permissions.
- (void)showSiteSettingsData:(id)sender {
  DCHECK(webContents_);
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_SITE_SETTINGS_OPENED);
  webContents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIContentSettingsURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
}

// Handler for the site settings button below the list of permissions.
- (void)showSecurityPanel:(id)sender {
  DCHECK(webContents_);
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_SECURITY_DETAILS_OPENED);
  DevToolsWindow::OpenDevToolsWindow(webContents_,
                                     DevToolsToggleAction::ShowSecurityPanel());
}

// Handler for the link button to show certificate information.
- (void)showCertificateInfo:(id)sender {
  DCHECK(certificateId_);
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_CERTIFICATE_DIALOG_OPENED);
  ShowCertificateViewerByID(webContents_, [self parentWindow], certificateId_);
}

// Handler for the link button to revoke user certificate decisions.
- (void)resetCertificateDecisions:(id)sender {
  DCHECK(resetDecisionsButton_);
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
  [self close];
}

// Handler for the link to show help information about the connection tab.
- (void)showHelpPage:(id)sender {
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_HELP_OPENED);
  webContents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
}

// Create the contents of the Connection tab and add it to the given tab view.
// Returns a weak reference to the tab view item's view.
- (NSView*)addConnectionTabToTabView:(NSTabView*)tabView {
  base::scoped_nsobject<NSTabViewItem> item([[NSTabViewItem alloc] init]);
  base::scoped_nsobject<NSView> contentView(
      [[FlippedView alloc] initWithFrame:[tabView_ contentRect]]);
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
      [self addText:base::string16()
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
      [self addText:base::string16()
           withSize:[NSFont smallSystemFontSize]
               bold:NO
             toView:contentView.get()
            atPoint:textPosition];
  certificateInfoButton_ = nil;  // This will be created only if necessary.
  resetDecisionsButton_ = nil;   // This will be created only if necessary.
  separatorAfterConnection_ = [self addSeparatorToView:contentView];
  [separatorAfterConnection_ setAutoresizingMask:NSViewWidthSizable];

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

  [siteSettingsButton_ setFrameOrigin:NSMakePoint(kFramePadding, yPos)];
  yPos = NSMaxY([identityStatusField_ frame]) + kConnectionHeadlineSpacing;
  yPos = [self setYPositionOfView:securityDetailsButton_ to:yPos];

  // Lay out the Permissions tab.

  yPos = [self setYPositionOfView:cookiesView_ to:kFramePadding];

  // Put the permission info just below the link button.
  yPos = [self setYPositionOfView:permissionsView_
                               to:NSMaxY([cookiesView_ frame]) + kFramePadding];

  // Put the link button for site settings just below the permissions.
  // TODO(palmer): set the position of this based on RTL/LTR.
  // http://code.google.com/p/chromium/issues/detail?id=525304
  [siteSettingsButton_ setFrameOrigin:NSMakePoint(kFramePadding, yPos)];

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
  if (resetDecisionsButton_) {
    NSRect resetDecisionsButtonFrame = [resetDecisionsButton_ frame];
    resetDecisionsButtonFrame.origin.x =
        NSMinX([identityStatusDescriptionField_ frame]);
    resetDecisionsButtonFrame.origin.y = yPos + kVerticalSpacing;
    [resetDecisionsButton_ setFrame:resetDecisionsButtonFrame];
    yPos = NSMaxY(resetDecisionsButtonFrame);
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

  [self setYPositionOfView:helpButton_ to:yPos];

  // Adjust the tab view size and place it below the identity status.

  yPos = NSMaxY([identityStatusField_ frame]) + kVerticalSpacing;
  yPos = [self setYPositionOfView:segmentedControl_ to:yPos];

  yPos = NSMaxY([securityDetailsButton_ frame]) + kTabStripTopSpacing;
  yPos = [self setYPositionOfView:segmentedControl_ to:yPos];

  CGFloat connectionTabHeight = NSMaxY([helpButton_ frame]) + kVerticalSpacing;

  NSRect tabViewFrame = [tabView_ frame];
  tabViewFrame.origin.y = yPos;
  tabViewFrame.size.height = std::max(
      connectionTabHeight, NSMaxY([siteSettingsButton_ frame]) + kFramePadding);
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
  [self setAnchorPoint:AnchorPointForWindow([self parentWindow])];
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
- (NSTextField*)addText:(const base::string16&)text
               withSize:(CGFloat)fontSize
                   bold:(BOOL)bold
                 toView:(NSView*)view
                atPoint:(NSPoint)point {
  // Size the text to take up the full available width, with some padding.
  // The height is arbitrary as it will be adjusted later.
  CGFloat width = NSWidth([view frame]) - point.x - kFramePadding;
  NSRect frame = NSMakeRect(point.x, point.y, width, 100);
  base::scoped_nsobject<NSTextField> textField(
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
  base::scoped_nsobject<NSImageView> imageView(
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
  NSBox* spacer = [self horizontalSeparatorWithFrame:NSMakeRect(
      kFramePadding, 0, width, 0)];
  [view addSubview:spacer];
  return spacer;
}

// Add a link button with the given text to |view|.
- (NSButton*)addLinkButtonWithText:(NSString*)text toView:(NSView*)view {
  // Frame size is arbitrary; it will be adjusted by the layout tweaker.
  NSRect frame = NSMakeRect(kFramePadding, 0, 100, 10);
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:frame]);
  base::scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:text]);
  [cell setControlSize:NSSmallControlSize];
  [button setCell:cell.get()];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [view addSubview:button.get()];

  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];
  return button.get();
}

// Add a button with the given text to |view| setting the max size appropriately
// for the connection info section.
- (NSButton*)addButtonWithTextToConnectionSection:(NSString*)text
                                           toView:(NSView*)view {
  NSRect containerFrame = [view frame];
  // Frame size is arbitrary; it will be adjusted by the layout tweaker.
  NSRect frame = NSMakeRect(kFramePadding, 0, 100, 10);
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:frame]);

  // Determine the largest possible size for this button. The size is the width
  // of the connection section minus the padding on both sides minus the
  // connection image size and spacing.
  CGFloat maxTitleWidth = containerFrame.size.width - kFramePadding * 2 -
                          kConnectionImageSize - kConnectionImageSpacing;

  base::scoped_nsobject<NSButtonCell> cell(
      [[NSButtonCell alloc] initTextCell:text]);
  [button setCell:cell.get()];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];

  // Ensure the containing view is large enough to contain the button with its
  // widest possible title.
  NSRect buttonFrame = [button frame];
  buttonFrame.size.width = maxTitleWidth;

  [button setFrame:buttonFrame];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [view addSubview:button.get()];

  return button.get();
}

// Add a pop-up button for |permissionInfo| to the given view.
- (NSPopUpButton*)addPopUpButtonForPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                                       toView:(NSView*)view
                                      atPoint:(NSPoint)point {

  GURL url = webContents_ ? webContents_->GetURL() : GURL();
  __block WebsiteSettingsBubbleController* weakSelf = self;
  PermissionMenuModel::ChangeCallback callback =
      base::BindBlock(^(const WebsiteSettingsUI::PermissionInfo& permission) {
          [weakSelf onPermissionChanged:permission.type to:permission.setting];
      });
  base::scoped_nsobject<PermissionSelectorButton> button(
      [[PermissionSelectorButton alloc] initWithPermissionInfo:permissionInfo
                                                        forURL:url
                                                  withCallback:callback]);
  // Determine the largest possible size for this button.
  CGFloat maxTitleWidth = [button
      maxTitleWidthForContentSettingsType:permissionInfo.type
                       withDefaultSetting:permissionInfo.default_setting];

  // Ensure the containing view is large enough to contain the button with its
  // widest possible title.
  NSRect containerFrame = [view frame];
  containerFrame.size.width = std::max(
      NSWidth(containerFrame), point.x + maxTitleWidth + kFramePadding);
  [view setFrame:containerFrame];
  [view addSubview:button.get()];
  return button.get();
}

// Called when the user changes the setting of a permission.
- (void)onPermissionChanged:(ContentSettingsType)permissionType
                         to:(ContentSetting)newSetting {
  if (presenter_)
    presenter_->OnSitePermissionChanged(permissionType, newSetting);
}

// Called when the user changes the selected segment in the segmented control.
- (void)tabSelected:(id)sender {
  NSInteger index = [segmentedControl_ selectedSegment];
  switch (index) {
    case WebsiteSettingsUI::TAB_ID_PERMISSIONS:
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_PERMISSIONS_TAB_SELECTED);
      break;
    case WebsiteSettingsUI::TAB_ID_CONNECTION:
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_TAB_SELECTED);
      break;
    default:
      NOTREACHED();
  }
  [tabView_ selectTabViewItemAtIndex:index];
}

// Adds a new row to the UI listing the permissions. Returns the NSPoint of the
// last UI element added (either the permission button, in LTR, or the text
// label, in RTL).
- (NSPoint)addPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                  toView:(NSView*)view
                 atPoint:(NSPoint)point {
  base::string16 labelText =
      WebsiteSettingsUI::PermissionTypeToUIString(permissionInfo.type) +
      base::ASCIIToUTF16(":");
  bool isRTL =
      base::i18n::RIGHT_TO_LEFT == base::i18n::GetStringDirection(labelText);
  base::scoped_nsobject<NSImage> image(
      [WebsiteSettingsUI::GetPermissionIcon(permissionInfo).ToNSImage()
          retain]);

  NSPoint position;
  NSImageView* imageView;
  NSPopUpButton* button;
  NSTextField* label;

  CGFloat viewWidth = NSWidth([view frame]);

  if (isRTL) {
    point.x = NSWidth([view frame]) - kPermissionImageSize -
              kPermissionImageSpacing - kFramePadding;
    imageView = [self addImageWithSize:[image size] toView:view atPoint:point];
    [imageView setImage:image];
    point.x -= kPermissionImageSpacing;

    label = [self addText:labelText
                 withSize:[NSFont smallSystemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];
    point.x -= NSWidth([label frame]);
    [label setFrameOrigin:point];

    position = NSMakePoint(point.x, point.y);
    button = [self addPopUpButtonForPermission:permissionInfo
                                        toView:view
                                       atPoint:position];
    position.x -= NSWidth([button frame]);
    [button setFrameOrigin:position];
  } else {
    imageView = [self addImageWithSize:[image size] toView:view atPoint:point];
    [imageView setImage:image];
    point.x += kPermissionImageSize + kPermissionImageSpacing;

    label = [self addText:labelText
                 withSize:[NSFont smallSystemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];

    position = NSMakePoint(NSMaxX([label frame]), point.y);
    button = [self addPopUpButtonForPermission:permissionInfo
                                        toView:view
                                       atPoint:position];
  }

  [view setFrameSize:NSMakeSize(viewWidth, NSHeight([view frame]))];

  // Adjust the vertical position of the button so that its title text is
  // aligned with the label. Assumes that the text is the same size in both.
  // Also adjust the horizontal position to remove excess space due to the
  // invisible bezel.
  NSRect titleRect = [[button cell] titleRectForBounds:[button bounds]];
  if (isRTL) {
    position.x += kPermissionPopUpXSpacing;
  } else {
    position.x -= titleRect.origin.x - kPermissionPopUpXSpacing;
  }
  position.y -= titleRect.origin.y;
  [button setFrameOrigin:position];

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

// Set the content of the identity and identity status fields.
- (void)setIdentityInfo:(const WebsiteSettingsUI::IdentityInfo&)identityInfo {
  [identityField_ setStringValue:
      base::SysUTF8ToNSString(identityInfo.site_identity)];
  [identityStatusField_ setStringValue:base::SysUTF16ToNSString(
                                           identityInfo.GetSecuritySummary())];

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

    // Check if a security decision has been made, and if so, add a button to
    // allow the user to retract their decision.
    if (identityInfo.show_ssl_decision_revoke_button) {
      NSString* text = l10n_util::GetNSString(
          IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON);
      resetDecisionsButton_ =
          [self addButtonWithTextToConnectionSection:text
                                              toView:connectionTabContentView_];
      [resetDecisionsButton_ setTarget:self];
      [resetDecisionsButton_ setAction:@selector(resetCertificateDecisions:)];
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
  // A result of re-ordering of the permissions (crbug.com/444244) is
  // that sometimes permissions may not be displayed at all, so it's
  // incorrect to check they are set before the cookie info.

  // |cookieInfoList| should only ever have 2 items: first- and third-party
  // cookies.
  DCHECK_EQ(cookieInfoList.size(), 2u);
  base::string16 firstPartyLabelText;
  base::string16 thirdPartyLabelText;
  for (const auto& i : cookieInfoList) {
    if (i.is_first_party) {
      firstPartyLabelText =
          l10n_util::GetStringFUTF16(IDS_WEBSITE_SETTINGS_FIRST_PARTY_SITE_DATA,
                                     base::IntToString16(i.allowed));
    } else {
      thirdPartyLabelText =
          l10n_util::GetStringFUTF16(IDS_WEBSITE_SETTINGS_THIRD_PARTY_SITE_DATA,
                                     base::IntToString16(i.allowed));
    }
  }

  base::string16 sectionTitle =
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  bool isRTL = base::i18n::RIGHT_TO_LEFT ==
               base::i18n::GetStringDirection(firstPartyLabelText);

  [cookiesView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kFramePadding, 0);

  NSTextField* label;

  CGFloat viewWidth = NSWidth([cookiesView_ frame]);

  NSTextField* header = [self addText:sectionTitle
                             withSize:[NSFont smallSystemFontSize]
                                 bold:YES
                               toView:cookiesView_
                              atPoint:controlOrigin];
  [header sizeToFit];

  if (isRTL) {
    controlOrigin.x = viewWidth - kFramePadding - NSWidth([header frame]);
    [header setFrameOrigin:controlOrigin];
  }
  controlOrigin.y += NSHeight([header frame]) + kPermissionsHeadlineSpacing;
  controlOrigin.y += kPermissionsTabSpacing;

  // Reset X for the cookie image.
  if (isRTL) {
    controlOrigin.x = viewWidth - kPermissionImageSize -
                      kPermissionImageSpacing - kFramePadding;
  }

  WebsiteSettingsUI::PermissionInfo info;
  info.type = CONTENT_SETTINGS_TYPE_COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  NSImage* image = WebsiteSettingsUI::GetPermissionIcon(info).ToNSImage();
  NSImageView* imageView = [self addImageWithSize:[image size]
                                           toView:cookiesView_
                                          atPoint:controlOrigin];
  [imageView setImage:image];

  base::string16 comma = base::ASCIIToUTF16(", ");
  NSString* cookieButtonText = base::SysUTF16ToNSString(firstPartyLabelText);

  if (isRTL) {
    NSButton* cookiesButton =
        [self addLinkButtonWithText:cookieButtonText toView:cookiesView_];
    [cookiesButton setTarget:self];
    [cookiesButton setAction:@selector(showCookiesAndSiteData:)];
    controlOrigin.x -= NSWidth([cookiesButton frame]);
    [cookiesButton setFrameOrigin:controlOrigin];

    label = [self addText:comma + thirdPartyLabelText
                 withSize:[NSFont smallSystemFontSize]
                     bold:NO
                   toView:cookiesView_
                  atPoint:controlOrigin];
    [label sizeToFit];
    controlOrigin.x -= NSWidth([label frame]) - kTextLabelXPadding;
    [label setFrameOrigin:controlOrigin];
  } else {
    controlOrigin.x += kPermissionImageSize + kPermissionImageSpacing;

    NSButton* cookiesButton =
        [self addLinkButtonWithText:cookieButtonText toView:cookiesView_];
    [cookiesButton setTarget:self];
    [cookiesButton setAction:@selector(showCookiesAndSiteData:)];
    [cookiesButton setFrameOrigin:controlOrigin];

    controlOrigin.x += NSWidth([cookiesButton frame]) - kTextLabelXPadding;

    label = [self addText:comma + thirdPartyLabelText
                 withSize:[NSFont smallSystemFontSize]
                     bold:NO
                   toView:cookiesView_
                  atPoint:controlOrigin];
    [label sizeToFit];
  }

  // Align the icon with the text.
  [self alignPermissionIcon:imageView withTextField:label];

  controlOrigin.y += NSHeight([label frame]) + kPermissionsTabSpacing;

  [cookiesView_ setFrameSize:
      NSMakeSize(NSWidth([cookiesView_ frame]), controlOrigin.y)];

  [self performLayout];
}

- (void)setPermissionInfo:(const PermissionInfoList&)permissionInfoList {
  [permissionsView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kFramePadding, 0);

  if (permissionInfoList.size() > 0) {
    base::string16 sectionTitle = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
    bool isRTL = base::i18n::RIGHT_TO_LEFT ==
                 base::i18n::GetStringDirection(sectionTitle);
    NSTextField* header = [self addText:sectionTitle
                               withSize:[NSFont smallSystemFontSize]
                                   bold:YES
                                 toView:permissionsView_
                                atPoint:controlOrigin];
    [header sizeToFit];
    if (isRTL) {
      controlOrigin.x = NSWidth([permissionsView_ frame]) - kFramePadding -
                        NSWidth([header frame]);
      [header setFrameOrigin:controlOrigin];
    }
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
  }

  [permissionsView_ setFrameSize:
      NSMakeSize(NSWidth([permissionsView_ frame]), controlOrigin.y)];
  [self performLayout];
}

- (void)setSelectedTab:(WebsiteSettingsUI::TabId)tabId {
  NSInteger index = static_cast<NSInteger>(tabId);
  [segmentedControl_ setSelectedSegment:index];
  [tabView_ selectTabViewItemAtIndex:index];
}

@end

WebsiteSettingsUIBridge::WebsiteSettingsUIBridge(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      bubble_controller_(nil) {}

WebsiteSettingsUIBridge::~WebsiteSettingsUIBridge() {
}

void WebsiteSettingsUIBridge::set_bubble_controller(
    WebsiteSettingsBubbleController* controller) {
  bubble_controller_ = controller;
}

void WebsiteSettingsUIBridge::Show(
    gfx::NativeWindow parent,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const SecurityStateModel::SecurityInfo& security_info) {
  if (chrome::ToolkitViewsDialogsEnabled()) {
    chrome::ShowWebsiteSettingsBubbleViewsAtPoint(
        gfx::ScreenPointFromNSPoint(AnchorPointForWindow(parent)), profile,
        web_contents, url, security_info);
    return;
  }

  bool is_internal_page = InternalChromePage(url);

  // Create the bridge. This will be owned by the bubble controller.
  WebsiteSettingsUIBridge* bridge = new WebsiteSettingsUIBridge(web_contents);

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
        bridge, profile,
        TabSpecificContentSettings::FromWebContents(web_contents), web_contents,
        url, security_info, content::CertStore::GetInstance());
    [bubble_controller setPresenter:presenter];
  }

  [bubble_controller showWindow:nil];
}

void WebsiteSettingsUIBridge::SetIdentityInfo(
    const WebsiteSettingsUI::IdentityInfo& identity_info) {
  [bubble_controller_ setIdentityInfo:identity_info];
}

void WebsiteSettingsUIBridge::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == web_contents_->GetMainFrame()) {
    [bubble_controller_ close];
  }
}

void WebsiteSettingsUIBridge::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  [bubble_controller_ setCookieInfo:cookie_info_list];
}

void WebsiteSettingsUIBridge::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  [bubble_controller_ setPermissionInfo:permission_info_list];
}

void WebsiteSettingsUIBridge::SetSelectedTab(TabId tab_id) {
  [bubble_controller_ setSelectedTab:tab_id];
}
