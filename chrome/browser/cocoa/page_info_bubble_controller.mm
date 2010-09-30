// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/page_info_bubble_controller.h"

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/certificate_viewer.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#import "chrome/browser/cocoa/info_bubble_window.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface PageInfoBubbleController (Private)
- (PageInfoModel*)model;
- (NSButton*)certificateButtonWithFrame:(NSRect)frame;
- (NSImage*)statusIconForState:(PageInfoModel::SectionInfoState)state;
- (void)configureTextFieldAsLabel:(NSTextField*)textField;
- (CGFloat)addTitleViewForInfo:(const PageInfoModel::SectionInfo&)info
                    toSubviews:(NSMutableArray*)subviews
                       atPoint:(NSPoint)point;
- (CGFloat)addDescriptionViewForInfo:(const PageInfoModel::SectionInfo&)info
                          toSubviews:(NSMutableArray*)subviews
                             atPoint:(NSPoint)point;
- (CGFloat)addCertificateButtonToSubviews:(NSMutableArray*)subviews
                                 atOffset:(CGFloat)offset;
- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset;
@end

namespace {

// The width of the window, in view coordinates. The height will be determined
// by the content.
const NSInteger kWindowWidth = 380;

// Spacing in between sections.
const NSInteger kVerticalSpacing = 10;

// Padding along on the X-axis between the window frame and content.
const NSInteger kFramePadding = 20;

// Spacing between the title and description text views.
const NSInteger kTitleSpacing = 2;

// Spacing between the image and the text.
const NSInteger kImageSpacing = 10;

// Square size of the image.
const CGFloat kImageSize = 30;

// The X position of the text fields. Variants for with and without an image.
const CGFloat kTextXPositionNoImage = kFramePadding;
const CGFloat kTextXPosition = kTextXPositionNoImage + kImageSize +
    kImageSpacing;

// Width of the text fields.
const CGFloat kTextWidth = kWindowWidth - (kImageSize + kImageSpacing +
    kFramePadding * 2);

// Takes in the bubble's height and the parent window, which should be a
// BrowserWindow, and gets the proper anchor point for the bubble. The point is
// in screen coordinates.
NSPoint AnchorPointFromParentWindow(NSWindow* parent, CGFloat bubbleHeight) {
  BrowserWindowController* controller = [parent windowController];
  NSPoint origin = NSZeroPoint;
  if ([controller isKindOfClass:[BrowserWindowController class]]) {
    LocationBarViewMac* location_bar = [controller locationBarBridge];
    if (location_bar) {
      NSPoint arrowTip = location_bar->GetPageInfoBubblePoint();
      origin = [parent convertBaseToScreen:arrowTip];
      origin.y -= bubbleHeight;
    }
  }
  return origin;
}

// Bridge that listens for change notifications from the model.
class PageInfoModelBubbleBridge : public PageInfoModel::PageInfoModelObserver {
 public:
  PageInfoModelBubbleBridge() : controller_(nil) {}

  // PageInfoModelObserver implementation.
  virtual void ModelChanged() {
    [controller_ performLayout];
  }

  // Sets the controller.
  void set_controller(PageInfoBubbleController* controller) {
    controller_ = controller;
  }

 private:
  PageInfoBubbleController* controller_;  // weak
};

}  // namespace

namespace browser {

void ShowPageInfoBubble(gfx::NativeWindow parent,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history) {
  PageInfoModelBubbleBridge* bridge = new PageInfoModelBubbleBridge();
  PageInfoModel* model =
      new PageInfoModel(profile, url, ssl, show_history, bridge);
  PageInfoBubbleController* controller =
      [[PageInfoBubbleController alloc] initWithPageInfoModel:model
                                                modelObserver:bridge
                                                 parentWindow:parent];
  bridge->set_controller(controller);
  [controller setCertID:ssl.cert_id()];
  [controller showWindow:nil];
}

}  // namespace browser

@implementation PageInfoBubbleController

@synthesize certID = certID_;

