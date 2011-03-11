// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/page_info_bubble_controller.h"

#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/common/url_constants.h"
#include "content/browser/certificate_viewer.h"
#include "content/browser/cert_store.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image.h"

@interface PageInfoBubbleController (Private)
- (PageInfoModel*)model;
- (NSButton*)certificateButtonWithFrame:(NSRect)frame;
- (void)configureTextFieldAsLabel:(NSTextField*)textField;
- (CGFloat)addHeadlineViewForInfo:(const PageInfoModel::SectionInfo&)info
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
- (CGFloat)addHelpButtonToSubviews:(NSMutableArray*)subviews
                          atOffset:(CGFloat)offset;
- (CGFloat)addSeparatorToSubviews:(NSMutableArray*)subviews
                         atOffset:(CGFloat)offset;
- (NSPoint)anchorPointForWindowWithHeight:(CGFloat)bubbleHeight
                             parentWindow:(NSWindow*)parent;
@end

// This simple NSView subclass is used as the single subview of the page info
// bubble's window's contentView. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
@interface PageInfoContentView : NSView
@end
@implementation PageInfoContentView
- (BOOL)isFlipped {
  return YES;
}
@end

namespace {

// The width of the window, in view coordinates. The height will be determined
// by the content.
const CGFloat kWindowWidth = 380;

// Spacing in between sections.
const CGFloat kVerticalSpacing = 10;

// Padding along on the X-axis between the window frame and content.
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

// Bridge that listens for change notifications from the model.
class PageInfoModelBubbleBridge : public PageInfoModel::PageInfoModelObserver {
 public:
  PageInfoModelBubbleBridge()
      : controller_(nil),
        ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  }

  // PageInfoModelObserver implementation.
  virtual void ModelChanged() {
    // Check to see if a layout has already been scheduled.
    if (!task_factory_.empty())
      return;

    // Delay performing layout by a second so that all the animations from
    // InfoBubbleWindow and origin updates from BaseBubbleController finish, so
    // that we don't all race trying to change the frame's origin.
    //
    // Using ScopedRunnableMethodFactory is superior here to |-performSelector:|
    // because it will not retain its target; if the child outlives its parent,
    // zombies get left behind (http://crbug.com/59619). This will also cancel
    // the scheduled Tasks if the controller (and thus this bridge) get
    // destroyed before the message can be delivered.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        task_factory_.NewRunnableMethod(
            &PageInfoModelBubbleBridge::PerformLayout),
        1000 /* milliseconds */);
  }

  // Sets the controller.
  void set_controller(PageInfoBubbleController* controller) {
    controller_ = controller;
  }

 private:
  void PerformLayout() {
    [controller_ performLayout];
  }

  PageInfoBubbleController* controller_;  // weak

  // Factory that vends RunnableMethod tasks for scheduling layout.
  ScopedRunnableMethodFactory<PageInfoModelBubbleBridge> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoModelBubbleBridge);
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
  DCHECK(parentWindow);

  // Use an arbitrary height because it will be changed by the bridge.
  NSRect contentRect = NSMakeRect(0, 0, kWindowWidth, 0);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);

  if ((self = [super initWithWindow:window.get()
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    model_.reset(model);
    bridge_.reset(bridge);
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

- (IBAction)showHelpPage:(id)sender {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kPageInfoHelpCenterURL));
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
}

// This will create the subviews for the page info window. The general layout
// is 2 or 3 boxed and titled sections, each of which has a status image to
// provide visual feedback and a description that explains it. The description
// text is usually only 1 or 2 lines, but can be much longer. At the bottom of
// the window is a button to view the SSL certificate, which is disabled if
// not using HTTPS.
- (void)performLayout {
  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = kFramePadding + info_bubble::kBubbleArrowHeight;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  // The subviews will be attached to the PageInfoContentView, which has a
  // flipped origin. This allows the code to build top-to-bottom.
  const int sectionCount = model_->GetSectionCount();
  for (int i = 0; i < sectionCount; ++i) {
    PageInfoModel::SectionInfo info = model_->GetSectionInfo(i);

    // Only certain sections have images. This affects the X position.
    BOOL hasImage = model_->GetIconImage(info.icon_id) != nil;
    CGFloat xPosition = (hasImage ? kTextXPosition : kTextXPositionNoImage);

    // Insert the image subview for sections that are appropriate.
    CGFloat imageBaseline = offset + kImageSize;
    if (hasImage) {
      [self addImageViewForInfo:info toSubviews:subviews atOffset:offset];
    }

    // Add the title.
    if (!info.headline.empty()) {
      offset += [self addHeadlineViewForInfo:info
                                  toSubviews:subviews
                                     atPoint:NSMakePoint(xPosition, offset)];
      offset += kHeadlineSpacing;
    }

    // Create the description of the state.
    offset += [self addDescriptionViewForInfo:info
                                   toSubviews:subviews
                                      atPoint:NSMakePoint(xPosition, offset)];

    if (info.type == PageInfoModel::SECTION_INFO_IDENTITY && certID_) {
      offset += kVerticalSpacing;
      offset += [self addCertificateButtonToSubviews:subviews atOffset:offset];
    }

    // If at this point the description and optional headline and button are
    // not as tall as the image, adjust the offset by the difference.
    CGFloat imageBaselineDelta = imageBaseline - offset;
    if (imageBaselineDelta > 0)
      offset += imageBaselineDelta;

    // Add the separators.
    offset += kVerticalSpacing;
    offset += [self addSeparatorToSubviews:subviews atOffset:offset];
  }

  // The last item at the bottom of the window is the help center link.
  offset += [self addHelpButtonToSubviews:subviews atOffset:offset];
  offset += kVerticalSpacing;

  // Create the dummy view that uses flipped coordinates.
  NSRect contentFrame = NSMakeRect(0, 0, kWindowWidth, offset);
  scoped_nsobject<PageInfoContentView> contentView(
      [[PageInfoContentView alloc] initWithFrame:contentFrame]);
  [contentView setSubviews:subviews];

  // Replace the window's content.
  [[[self window] contentView] setSubviews:
      [NSArray arrayWithObject:contentView]];

  NSRect windowFrame = NSMakeRect(0, 0, kWindowWidth, offset);
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

  NSPoint anchorPoint =
      [self anchorPointForWindowWithHeight:NSHeight(windowFrame)
                              parentWindow:[self parentWindow]];
  [self setAnchorPoint:anchorPoint];
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
  [[certButton cell] setControlSize:NSSmallControlSize];
  NSFont* font = [NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]];
  [[certButton cell] setFont:font];
  return certButton;
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
- (CGFloat)addHeadlineViewForInfo:(const PageInfoModel::SectionInfo&)info
                       toSubviews:(NSMutableArray*)subviews
                          atPoint:(NSPoint)point {
  NSRect frame = NSMakeRect(point.x, point.y, kTextWidth, kImageSpacing);
  scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:frame]);
  [self configureTextFieldAsLabel:textField.get()];
  [textField setStringValue:base::SysUTF16ToNSString(info.headline)];
  NSFont* font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
  [textField setFont:font];
  frame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
          textField];
  [textField setFrame:frame];
  [subviews addObject:textField.get()];
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
  [textField setFont:[NSFont labelFontOfSize:[NSFont smallSystemFontSize]]];

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
  // The certificate button should only be added if there is SSL information.
  DCHECK(certID_);

  // Create the certificate button. The frame will be fixed up by GTM, so
  // use arbitrary values.
  NSRect frame = NSMakeRect(kTextXPosition, offset, 100, 14);
  NSButton* certButton = [self certificateButtonWithFrame:frame];
  [subviews addObject:certButton];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:certButton];

  // By default, assume that we don't have certificate information to show.
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetInstance()->RetrieveCert(certID_, &cert);

  // Don't bother showing certificates if there isn't one.
  if (!cert.get() || !cert->os_cert_handle()) {
    // This should only ever happen in unit tests.
    [certButton setEnabled:NO];
  }

  return NSHeight([certButton frame]);
}

