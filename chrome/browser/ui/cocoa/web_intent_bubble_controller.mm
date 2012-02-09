// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_intent_bubble_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
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

// The width of a service button, in view coordinates.
const CGFloat kServiceButtonWidth = 300;

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

// This simple NSView subclass is used as the single subview of the page info
// bubble's window's contentView. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
@interface WebIntentsContentView : NSView
@end
@implementation WebIntentsContentView
- (BOOL)isFlipped {
  return YES;
}
@end

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

    [[self bubble] setArrowLocation:info_bubble::kTopLeft];
    [self performLayoutWithModel:NULL];
    [self showWindow:nil];
    picker_->set_controller(self);
  }

  return self;
}

- (void)setInlineDispositionTabContents:(TabContentsWrapper*)wrapper {
  contents_ = wrapper;
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
  if (picker_)
    picker_->OnServiceChosen([sender tag]);
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

- (CGFloat)addInlineHtmlToSubviews:(NSMutableArray*)subviews
                          atOffset:(CGFloat)offset {
  if (!contents_)
    return 0;

  // Determine a good size for the inline disposition window.
  gfx::Size size = WebIntentPicker::GetDefaultInlineDispositionSize(
      contents_->web_contents());
  NSRect frame = NSMakeRect(kFramePadding, offset, size.width(), size.height());

  [contents_->web_contents()->GetNativeView() setFrame:frame];
  [subviews addObject:contents_->web_contents()->GetNativeView()];

  return NSHeight(frame);
}

// Add a single button for a specific service
-(CGFloat)addServiceButton:(NSString*)title
                 withImage:(NSImage*)image
                     index:(NSUInteger)index
                toSubviews:(NSMutableArray*)subviews
                  atOffset:(CGFloat)offset {
  NSRect frame = NSMakeRect(kFramePadding, offset, kServiceButtonWidth, 45);
  scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:frame]);

  if (image) {
    [button setImage:image];
    [button setImagePosition:NSImageLeft];
  }
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setTarget:self];
  [button setTitle:title];
  [button setTag:index];
  [button setAction:@selector(invokeService:)];
  [subviews addObject:button.get()];

  // Call size-to-fit to fixup size.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button.get()];

  // But make sure we're limited to a fixed size.
  frame = [button frame];
  frame.size.width = kServiceButtonWidth;
  [button setFrame:frame];

  return NSHeight([button frame]);
}

// Layout the contents of the picker bubble.
- (void)performLayoutWithModel:(WebIntentPickerModel*)model {
  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = kFramePadding + info_bubble::kBubbleArrowHeight;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  if (contents_) {
    offset += [self addInlineHtmlToSubviews:subviews atOffset:offset];
  } else {
    offset += [self addHeaderToSubviews:subviews atOffset:offset];
    if (model) {
      for (NSUInteger i = 0; i < model->GetItemCount(); ++i) {
        const WebIntentPickerModel::Item& item = model->GetItemAt(i);
        offset += [self addServiceButton:base::SysUTF16ToNSString(item.title)
                               withImage:item.favicon.ToNSImage()
                                   index:i
                              toSubviews:subviews
                                atOffset:offset];
      }
    }
    offset += [self addCwsButtonToSubviews:subviews atOffset:offset];
  }

  // Add the bottom padding.
  offset += kVerticalSpacing;

  // Create the dummy view that uses flipped coordinates.
  NSRect contentFrame = NSMakeRect(0, 0, kWindowWidth, offset);
  scoped_nsobject<WebIntentsContentView> contentView(
      [[WebIntentsContentView alloc] initWithFrame:contentFrame]);
  [contentView setSubviews:subviews];
  [contentView setAutoresizingMask:NSViewMinYMargin];

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
  [[[self window] contentView] setSubviews:
      [NSArray arrayWithObject:contentView]];
}

@end  // WebIntentBubbleController
