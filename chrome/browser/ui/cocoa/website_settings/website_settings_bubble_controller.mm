// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"

#import <AppKit/AppKit.h>

#include <cmath>

#include "base/i18n/rtl.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_selector_button.h"
#import "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/a11y_util.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/resources/grit/ui_resources.h"

using ChosenObjectInfoPtr =
    std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo>;
using ChosenObjectDeleteCallback =
    base::Callback<void(const WebsiteSettingsUI::ChosenObjectInfo&)>;

namespace {

// General ---------------------------------------------------------------------

// The default width of the window, in view coordinates. It may be larger to
// fit the content.
const CGFloat kDefaultWindowWidth = 320;

// Padding around each section
const CGFloat kSectionVerticalPadding = 20;
const CGFloat kSectionHorizontalPadding = 16;

// Links are buttons with invisible padding, so we need to move them back to
// align with other text.
const CGFloat kLinkButtonXAdjustment = 1;

// Security Section ------------------------------------------------------------

// Spacing between security summary, security details, and cert decisions text.
const CGFloat kSecurityParagraphSpacing = 12;

// Site Settings Section -------------------------------------------------------

// Square size of the permission images.
const CGFloat kPermissionImageSize = 16;

// Spacing between a permission image and the text.
const CGFloat kPermissionImageSpacing = 6;

// Minimum distance between the label and its corresponding menu.
const CGFloat kMinSeparationBetweenLabelAndMenu = 16;

// Square size of the permission delete button image.
const CGFloat kPermissionDeleteImageSize = 16;

// The spacing between individual permissions.
const CGFloat kPermissionsVerticalSpacing = 16;

// Amount to lower each permission icon to align the icon baseline with the
// label text.
const CGFloat kPermissionIconYAdjustment = 1;

// Amount to lower each permission popup button to make its text align with the
// permission label.
const CGFloat kPermissionPopupButtonYAdjustment = 3;

// Internal Page Bubble --------------------------------------------------------

// Padding between the window frame and content for the internal page bubble.
const CGFloat kInternalPageFramePadding = 10;

// Spacing between the image and text for internal pages.
const CGFloat kInternalPageImageSpacing = 10;

// -----------------------------------------------------------------------------

// NOTE: This assumes that there will never be more than one website settings
// popup shown, and that the one that is shown is associated with the current
// window. This matches the behaviour in views: see WebsiteSettingsPopupView.
bool g_is_popup_showing = false;

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
      origin = ui::ConvertPointFromWindowToScreen(parent, bubble_point);
    }
  }
  return origin;
}

}  // namespace

@interface ChosenObjectDeleteButton : HoverImageButton {
 @private
  ChosenObjectInfoPtr objectInfo_;
  ChosenObjectDeleteCallback callback_;
}

// Designated initializer. Takes ownership of |objectInfo|.
- (instancetype)initWithChosenObject:(ChosenObjectInfoPtr)objectInfo
                             atPoint:(NSPoint)point
                        withCallback:(ChosenObjectDeleteCallback)callback;

// Action when the button is clicked.
- (void)deleteClicked:(id)sender;

@end

@implementation ChosenObjectDeleteButton

- (instancetype)initWithChosenObject:(ChosenObjectInfoPtr)objectInfo
                             atPoint:(NSPoint)point
                        withCallback:(ChosenObjectDeleteCallback)callback {
  NSRect frame = NSMakeRect(point.x, point.y, kPermissionDeleteImageSize,
                            kPermissionDeleteImageSize);
  if (self = [super initWithFrame:frame]) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    [self setDefaultImage:rb.GetNativeImageNamed(IDR_CLOSE_2).ToNSImage()];
    [self setHoverImage:rb.GetNativeImageNamed(IDR_CLOSE_2_H).ToNSImage()];
    [self setPressedImage:rb.GetNativeImageNamed(IDR_CLOSE_2_P).ToNSImage()];
    [self setBordered:NO];
    [self setToolTip:l10n_util::GetNSString(
                         objectInfo->ui_info.delete_tooltip_string_id)];
    [self setTarget:self];
    [self setAction:@selector(deleteClicked:)];
    objectInfo_ = std::move(objectInfo);
    callback_ = callback;
  }
  return self;
}

- (void)deleteClicked:(id)sender {
  callback_.Run(*objectInfo_);
}

@end

@implementation WebsiteSettingsBubbleController

