// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_intent_bubble_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// The width of the window, in view coordinates. The height will be
// determined by the content.
const CGFloat kWindowWidth = 380;

// Padding along on the X-axis between the window frame and content.
const CGFloat kFramePadding = 10;

// Spacing in between sections.
const CGFloat kVerticalSpacing = 10;

// Square size of the image.
const CGFloat kImageSize = 32;

// Spacing between the image and the text.
const CGFloat kImageSpacing = 10;

// Spacing between icons.
const CGFloat kImagePadding = 6;

// Width of the text fields.
const CGFloat kTextWidth = kWindowWidth - (kImageSize + kImageSpacing +
    kFramePadding * 2);

}  // namespace

@implementation WebIntentBubbleController;

- (id)initWithPicker:(WebIntentPickerCocoa*)picker
        parentWindow:(NSWindow*)parent
          anchoredAt:(NSPoint)point {
  // Use an arbitrary height because it will reflect the size of the content.
  NSRect contentRect = NSMakeRect(0, 0, kWindowWidth, kVerticalSpacing);
  // Create an empty window into which content is placed.
  scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);
  if ((self = [super initWithWindow:window.get()
                       parentWindow:parent
                         anchoredAt:point])) {
    picker_ = picker;
    iconImages_.reset([[NSPointerArray alloc] initWithOptions:
        NSPointerFunctionsStrongMemory |
        NSPointerFunctionsObjectPersonality]);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    defaultIcon_.reset([rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON) retain]);

    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    [self performLayout];
    [self showWindow:nil];
    picker_->set_controller(self);
  }

  return self;
}

- (void)setServiceURLs:(NSArray*)urls {
  serviceURLs_.reset([urls retain]);

  if ([iconImages_ count] < [serviceURLs_ count])
    [iconImages_ setCount:[serviceURLs_ count]];

  [self performLayout];
}

- (void)setInlineDispositionTabContents:(TabContentsWrapper*)wrapper {
  contents_ = wrapper;
  [self performLayout];
}

- (void)replaceImageAtIndex:(size_t)index withImage:(NSImage*)image {
  if ([iconImages_ count] <= index)
    [iconImages_ setCount:index + 1];

  [iconImages_ replacePointerAtIndex:index withPointer:image];
  [self performLayout];
}

// We need to watch for window closing so we can notify up via |picker_|.
- (void)windowWillClose:(NSNotification*)notification {
  if (picker_) {
    WebIntentPickerCocoa* temp = picker_;
    picker_ = NULL;  // Abandon picker, we are done with it.
    temp->OnCancelled();
  }
  [super windowWillClose:notification];
}

// Pop up a new tab with the Chrome Web Store.
- (IBAction)showChromeWebStore:(id)sender {
  GURL url(l10n_util::GetStringUTF8(IDS_WEBSTORE_URL));
  Browser* browser = BrowserList::GetLastActive();
  OpenURLParams params(
      url, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
      false);
  browser->OpenURL(params);
}

// A picker button has been pressed - invoke corresponding service.
- (IBAction)invokeService:(id)sender {
  if (picker_) {
    WebIntentPickerCocoa* temp = picker_;
    picker_ = NULL;  // Abandon picker, we are done with it.
    temp->OnServiceChosen([[sender selectedCell] tag]);
  }
}

// Sets proprties on the given |field| to act as the title or description labels
// in the bubble.
- (void)configureTextFieldAsLabel:(NSTextField*)field {
  [field setEditable:NO];
  [field setSelectable:YES];
  [field setDrawsBackground:NO];
  [field setBezeled:NO];
}

// Adds a link to the Chrome Web Store, to obtain further intent handlers.
// Returns the y position delta for the next offset.
- (CGFloat)addCwsButtonToSubviews:(NSMutableArray*)subviews
                         atOffset:(CGFloat)offset {
  NSRect frame = NSMakeRect(kFramePadding, offset, 100, 10);
  scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:frame]);
  NSString* string =
      l10n_util::GetNSStringWithFixup(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE);
  scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:string]);
  [cell setControlSize:NSSmallControlSize];
  [button setCell:cell.get()];
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTarget:self];
  [button setAction:@selector(showChromeWebStore:)];
  [subviews addObject:button.get()];

  // Call size-to-fit to fixup for the localized string.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];

  return NSHeight([button frame]);
}