// Adds the state image at a pre-determined x position and the given y. This
// does not affect the next Y position because the image is placed next to
// a text field that is larger and accounts for the image's size.
- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset {
  NSRect frame =
      NSMakeRect(kFramePadding, offset, kImageSize, kImageSize);
  scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:frame]);
  [imageView setImageFrameStyle:NSImageFrameNone];
  [imageView setImage:*model_->GetIconImage(info.icon_id)];
  [subviews addObject:imageView.get()];
}

// Adds the help center button that explains the icons. Returns the y position
// delta for the next offset.
- (CGFloat)addHelpButtonToSubviews:(NSMutableArray*)subviews
                          atOffset:(CGFloat)offset {
  NSRect frame = NSMakeRect(kFramePadding, offset, 100, 10);
  scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:frame]);
  NSString* string =
      l10n_util::GetNSStringWithFixup(IDS_PAGE_INFO_HELP_CENTER_LINK);
  scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:string]);
  [cell setControlSize:NSSmallControlSize];
  [button setCell:cell.get()];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTarget:self];
  [button setAction:@selector(showHelpPage:)];
  [subviews addObject:button.get()];

  // Call size-to-fit to fixup for the localized string.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];
  return NSHeight([button frame]);
}

// Adds a 1px separator between sections. Returns the y position delta for the
// next offset.
- (CGFloat)addSeparatorToSubviews:(NSMutableArray*)subviews
                         atOffset:(CGFloat)offset {
  const CGFloat kSpacerHeight = 1.0;
  NSRect frame = NSMakeRect(kFramePadding, offset,
      kWindowWidth - 2 * kFramePadding, kSpacerHeight);
  scoped_nsobject<NSBox> spacer([[NSBox alloc] initWithFrame:frame]);
  [spacer setBoxType:NSBoxSeparator];
  [spacer setBorderType:NSLineBorder];
  [spacer setAlphaValue:0.2];
  [subviews addObject:spacer.get()];
  return kVerticalSpacing + kSpacerHeight;
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

@end