- (CGFloat)defaultWindowWidth {
  return kDefaultWindowWidth;
}

bool IsInternalURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeDevToolsScheme) ||
         url.SchemeIs(extensions::kExtensionScheme) ||
         url.SchemeIs(content::kViewSourceScheme);
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
    websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge
                webContents:(content::WebContents*)webContents
                        url:(const GURL&)url {
  DCHECK(parentWindow);

  webContents_ = webContents;
  permissionsPresent_ = NO;
  url_ = url;

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
    [[self bubble] setArrowLocation:info_bubble::kTopLeading];

    // Create the container view that uses flipped coordinates.
    NSRect contentFrame = NSMakeRect(0, 0, [self defaultWindowWidth], 300);
    contentView_.reset(
        [[FlippedView alloc] initWithFrame:contentFrame]);

    // Replace the window's content.
    [[[self window] contentView] setSubviews:
        [NSArray arrayWithObject:contentView_.get()]];

    if (IsInternalURL(url_)) {
      [self initializeContentsForInternalPage:url_];
    } else {
      [self initializeContents];
    }

    bridge_.reset(bridge);
    bridge_->set_bubble_controller(self);
  }
  return self;
}

- (LocationBarDecoration*)decorationForBubble {
  BrowserWindowController* controller = [[self parentWindow] windowController];
  LocationBarViewMac* location_bar = [controller locationBarBridge];
  return location_bar ? location_bar->GetPageInfoDecoration() : nullptr;
}

- (Profile*)profile {
  return Profile::FromBrowserContext(webContents_->GetBrowserContext());
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
- (void)initializeContentsForInternalPage:(const GURL&)url {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  int text = IDS_PAGE_INFO_INTERNAL_PAGE;
  int icon = IDR_PRODUCT_LOGO_16;
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    text = IDS_PAGE_INFO_EXTENSION_PAGE;
    icon = IDR_PLUGINS_FAVICON;
  } else if (url.SchemeIs(content::kViewSourceScheme)) {
    text = IDS_PAGE_INFO_VIEW_SOURCE_PAGE;
    // view-source scheme uses the same icon as chrome:// pages.
    icon = IDR_PRODUCT_LOGO_16;
  } else if (!url.SchemeIs(content::kChromeUIScheme) &&
             !url.SchemeIs(content::kChromeDevToolsScheme)) {
    NOTREACHED();
  }

  NSPoint controlOrigin =
      NSMakePoint(kInternalPageFramePadding,
                  kInternalPageFramePadding + info_bubble::kBubbleArrowHeight);
  NSImage* productLogoImage = rb.GetNativeImageNamed(icon).ToNSImage();
  NSImageView* imageView = [self addImageWithSize:[productLogoImage size]
                                           toView:contentView_
                                          atPoint:controlOrigin];
  [imageView setImage:productLogoImage];

  NSRect imageFrame = [imageView frame];
  controlOrigin.x += NSWidth(imageFrame) + kInternalPageImageSpacing;
  NSTextField* textField = [self addText:l10n_util::GetStringUTF16(text)
                                withSize:[NSFont smallSystemFontSize]
                                    bold:NO
                                  toView:contentView_
                                 atPoint:controlOrigin];
  // Center the image vertically with the text. Previously this code centered
  // the text vertically while holding the image in place. That produced correct
  // results when the image, at 26x26, was taller than (or just slightly
  // shorter) than the text, but produced incorrect results once the icon
  // shrank to 16x16. The icon should now always be shorter than the text.
  // See crbug.com/572044 .
  NSRect textFrame = [textField frame];
  imageFrame.origin.y += (NSHeight(textFrame) - NSHeight(imageFrame)) / 2;
  [imageView setFrame:imageFrame];

  // Adjust the contentView to fit everything.
  CGFloat maxY = std::max(NSMaxY(imageFrame), NSMaxY(textFrame));
  [contentView_ setFrame:NSMakeRect(
      0, 0, [self defaultWindowWidth], maxY + kInternalPageFramePadding)];

  [self sizeAndPositionWindow];
}

// Create the subviews for the website settings bubble.
- (void)initializeContents {
  securitySectionView_ = [self addSecuritySectionToView:contentView_];
  separatorAfterSecuritySection_ = [self addSeparatorToView:contentView_];
  siteSettingsSectionView_ = [self addSiteSettingsSectionToView:contentView_];

  [self performLayout];
}