// Adds a header (icon and explanatory text) to picker bubble.
// Returns the y position delta for the next offset.
- (CGFloat)addHeaderToSubviews:(NSMutableArray*)subviews
                      atOffset:(CGFloat)offset {
  NSRect imageFrame = NSMakeRect(kFramePadding, offset, kImageSize, kImageSize);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* nsImage = rb.GetNativeImageNamed(IDR_PAGEINFO_INFO);

  scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:imageFrame]);
  [imageView setImage:nsImage];
  [imageView setImageFrameStyle:NSImageFrameNone];

  NSRect textFrame = NSMakeRect(kFramePadding + kImageSize + kImageSpacing,
                                offset, kTextWidth, 1);
  scoped_nsobject<NSTextField> textField(
        [[NSTextField alloc] initWithFrame:textFrame]);
  [self configureTextFieldAsLabel:textField.get()];

  NSString* nsString =
      l10n_util::GetNSStringWithFixup(IDS_CHOOSE_INTENT_HANDLER_MESSAGE);
  [textField setStringValue:nsString];
  textFrame.size.height +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
            textField];

  // Adjust view height to fit elements, center-align elements.
  CGFloat maxHeight = std::max(imageFrame.size.height,textFrame.size.height);
  if (maxHeight > textFrame.size.height)
    textFrame.origin.y += (maxHeight - textFrame.size.height) / 2;
  else
    imageFrame.origin.y += maxHeight / 2;
  [textField setFrame:textFrame];
  [imageView setFrame:imageFrame];

  [subviews addObject:textField.get()];
  [subviews addObject:imageView.get()];
  return NSHeight([imageView frame]);
}

// Add all service icons to picker UI.
// Returns the y position delta for the next offset.
- (CGFloat)addIcons:(NSPointerArray*)icons
          toSubviews:(NSMutableArray*)subviews
            atOffset:(CGFloat)offset {
  CGFloat matrixOffset = kFramePadding + kImageSize + kImagePadding;
  CGFloat matrixWidth = kWindowWidth - matrixOffset - kFramePadding;

  CGFloat iconWidth = kImageSize + kImagePadding;
  NSInteger iconsPerRow = matrixWidth / iconWidth;
  NSInteger numRows = ([icons count] / iconsPerRow) + 1;

  NSRect frame = NSMakeRect(matrixOffset, offset, matrixWidth,
                            (kImageSize + kImagePadding) * numRows);

  scoped_nsobject<NSMatrix> matrix(
      [[NSMatrix alloc] initWithFrame:frame
                                 mode:NSHighlightModeMatrix
                            cellClass:[NSImageCell class]
                         numberOfRows:numRows
                      numberOfColumns:iconsPerRow]);

  [matrix setCellSize:NSMakeSize(kImageSize,kImageSize)];
  [matrix setIntercellSpacing:NSMakeSize(kImagePadding,kImagePadding)];

  for (NSUInteger i = 0; i < [iconImages_ count]; ++i) {
    scoped_nsobject<NSButtonCell> cell([[NSButtonCell alloc] init]);
    NSImage* image = static_cast<NSImage*>([iconImages_ pointerAtIndex:i]);
    if (!image)
      image = defaultIcon_;

    // Set cell styles so it acts as image button.
    [cell setBordered:NO];
    [cell setImage:image];
    [cell setImagePosition:NSImageOnly];
    [cell setButtonType:NSMomentaryChangeButton];
    [cell setTarget:self];
    [cell setAction:@selector(invokeService:)];
    [cell setTag:i];
    [cell setEnabled:YES];

    [matrix putCell:cell atRow:(i / iconsPerRow) column:(i % iconsPerRow)];
    if (serviceURLs_ && [serviceURLs_ count] >= i)
      [matrix setToolTip:[serviceURLs_ objectAtIndex:i] forCell:cell];
  }

  [subviews addObject:matrix];
  return NSHeight([matrix frame]);
}

- (CGFloat)addInlineHtmlToSubviews:(NSMutableArray*)subviews
                          atOffset:(CGFloat)offset {
  if (!contents_)
    return 0;

  // Determine a good size for the inline disposition window.
  gfx::Size tab_size = contents_->web_contents()->GetView()->GetContainerSize();
  CGFloat width = std::max(CGFloat(tab_size.width()/2.0), kWindowWidth);
  CGFloat height = std::max(CGFloat(tab_size.height()/2.0), kWindowWidth);
  NSRect frame = NSMakeRect(kFramePadding, offset, width, height);

  [contents_->web_contents()->GetNativeView() setFrame:frame];
  [subviews addObject:contents_->web_contents()->GetNativeView()];

  return NSHeight(frame);
}

// Layout the contents of the picker bubble.
- (void)performLayout {
  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = kFramePadding + info_bubble::kBubbleArrowHeight;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  if (contents_) {
    offset += [self addInlineHtmlToSubviews:subviews atOffset:offset];
  } else {
    offset += [self addCwsButtonToSubviews:subviews atOffset:offset];
    offset += [self addIcons:iconImages_ toSubviews:subviews atOffset:offset];
    offset += [self addHeaderToSubviews:subviews atOffset:offset];
  }

  // Add the bottom padding.
  offset += kVerticalSpacing;

  // Adjust frame to fit all elements.
  NSRect windowFrame = NSMakeRect(0, 0, kWindowWidth, offset);
  windowFrame.size = [[[self window] contentView] convertSize:windowFrame.size
                                                       toView:nil];
  // Adjust the origin by the difference in height.
  windowFrame.origin = [[self window] frame].origin;
  windowFrame.origin.y -= NSHeight(windowFrame) -
      NSHeight([[self window] frame]);

  [[self window] setFrame:windowFrame display:YES animate:YES];

  // Replace the window's content.
  [[[self window] contentView] setSubviews:subviews];
}

@end  // WebIntentBubbleController