- (id)initWithPageInfoModel:(PageInfoModel*)model
              modelObserver:(PageInfoModel::PageInfoModelObserver*)bridge
               parentWindow:(NSWindow*)parentWindow {
  // Use an arbitrary height because it will be changed by the bridge.
  NSRect contentRect = NSMakeRect(0, 0, kWindowWidth, 50);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);

  NSPoint anchorPoint = AnchorPointFromParentWindow(parentWindow, 0);
  if ((self = [super initWithWindow:window.get()
                       parentWindow:parentWindow
                         anchoredAt:anchorPoint])) {
    model_.reset(model);
    bridge_.reset(bridge);

    // Load the image refs.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    okImage_.reset([rb.GetNSImageNamed(IDR_PAGEINFO_GOOD) retain]);
    DCHECK_GE(kImageSize, [okImage_ size].width);
    DCHECK_GE(kImageSize, [okImage_ size].height);
    warningMinorImage_.reset(
        [rb.GetNSImageNamed(IDR_PAGEINFO_WARNING_MINOR) retain]);
    DCHECK_GE(kImageSize, [warningMinorImage_ size].width);
    DCHECK_GE(kImageSize, [warningMinorImage_ size].height);
    warningMajorImage_.reset(
        [rb.GetNSImageNamed(IDR_PAGEINFO_WARNING_MAJOR) retain]);
    DCHECK_GE(kImageSize, [warningMajorImage_ size].width);
    DCHECK_GE(kImageSize, [warningMajorImage_ size].height);
    errorImage_.reset([rb.GetNSImageNamed(IDR_PAGEINFO_BAD) retain]);
    DCHECK_GE(kImageSize, [errorImage_ size].width);
    DCHECK_GE(kImageSize, [errorImage_ size].height);

    [[self bubble] setArrowLocation:info_bubble::kTopLeft];

    [self performLayout];
  }
  return self;
}

- (PageInfoModel*)model {
  return model_.get();
}

- (IBAction)showCertWindow:(id)sender {
  DCHECK(certID_ != 0);
  ShowCertificateViewerByID([self parentWindow], certID_);
}

// This will create the subviews for the page info window. The general layout
// is 2 or 3 boxed and titled sections, each of which has a status image to
// provide visual feedback and a description that explains it. The description
// text is usually only 1 or 2 lines, but can be much longer. At the bottom of
// the window is a button to view the SSL certificate, which is disabled if
// not using HTTPS.
- (void)performLayout {
  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = kVerticalSpacing;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  // Build the window from bottom-up because Cocoa's coordinate origin is the
  // lower left.
  for (int i = model_->GetSectionCount() - 1; i >= 0; --i) {
    PageInfoModel::SectionInfo info = model_->GetSectionInfo(i);

    // Only certain sections have images. This affects the X position.
    BOOL hasImage = info.type == PageInfoModel::SECTION_INFO_IDENTITY ||
        info.type == PageInfoModel::SECTION_INFO_CONNECTION;
    CGFloat xPosition = (hasImage ? kTextXPosition : kTextXPositionNoImage);

    if (info.type == PageInfoModel::SECTION_INFO_IDENTITY) {
      offset += [self addCertificateButtonToSubviews:subviews
          atOffset:offset];
    }

    // Create the description of the state.
    offset += [self addDescriptionViewForInfo:info
                                   toSubviews:subviews
                                      atPoint:NSMakePoint(xPosition, offset)];

    // Add the title.
    offset += kTitleSpacing;
    offset += [self addTitleViewForInfo:info
                             toSubviews:subviews
                                atPoint:NSMakePoint(xPosition, offset)];

    // Insert the image subview for sections that are appropriate.
    if (hasImage) {
      [self addImageViewForInfo:info toSubviews:subviews atOffset:offset];
    }

    // Add the separators.
    offset += kVerticalSpacing;
    if (i != 0) {
      scoped_nsobject<NSBox> spacer(
          [[NSBox alloc] initWithFrame:NSMakeRect(0, offset, kWindowWidth, 1)]);
      [spacer setBoxType:NSBoxSeparator];
      [spacer setBorderType:NSLineBorder];
      [subviews addObject:spacer.get()];
      offset += kVerticalSpacing;
    }
  }

  // Replace the window's content.
  [[[self window] contentView] setSubviews:subviews];

  offset += kFramePadding;

  // TODO(rsesek): Remove constant value to account for http://crbug.com/57306.
  NSRect windowFrame = NSMakeRect(0, 0, kWindowWidth, 50);
  windowFrame.size.height += offset;
  windowFrame.size = [[[self window] contentView] convertSize:windowFrame.size
                                                       toView:nil];
  // Just setting |size| will cause the window to grow upwards. Shift the
  // origin up by grow amount, which causes the window to grow downwards.
  windowFrame.origin = AnchorPointFromParentWindow([self parentWindow],
                                                   NSHeight(windowFrame));

  // Resize the window. Only animate if the window is visible, otherwise it
  // could be "growing" while it's opening, looking awkward.
  [[self window] setFrame:windowFrame
                  display:YES
                  animate:[[self window] isVisible]];
}