// Create and return a subview for the security section and add it to the given
// |superview|. |superview| retains the new view.
- (NSView*)addSecuritySectionToView:(NSView*)superview {
  base::scoped_nsobject<NSView> securitySectionView(
      [[FlippedView alloc] initWithFrame:[superview frame]]);
  [superview addSubview:securitySectionView];

  // Create a controlOrigin to place the text fields. The y value doesn't
  // matter, because the correct value is calculated in -performLayout.
  NSPoint controlOrigin = NSMakePoint(kSectionHorizontalPadding, 0);

  // Create a text field for the security summary (private/not private/etc.).
  securitySummaryField_ = [self addText:base::string16()
                               withSize:[NSFont systemFontSize]
                                   bold:NO
                                 toView:securitySectionView
                                atPoint:controlOrigin];

  securityDetailsField_ = [self addText:base::string16()
                               withSize:[NSFont smallSystemFontSize]
                                   bold:NO
                                 toView:securitySectionView
                                atPoint:controlOrigin];

  // These will be created only if necessary.
  resetDecisionsField_ = nil;
  resetDecisionsButton_ = nil;

  NSString* connectionHelpButtonText =
      l10n_util::GetNSString(IDS_LEARN_MORE);
  connectionHelpButton_ = [self addLinkButtonWithText:connectionHelpButtonText
                                                toView:securitySectionView];
  [connectionHelpButton_ setTarget:self];
  [connectionHelpButton_ setAction:@selector(openConnectionHelp:)];

  return securitySectionView.get();
}

// Create and return a subview for the site settings and add it to the given
// |superview|. |superview| retains the new view.
- (NSView*)addSiteSettingsSectionToView:(NSView*)superview {
  base::scoped_nsobject<NSView> siteSettingsSectionView(
      [[FlippedView alloc] initWithFrame:[superview frame]]);
  [superview addSubview:siteSettingsSectionView];

  // Initialize the two containers that hold the controls. The initial frames
  // are arbitrary, and will be adjusted after the controls are laid out.
  cookiesView_ =
      [[[FlippedView alloc] initWithFrame:[superview frame]] autorelease];
  [cookiesView_ setAutoresizingMask:NSViewWidthSizable];
  [siteSettingsSectionView addSubview:cookiesView_];

  permissionsView_ =
      [[[FlippedView alloc] initWithFrame:[superview frame]] autorelease];
  [siteSettingsSectionView addSubview:permissionsView_];

  // Create the link button to view site settings. Its position will be set in
  // performLayout.
  NSString* siteSettingsButtonText =
      l10n_util::GetNSString(IDS_PAGE_INFO_SITE_SETTINGS_LINK);
  siteSettingsButton_ = [self addButtonWithText:siteSettingsButtonText
                                         toView:siteSettingsSectionView];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:siteSettingsButton_];

  [siteSettingsButton_ setTarget:self];
  [siteSettingsButton_ setAction:@selector(showSiteSettingsData:)];

  return siteSettingsSectionView.get();
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
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

// TODO(lgarron): Move some of this to the presenter for separation of concerns
// and platform unification. (https://crbug.com/571533)
- (void)openConnectionHelp:(id)sender {
  DCHECK(webContents_);
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_HELP_OPENED);
  webContents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

// Handler for the link button to show certificate information.
- (void)showCertificateInfo:(id)sender {
  DCHECK(certificate_.get());
  DCHECK(presenter_);
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_CERTIFICATE_DIALOG_OPENED);
  ShowCertificateViewer(webContents_, [self parentWindow], certificate_.get());
}

// Handler for the link button to revoke user certificate decisions.
- (void)resetCertificateDecisions:(id)sender {
  DCHECK(resetDecisionsButton_);
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
  [self close];
}

- (CGFloat)layoutViewAtRTLStart:(NSView*)view withYPosition:(CGFloat)yPos {
  CGFloat xPos;
  if (base::i18n::IsRTL()) {
    xPos =
        kDefaultWindowWidth - kSectionHorizontalPadding - NSWidth([view frame]);
  } else {
    xPos = kSectionHorizontalPadding;
  }
  [view setFrameOrigin:NSMakePoint(xPos, yPos)];
  return yPos + NSHeight([view frame]);
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

- (void)setHeightOfView:(NSView*)view to:(CGFloat)height {
  [view setFrameSize:NSMakeSize(NSWidth([view frame]), height)];
}

// Layout all of the controls in the window. This should be called whenever
// the content has changed.
- (void)performLayout {
  // Make the content at least as wide as the permissions view.
  CGFloat contentWidth = std::max([self defaultWindowWidth],
                                  NSWidth([permissionsView_ frame]));

  // Set the width of the content view now, so that all the text fields will
  // be sized to fit before their heights and vertical positions are adjusted.
  [self setWidthOfView:contentView_ to:contentWidth];
  [self setWidthOfView:securitySectionView_ to:contentWidth];
  [self setWidthOfView:siteSettingsSectionView_ to:contentWidth];

  CGFloat yPos = 0;

  [self layoutSecuritySection];
  yPos = [self setYPositionOfView:securitySectionView_ to:yPos];

  yPos = [self setYPositionOfView:separatorAfterSecuritySection_ to:yPos];

  [self layoutSiteSettingsSection];
  yPos = [self setYPositionOfView:siteSettingsSectionView_ to:yPos];

  [contentView_ setFrame:NSMakeRect(0, 0, NSWidth([contentView_ frame]), yPos)];

  [self sizeAndPositionWindow];
}

- (void)layoutSecuritySection {
  // Start the layout with the first element. Margins are handled by the caller.
  CGFloat yPos = 0;

  [self sizeTextFieldHeightToFit:securitySummaryField_];
  yPos = [self setYPositionOfView:securitySummaryField_
                               to:yPos + kSectionVerticalPadding];

  [self sizeTextFieldHeightToFit:securityDetailsField_];
  yPos = [self setYPositionOfView:securityDetailsField_
                               to:yPos + kSecurityParagraphSpacing];

  [connectionHelpButton_
      setFrameOrigin:NSMakePoint(
                         kSectionHorizontalPadding - kLinkButtonXAdjustment,
                         yPos)];
  yPos = NSMaxY([connectionHelpButton_ frame]);

  if (resetDecisionsButton_) {
    DCHECK(resetDecisionsField_);
    yPos = [self setYPositionOfView:resetDecisionsField_
                                 to:yPos + kSecurityParagraphSpacing];
    [resetDecisionsButton_
        setFrameOrigin:NSMakePoint(NSMinX([resetDecisionsButton_ frame]) -
                                       kLinkButtonXAdjustment,
                                   yPos)];
    yPos = NSMaxY([resetDecisionsButton_ frame]);
  }

  // Resize the height based on contents.
  [self setHeightOfView:securitySectionView_ to:yPos + kSectionVerticalPadding];
}

- (void)layoutSiteSettingsSection {
  // Start the layout with the first element. Margins are handled by the caller.
  CGFloat yPos = 0;

  yPos =
      [self setYPositionOfView:cookiesView_ to:yPos + kSectionVerticalPadding];

  if (permissionsPresent_) {
    // Put the permission info just below the link button.
    yPos = [self setYPositionOfView:permissionsView_ to:yPos];
  }

  yPos = [self layoutViewAtRTLStart:siteSettingsButton_ withYPosition:yPos];

  // Resize the height based on contents.
  [self setHeightOfView:siteSettingsSectionView_
                     to:yPos + kSectionVerticalPadding];
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
  CGFloat width = NSWidth([view frame]) - point.x - kSectionHorizontalPadding;
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
// position and the given y position. The image is not in the accessibility
// order, since the image is always accompanied by text in this bubble. Return
// the new NSImageView.
- (NSImageView*)addImageWithSize:(NSSize)size
                          toView:(NSView*)view
                         atPoint:(NSPoint)point {
  NSRect frame = NSMakeRect(point.x, point.y, size.width, size.height);
  base::scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:frame]);
  ui::a11y_util::HideImageFromAccessibilityOrder(imageView);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [view addSubview:imageView.get()];
  return imageView.get();
}

// Add a separator as a subview of the given view. Return the new view.
- (NSView*)addSeparatorToView:(NSView*)view {
  // Use an arbitrary position; it will be adjusted in performLayout.
  NSBox* spacer = [self
      horizontalSeparatorWithFrame:NSMakeRect(0, 0, NSWidth([view frame]), 0)];
  [view addSubview:spacer];
  return spacer;
}