// Creates the button with a given |frame| that, when clicked, will show the
// SSL certificate information.
- (NSButton*)certificateButtonWithFrame:(NSRect)frame {
  NSButton* certButton = [[[NSButton alloc] initWithFrame:frame] autorelease];
  [certButton setTitle:
      l10n_util::GetNSStringWithFixup(IDS_PAGEINFO_CERT_INFO_BUTTON)];
  [certButton setButtonType:NSMomentaryPushInButton];
  [certButton setBezelStyle:NSRoundRectBezelStyle];
  [certButton setTarget:self];
  [certButton setAction:@selector(showCertWindow:)];
  return certButton;
}

// Returns a weak reference to the NSImage instance to used, or nil if none, for
// the specified info |state|.
- (NSImage*)statusIconForState:(PageInfoModel::SectionInfoState)state {
  switch (state) {
    case PageInfoModel::SECTION_STATE_OK:
      return okImage_.get();
    case PageInfoModel::SECTION_STATE_WARNING_MINOR:
      return warningMinorImage_.get();
    case PageInfoModel::SECTION_STATE_WARNING_MAJOR:
      return warningMajorImage_.get();
    case PageInfoModel::SECTION_STATE_ERROR:
      return errorImage_.get();
    default:
      return nil;
  }
}

// Sets proprties on the given |field| to act as the title or description labels
// in the bubble.
- (void)configureTextFieldAsLabel:(NSTextField*)textField {
  [textField setEditable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
}

// Adds the title text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addTitleViewForInfo:(const PageInfoModel::SectionInfo&)info
                    toSubviews:(NSMutableArray*)subviews
                       atPoint:(NSPoint)point {
  NSRect frame = NSMakeRect(point.x, point.y, kTextWidth, kImageSpacing);
  scoped_nsobject<NSTextField> titleField(
      [[NSTextField alloc] initWithFrame:frame]);
  [self configureTextFieldAsLabel:titleField.get()];
  [titleField setStringValue:base::SysUTF16ToNSString(info.title)];
  [titleField setFont:[NSFont boldSystemFontOfSize:11]];
  frame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
          titleField];
  [titleField setFrame:frame];
  [subviews addObject:titleField.get()];
  return NSHeight(frame);
}

// Adds the description text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addDescriptionViewForInfo:(const PageInfoModel::SectionInfo&)info
                          toSubviews:(NSMutableArray*)subviews
                             atPoint:(NSPoint)point {
  NSRect frame = NSMakeRect(point.x, point.y, kTextWidth, kImageSize);
  scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:frame]);
  [self configureTextFieldAsLabel:textField.get()];
  [textField setStringValue:base::SysUTF16ToNSString(info.description)];
  [textField setFont:[NSFont labelFontOfSize:11]];

  // If the text is oversized, resize the text field.
  frame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
          textField];
  [subviews addObject:textField.get()];
  return NSHeight(frame);
}

// Adds the certificate button at a pre-determined x position and the given y.
// Returns the y position for the next element.
- (CGFloat)addCertificateButtonToSubviews:(NSMutableArray*)subviews
                                 atOffset:(CGFloat)offset {
  // Create the certificate button. The frame will be fixed up by GTM, so
  // use arbitrary values.
  NSRect frame = NSMakeRect(kTextXPosition, offset, 100, 20);
  NSButton* certButton = [self certificateButtonWithFrame:frame];
  [subviews addObject:certButton];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:certButton];

  // By default, assume that we don't have certificate information to show.
  [certButton setEnabled:NO];
  if (certID_) {
    scoped_refptr<net::X509Certificate> cert;
    CertStore::GetSharedInstance()->RetrieveCert(certID_, &cert);

    // Don't bother showing certificates if there isn't one. Gears runs
    // with no OS root certificate.
    if (cert.get() && cert->os_cert_handle()) {
      [certButton setEnabled:YES];
    }
  }

  return NSHeight(frame) + kVerticalSpacing;
}

// Adds the state image at a pre-determined x position and the given y. This
// does not affect the next Y position because the image is placed next to
// a text field that is larger and accounts for the image's size.
- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset {
  NSRect frame = NSMakeRect(kFramePadding, offset - kImageSize, kImageSize,
      kImageSize);
  scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:frame]);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [imageView setImage:[self statusIconForState:info.state]];
  [subviews addObject:imageView.get()];
}

@end