// Add a link button with the given text to |view|.
- (NSButton*)addLinkButtonWithText:(NSString*)text toView:(NSView*)view {
  // Frame size is arbitrary; it will be adjusted by the layout tweaker.
  NSRect frame = NSMakeRect(kSectionHorizontalPadding, 0, 100, 10);
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

// Create and return a button with the specified text and add it to the given
// |view|. |view| retains the new button.
- (NSButton*)addButtonWithText:(NSString*)text toView:(NSView*)view {
  NSRect containerFrame = [view frame];
  // Frame size is arbitrary; it will be adjusted by the layout tweaker.
  NSRect frame = NSMakeRect(kSectionHorizontalPadding, 0, 100, 10);
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:frame]);

  // Determine the largest possible size for this button. The size is the width
  // of the connection section minus the padding on both sides minus the
  // connection image size and spacing.
  // TODO(lgarron): handle this sizing in -performLayout.
  CGFloat maxTitleWidth =
      containerFrame.size.width - kSectionHorizontalPadding * 2;

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

// Set the content of the identity and identity status fields.
- (void)setIdentityInfo:(const WebsiteSettingsUI::IdentityInfo&)identityInfo {
  std::unique_ptr<WebsiteSettingsUI::SecurityDescription> security_description =
      identityInfo.GetSecurityDescription();
  [securitySummaryField_
      setStringValue:base::SysUTF16ToNSString(security_description->summary)];

  [securityDetailsField_
      setStringValue:SysUTF16ToNSString(security_description->details)];

  certificate_ = identityInfo.certificate;

  if (certificate_ &&  identityInfo.show_ssl_decision_revoke_button) {
    resetDecisionsField_ =
        [self addText:base::string16()
             withSize:[NSFont smallSystemFontSize]
                 bold:NO
               toView:securitySectionView_
              atPoint:NSMakePoint(kSectionHorizontalPadding, 0)];
    [resetDecisionsField_
        setStringValue:l10n_util::GetNSString(
                           IDS_PAGEINFO_INVALID_CERTIFICATE_DESCRIPTION)];
    [self sizeTextFieldHeightToFit:resetDecisionsField_];

    resetDecisionsButton_ =
        [self addLinkButtonWithText:
                  l10n_util::GetNSString(
                      IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON)
                             toView:securitySectionView_];
    [resetDecisionsButton_ setTarget:self];
    [resetDecisionsButton_ setAction:@selector(resetCertificateDecisions:)];
  }

  [self performLayout];
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
                                                  withCallback:callback
                                                       profile:[self profile]]);

  // Determine the largest possible size for this button.
  CGFloat maxTitleWidth =
      [button maxTitleWidthForContentSettingsType:permissionInfo.type
                               withDefaultSetting:permissionInfo.default_setting
                                          profile:[self profile]];

  // Ensure the containing view is large enough to contain the button with its
  // widest possible title.
  NSRect containerFrame = [view frame];
  containerFrame.size.width =
      std::max(NSWidth(containerFrame),
               point.x + maxTitleWidth + kSectionHorizontalPadding);
  [view setFrame:containerFrame];
  [view addSubview:button.get()];
  return button.get();
}

// Add a delete button for |objectInfo| to the given view.
- (NSButton*)addDeleteButtonForChosenObject:(ChosenObjectInfoPtr)objectInfo
                                     toView:(NSView*)view
                                    atPoint:(NSPoint)point {
  __block WebsiteSettingsBubbleController* weakSelf = self;
  auto callback =
      base::BindBlock(^(const WebsiteSettingsUI::ChosenObjectInfo& objectInfo) {
        [weakSelf onChosenObjectDeleted:objectInfo];
      });
  base::scoped_nsobject<ChosenObjectDeleteButton> button(
      [[ChosenObjectDeleteButton alloc]
          initWithChosenObject:std::move(objectInfo)
                       atPoint:point
                  withCallback:callback]);

  // Ensure the containing view is large enough to contain the button.
  NSRect containerFrame = [view frame];
  containerFrame.size.width =
      std::max(NSWidth(containerFrame), point.x + kPermissionDeleteImageSize +
                                            kSectionHorizontalPadding);
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

// Called when the user revokes permission for a previously chosen object.
- (void)onChosenObjectDeleted:(const WebsiteSettingsUI::ChosenObjectInfo&)info {
  if (presenter_)
    presenter_->OnSiteChosenObjectDeleted(info.ui_info, *info.object);
}

// Adds a new row to the UI listing the permissions. Returns the NSPoint of the
// last UI element added (either the permission button, in LTR, or the text
// label, in RTL).
- (NSPoint)addPermission:
    (const WebsiteSettingsUI::PermissionInfo&)permissionInfo
                  toView:(NSView*)view
                 atPoint:(NSPoint)point {
  base::string16 labelText =
      WebsiteSettingsUI::PermissionTypeToUIString(permissionInfo.type);
  bool isRTL = base::i18n::IsRTL();
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
              kSectionHorizontalPadding;
    imageView = [self addImageWithSize:[image size] toView:view atPoint:point];
    [imageView setImage:image];
    point.x -= kPermissionImageSpacing;

    label = [self addText:labelText
                 withSize:[NSFont systemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];
    point.x -= NSWidth([label frame]);
    [label setFrameOrigin:point];

    position =
        NSMakePoint(point.x, point.y + kPermissionPopupButtonYAdjustment);
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
                 withSize:[NSFont systemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];

    position = NSMakePoint(NSMaxX([label frame]),
                           point.y + kPermissionPopupButtonYAdjustment);

    button = [self addPopUpButtonForPermission:permissionInfo
                                        toView:view
                                       atPoint:position];
  }
  [label setToolTip:base::SysUTF16ToNSString(labelText)];

  [view setFrameSize:NSMakeSize(viewWidth, NSHeight([view frame]))];

  // Adjust the vertical position of the button so that its title text is
  // aligned with the label. Assumes that the text is the same size in both.
  // Also adjust the horizontal position to remove excess space due to the
  // invisible bezel.
  NSRect titleRect = [[button cell] titleRectForBounds:[button bounds]];
  if (isRTL) {
    position.x = kSectionHorizontalPadding;
  } else {
    position.x = kDefaultWindowWidth - kSectionHorizontalPadding -
                 [button frame].size.width;
  }
  position.y -= titleRect.origin.y;
  [button setFrameOrigin:position];

  // Truncate the label if it's too wide.
  // This is a workaround for https://crbug.com/654268 until MacViews ships.
  NSRect labelFrame = [label frame];
  if (isRTL) {
    CGFloat maxLabelWidth = NSMaxX(labelFrame) - NSMaxX([button frame]) -
                            kMinSeparationBetweenLabelAndMenu;
    if (NSWidth(labelFrame) > maxLabelWidth) {
      labelFrame.origin.x = NSMaxX(labelFrame) - maxLabelWidth;
      labelFrame.size.width = maxLabelWidth;
      [label setFrame:labelFrame];
      [[label cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    }
  } else {
    CGFloat maxLabelWidth = NSMinX([button frame]) - NSMinX(labelFrame) -
                            kMinSeparationBetweenLabelAndMenu;
    if (NSWidth(labelFrame) > maxLabelWidth) {
      labelFrame.size.width = maxLabelWidth;
      [label setFrame:labelFrame];
      [[label cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    }
  }

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

// Adds a new row to the UI listing the permissions. Returns the NSPoint of the
// last UI element added (either the permission button, in LTR, or the text
// label, in RTL).
- (NSPoint)addChosenObject:(ChosenObjectInfoPtr)objectInfo
                    toView:(NSView*)view
                   atPoint:(NSPoint)point {
  base::string16 labelText = l10n_util::GetStringFUTF16(
      objectInfo->ui_info.label_string_id,
      WebsiteSettingsUI::ChosenObjectToUIString(*objectInfo));
  bool isRTL = base::i18n::IsRTL();
  base::scoped_nsobject<NSImage> image(
      [WebsiteSettingsUI::GetChosenObjectIcon(*objectInfo, false)
              .ToNSImage() retain]);

  NSPoint position;
  NSImageView* imageView;
  NSButton* button;
  NSTextField* label;

  CGFloat viewWidth = NSWidth([view frame]);

  if (isRTL) {
    point.x = NSWidth([view frame]) - kPermissionImageSize -
              kPermissionImageSpacing - kSectionHorizontalPadding;
    imageView = [self addImageWithSize:[image size] toView:view atPoint:point];
    [imageView setImage:image];
    point.x -= kPermissionImageSpacing;

    label = [self addText:labelText
                 withSize:[NSFont systemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];
    point.x -= NSWidth([label frame]);
    [label setFrameOrigin:point];

    position = NSMakePoint(point.x, point.y);
    button = [self addDeleteButtonForChosenObject:std::move(objectInfo)
                                           toView:view
                                          atPoint:position];
    position.x -= NSWidth([button frame]);
    [button setFrameOrigin:position];
  } else {
    imageView = [self addImageWithSize:[image size] toView:view atPoint:point];
    [imageView setImage:image];
    point.x += kPermissionImageSize + kPermissionImageSpacing;

    label = [self addText:labelText
                 withSize:[NSFont systemFontSize]
                     bold:NO
                   toView:view
                  atPoint:point];
    [label sizeToFit];

    position = NSMakePoint(NSMaxX([label frame]), point.y);
    button = [self addDeleteButtonForChosenObject:std::move(objectInfo)
                                           toView:view
                                          atPoint:position];
  }

  [view setFrameSize:NSMakeSize(viewWidth, NSHeight([view frame]))];

  // Adjust the vertical position of the button so that its title text is
  // aligned with the label. Assumes that the text is the same size in both.
  // Also adjust the horizontal position to remove excess space due to the
  // invisible bezel.
  NSRect titleRect = [[button cell] titleRectForBounds:[button bounds]];
  position.y -= titleRect.origin.y;
  [button setFrameOrigin:position];

  // Align the icon with the text.
  [self alignPermissionIcon:imageView withTextField:label];

  NSRect buttonFrame = [button frame];
  return NSMakePoint(NSMaxX(buttonFrame), NSMaxY(buttonFrame));
}

// Align an image with a text field by vertically centering the image on
// the cap height of the first line of text.
- (void)alignPermissionIcon:(NSImageView*)imageView
              withTextField:(NSTextField*)textField {

  NSRect frame = [imageView frame];
  frame.origin.y += kPermissionIconYAdjustment;
  [imageView setFrame:frame];
}

- (void)setCookieInfo:(const CookieInfoList&)cookieInfoList {
  // A result of re-ordering of the permissions (crbug.com/444244) is
  // that sometimes permissions may not be displayed at all, so it's
  // incorrect to check they are set before the cookie info.

  // |cookieInfoList| should only ever have 2 items: first- and third-party
  // cookies.
  DCHECK_EQ(cookieInfoList.size(), 2u);

  int totalAllowed = 0;
  for (const auto& i : cookieInfoList) {
    totalAllowed += i.allowed;
  }
  base::string16 label_text = l10n_util::GetPluralStringFUTF16(
      IDS_WEBSITE_SETTINGS_NUM_COOKIES, totalAllowed);

  base::string16 sectionTitle =
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  bool isRTL = base::i18n::IsRTL();

  [cookiesView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kSectionHorizontalPadding, 0);

  CGFloat viewWidth = NSWidth([cookiesView_ frame]);

  // Reset X for the cookie image.
  if (isRTL) {
    controlOrigin.x = viewWidth - kPermissionImageSize -
                      kPermissionImageSpacing - kSectionHorizontalPadding;
  }

  WebsiteSettingsUI::PermissionInfo info;
  info.type = CONTENT_SETTINGS_TYPE_COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  // info.default_setting, info.source, and info.is_incognito have not been set,
  // but GetPermissionIcon doesn't use any of those.
  NSImage* image = WebsiteSettingsUI::GetPermissionIcon(info).ToNSImage();
  NSImageView* imageView = [self addImageWithSize:[image size]
                                           toView:cookiesView_
                                          atPoint:controlOrigin];
  [imageView setImage:image];

  NSButton* cookiesButton =
      [self addLinkButtonWithText:base::SysUTF16ToNSString(label_text)
                           toView:cookiesView_];
  [cookiesButton setTarget:self];
  [cookiesButton setAction:@selector(showCookiesAndSiteData:)];

  if (isRTL) {
    controlOrigin.x -= kPermissionImageSpacing;
    NSTextField* cookiesLabel =
        [self addText:l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES)
             withSize:[NSFont systemFontSize]
                 bold:NO
               toView:cookiesView_
              atPoint:controlOrigin];
    [cookiesLabel sizeToFit];

    NSPoint cookiesLabelOrigin = [cookiesLabel frame].origin;
    cookiesLabelOrigin.x -= NSWidth([cookiesLabel frame]);
    [cookiesLabel setFrameOrigin:cookiesLabelOrigin];

    // Align the icon with the text.
    [self alignPermissionIcon:imageView withTextField:cookiesLabel];

    controlOrigin.y += NSHeight([cookiesLabel frame]);
    controlOrigin.x -= NSWidth([cookiesButton frame]) - kLinkButtonXAdjustment;
    [cookiesButton setFrameOrigin:controlOrigin];
  } else {
    controlOrigin.x += kPermissionImageSize + kPermissionImageSpacing;
    NSTextField* cookiesLabel =
        [self addText:l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES)
             withSize:[NSFont systemFontSize]
                 bold:NO
               toView:cookiesView_
              atPoint:controlOrigin];
    [cookiesLabel sizeToFit];

    controlOrigin.y += NSHeight([cookiesLabel frame]);
    controlOrigin.x -= kLinkButtonXAdjustment;
    [cookiesButton setFrameOrigin:controlOrigin];

    // Align the icon with the text.
    [self alignPermissionIcon:imageView withTextField:cookiesLabel];
  }

  controlOrigin.y += NSHeight([cookiesButton frame]);
  [cookiesView_
      setFrameSize:NSMakeSize(NSWidth([cookiesView_ frame]), controlOrigin.y)];

  [self performLayout];
}

- (void)setPermissionInfo:(const PermissionInfoList&)permissionInfoList
         andChosenObjects:(ChosenObjectInfoList)chosenObjectInfoList {
  [permissionsView_ setSubviews:[NSArray array]];
  NSPoint controlOrigin = NSMakePoint(kSectionHorizontalPadding, 0);

  permissionsPresent_ = YES;

  if (permissionInfoList.size() > 0 || chosenObjectInfoList.size() > 0) {
    base::string16 sectionTitle = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);

    for (const auto& permission : permissionInfoList) {
      controlOrigin.y += kPermissionsVerticalSpacing;
      NSPoint rowBottomRight = [self addPermission:permission
                                            toView:permissionsView_
                                           atPoint:controlOrigin];
      controlOrigin.y = rowBottomRight.y;
    }

    for (auto& object : chosenObjectInfoList) {
      controlOrigin.y += kPermissionsVerticalSpacing;
      NSPoint rowBottomRight = [self addChosenObject:std::move(object)
                                              toView:permissionsView_
                                             atPoint:controlOrigin];
      controlOrigin.y = rowBottomRight.y;
    }

    controlOrigin.y += kPermissionsVerticalSpacing;
  }

  [permissionsView_ setFrameSize:
      NSMakeSize(NSWidth([permissionsView_ frame]), controlOrigin.y)];
  [self performLayout];
}

@end

WebsiteSettingsUIBridge::WebsiteSettingsUIBridge(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      bubble_controller_(nil) {
  DCHECK(!g_is_popup_showing);
  g_is_popup_showing = true;
}

WebsiteSettingsUIBridge::~WebsiteSettingsUIBridge() {
  DCHECK(g_is_popup_showing);
  g_is_popup_showing = false;
}

void WebsiteSettingsUIBridge::set_bubble_controller(
    WebsiteSettingsBubbleController* controller) {
  bubble_controller_ = controller;
}

void WebsiteSettingsUIBridge::Show(
    gfx::NativeWindow parent,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& virtual_url,
    const security_state::SecurityInfo& security_info) {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    chrome::ShowWebsiteSettingsBubbleViewsAtPoint(
        gfx::ScreenPointFromNSPoint(AnchorPointForWindow(parent)), profile,
        web_contents, virtual_url, security_info);
    return;
  }

  // Don't show the popup if it's already being shown. Since this method is
  // called each time the location icon is clicked, each click toggles the popup
  // in and out.
  if (g_is_popup_showing)
    return;

  // Create the bridge. This will be owned by the bubble controller.
  WebsiteSettingsUIBridge* bridge = new WebsiteSettingsUIBridge(web_contents);

  // Create the bubble controller. It will dealloc itself when it closes,
  // resetting |g_is_popup_showing|.
  WebsiteSettingsBubbleController* bubble_controller =
      [[WebsiteSettingsBubbleController alloc]
             initWithParentWindow:parent
          websiteSettingsUIBridge:bridge
                      webContents:web_contents
                              url:virtual_url];

  if (!IsInternalURL(virtual_url)) {
    // Initialize the presenter, which holds the model and controls the UI.
    // This is also owned by the bubble controller.
    WebsiteSettings* presenter = new WebsiteSettings(
        bridge, profile,
        TabSpecificContentSettings::FromWebContents(web_contents), web_contents,
        virtual_url, security_info);
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
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  [bubble_controller_ setPermissionInfo:permission_info_list
                       andChosenObjects:std::move(chosen_object_info_list)];
}
